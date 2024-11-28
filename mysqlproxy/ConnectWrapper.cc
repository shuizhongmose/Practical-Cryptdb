#include <sstream>
#include <fstream>
#include <assert.h>
#include <lua5.1/lua.hpp>
#include <NTL/BasicThreadPool.h>
#include <stdlib.h>

#include <util/ctr.hh>
#include <util/cryptdb_log.hh>
#include <util/scoped_lock.hh>
#include <util/util.hh>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/schema.hh>
#include <main/Analysis.hh>

#include <parser/sql_utils.hh>
#include <parser/mysql_type_metadata.hh>

using namespace NTL;

//线程变量拷贝
__thread ProxyState *thread_ps = NULL;

class WrapperState {
    WrapperState(const WrapperState &other);
    WrapperState &operator=(const WrapperState &rhs);

    KillZone kill_zone;

public:
    std::string last_query;
    std::string default_db;
    std::ofstream * PLAIN_LOG;

    WrapperState() {}

    const std::unique_ptr<QueryRewrite> &getQueryRewrite() const {
        assert(this->qr);
        return this->qr;
    }
    void setQueryRewrite(std::unique_ptr<QueryRewrite> &&in_qr) {
        this->qr = std::move(in_qr);
    }
    void selfKill(KillZone::Where where) {
        kill_zone.die(where);
    }
    void setKillZone(const KillZone &kz) {
        kill_zone = kz;
    }

    std::unique_ptr<ProxyState> ps;
    // we are running cryptdb in a threaded environment without proper
    // locking; this leads to crashes during onion adjustment unless we
    // take some minimal precautions
    // > everytime we process a query we take a reference to the SchemaInfo
    //   so that we know the same SchemaInfo (and it's children) will be
    //   available on the backend for Deltaz; this handles the following
    //   known bad cases.
    //   a. thread A marks the cache as stale; thread B sees that it is
    //      stale and updates the cache; thread A crashes while
    //      trying to do onion adjustment
    //   b. we must take the reference at the same time we get the schema
    //      from the cache, otherwise ...
    //      + thread A takes the reference to SchemaInfo when the cache is
    //        already stale, now he ``gets'' his SchemaInfo; because the
    //        cache is stale the second SchemaInfo is a new object and the
    //        reference doesn't protect it. now when thread B gets his
    //        SchemaInfo the cache is still stale so he deletes the only
    //        reference to the SchemaInfo thread A is using
    //      + this case only applies if we aren't using the lock on each
    //        function (connect, disonnect, rewrite, envoi); thread A
    //        takes a reference to SchemaInfo then before he can ``get''
    //        the SchemaInfo thread B stales the cache. now thread A
    //        gets the SchemaInfo and his reference doesn't protect it.
    //        when thread C gets his SchemaInfo the cache is stale so
    //        he deletes the only reference to the SchemaInfo thread A
    //        is using
    std::vector<SchemaInfoRef> schema_info_refs;

private:
    std::unique_ptr<QueryRewrite> qr;
};

//commented
//static Timer t;

//static EDBProxy * cl = NULL;
static SharedProxyState * shared_ps = NULL;
static pthread_mutex_t big_lock;

static bool EXECUTE_QUERIES = true;

static std::string TRAIN_QUERY ="";

static bool LOG_PLAIN_QUERIES = false;
static std::string PLAIN_BASELOG = "";


static int counter = 0;

static std::map<std::string, WrapperState*> clients;

static void
returnResultSet(lua_State *L, const ResType &res);

static Item_null *
make_null(const std::string &name = "")
{
    // LOG(debug) << ">>>>>>>>>>> current_thd in ConnectWrapper.cc/make_null =" << current_thd;
    char *const n = current_thd->strdup(name.c_str());
    return new Item_null(n);
}

static std::string
xlua_tolstring(lua_State *const l, int index)
{
    size_t len;
    char const *const s = lua_tolstring(l, index, &len);
    return std::string(s, len);
}

static void
xlua_pushlstring(lua_State *const l, const std::string &s)
{
    lua_pushlstring(l, s.data(), s.length());
}

static int
connect(lua_State *const L) {
    // LOG(debug)  << ">>>>>>>>>>> current_thd in connect =" << current_thd;
    //TODO: added, why test here?
    assert(test64bitZZConversions());

    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

    //来自lua脚本的参数.
    const std::string client = xlua_tolstring(L, 1);
    const std::string server = xlua_tolstring(L, 2);
    const uint port = luaL_checkint(L, 3);
    const std::string user = xlua_tolstring(L, 4);
    const std::string psswd = xlua_tolstring(L, 5);
    const std::string embed_dir = xlua_tolstring(L, 6);

    ConnectionInfo const ci = ConnectionInfo(server, user, psswd, port);

    assert(clients.end() == clients.find(client));
    //一个用户进入的时候, 新建一个wrapper, 走的时候删除.
    clients[client] = new WrapperState();

    // Is it the first connection?
    if (!shared_ps) {
        std::cerr << "starting proxy\n";
        LOG(wrapper) << "connect " << client << "; "
                     << "server = " << server << ":" << port << "; "
                     << "user = " << user << "; "
                     << "password = " << psswd;
        
        // FIXME: First execute mysql_library_init
        // ERROR：Connect::do_connect中的mysql_library_init与Connect::getEmbedded中的不一样，如果用同一个就无法进行查询操作
        // init_mysql(embed_dir);
    
        const std::string &false_str = "FALSE";
        const std::string &mkey      = "113341234";  // XXX do not change as
                                                     // it's used for tpcc exps
		//const std::string &mkey = "887766908";
        shared_ps =
            new SharedProxyState(ci, embed_dir, mkey,
                                 determineSecurityRating());
        //may need to do training
        const char *ev = getenv("TRAIN_QUERY");
        if (ev) {
            std::cerr << "Deprecated query training!" << std::endl;
        }

        ev = getenv("EXECUTE_QUERIES");
        if (ev && equalsIgnoreCase(false_str, ev)) {
            LOG(wrapper) << "do not execute queries";
            EXECUTE_QUERIES = false;
        } else {
            LOG(wrapper) << "execute queries";
            EXECUTE_QUERIES = true;
        }

        ev = getenv("LOAD_ENC_TABLES");
        if (ev) {
            std::cerr << "No current functionality for loading tables\n";
        }

        ev = getenv("LOG_PLAIN_QUERIES");
        if (ev) {
            std::string logPlainQueries = std::string(ev);
            if (logPlainQueries != "") {
                LOG_PLAIN_QUERIES = true;
                PLAIN_BASELOG = logPlainQueries;
                logPlainQueries += StringFromVal(++counter);

                assert_s(system(("rm -f" + logPlainQueries + "; touch " + logPlainQueries).c_str()) >= 0, "failed to rm -f and touch " + logPlainQueries);

                std::ofstream * const PLAIN_LOG =
                    new std::ofstream(logPlainQueries, std::ios_base::app);
                LOG(wrapper) << "proxy logs plain queries at " << logPlainQueries;
                assert_s(PLAIN_LOG != NULL, "could not create file " + logPlainQueries);
                clients[client]->PLAIN_LOG = PLAIN_LOG;
            } else {
                LOG_PLAIN_QUERIES = false;
            }
        }
    } else {
        if (LOG_PLAIN_QUERIES) {
            std::string logPlainQueries =
                PLAIN_BASELOG+StringFromVal(++counter);
            assert_s(system((" touch " + logPlainQueries).c_str()) >= 0, "failed to remove or touch plain log");
            LOG(wrapper) << "proxy logs plain queries at " << logPlainQueries;

            std::ofstream * const PLAIN_LOG =
                new std::ofstream(logPlainQueries, std::ios_base::app);
            assert_s(PLAIN_LOG != NULL, "could not create file " + logPlainQueries);
            clients[client]->PLAIN_LOG = PLAIN_LOG;
        }
    }
    // We don't want to use the THD from the previous connection
    // if such is even possible...
    clients[client]->ps =
        std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    // LOG(debug) << ">>>>>>>>>>>>>>>>> call safeCreateEmbeddedTHD in mysqlproxy/ConnectWrapper.cc/connect()";
    clients[client]->ps->safeCreateEmbeddedTHD();

    return 0;
}

static int
disconnect(lua_State *const L) {
    // LOG(debug)  << ">>>>>>>>>>> current_thd in disconnect =" << current_thd;
    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        return 0;
    }
    // LOG(wrapper) << "disconnect " << client;

    auto ws = clients[client];
    delete ws;
    clients[client] = NULL;

    thread_ps = NULL;
    clients.erase(client);

    mysql_thread_end();
    return 0;
}

static int
rewrite(lua_State *const L) {
    // LOG(debug) << ">>>>>>>>>> current_thd in rewrite =" << current_thd;
    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

         
    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        lua_pushnil(L);
        xlua_pushlstring(L, "failed to recognize client");  
        mysql_thread_end();   
        return 2;
    }

    WrapperState *const c_wrapper = clients[client];
    ProxyState *const ps = thread_ps = c_wrapper->ps.get();
    assert(ps);

    const std::string &query = xlua_tolstring(L, 2);
    const unsigned long long _thread_id =
        strtoull(xlua_tolstring(L, 3).c_str(), NULL, 10);
    //std::cout<<query<<std::endl;
    //this is not used??
    c_wrapper->last_query = query;
    if (EXECUTE_QUERIES) {
        try {
            TEST_Text(retrieveDefaultDatabase(_thread_id, ps->getConn(),
                                              &c_wrapper->default_db),
                      "proxy failed to retrieve default database!");
            // save a reference so a second thread won't eat objects
            // that DeltaOuput wants later
            const std::shared_ptr<const SchemaInfo> &schema =
                ps->getSchemaInfo();
            c_wrapper->schema_info_refs.push_back(schema);

            // ADD: 在retrieveDefaultDatabase中创建的线程没什么用，可以先清除掉？？
            ps->dumpTHDs();

            //parse, rewrite, delta, adjust, returnMeta, 
            // 实现rewrite操作
            // std::unique_ptr 管理内存，会自动回收，所以不需要再进行回收
            // LOG(debug) << "begin rewrite, current_thd=" << current_thd;
            std::unique_ptr<QueryRewrite> qr =
                std::unique_ptr<QueryRewrite>(new QueryRewrite(
                    Rewriter::rewrite(query, *schema.get(),
                                      c_wrapper->default_db, *ps)));
            // LOG(debug) << "done rewrite, current_thd=" << current_thd;
            assert(qr);
            c_wrapper->setQueryRewrite(std::move(qr));
        } catch (const AbstractException &e) {
            lua_pushboolean(L, false);              // status
            xlua_pushlstring(L, e.to_string());     // error message
            mysql_thread_end();
            return 2;
        } catch (const CryptDBError &e) {
            lua_pushboolean(L, false);              // status
            xlua_pushlstring(L, e.msg);             // error message
            mysql_thread_end();
            return 2;
        }
    }
    lua_pushboolean(L, true);                       // status
    lua_pushnil(L);                                 // error message
    mysql_thread_end();
    return 2;
}

inline std::vector<Item *>
itemNullVector(unsigned int count)
{
    std::vector<Item *> out;
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(make_null());
    }

    return out;
}

struct rawReturnValue{
    std::vector<std::vector<std::string> > rowValues;
    std::vector<std::string> fieldNames;
    std::vector<int> fieldTypes;
};

static ResType
getResTypeFromLuaTable(lua_State *const L, int fields_index,
                       int rows_index, int affected_rows_index,
                       int insert_id_index, int status_index) {
    const bool status = lua_toboolean(L, status_index);
    if (false == status) {
        return ResType(false, 0, 0);
    }

    rawReturnValue myRawFromLua;

    std::vector<std::string> names;
    std::vector<enum_field_types> types;
    /* iterate over the fields argument */
    lua_pushnil(L);
    while (lua_next(L, fields_index)) {
        if (!lua_istable(L, -1))
            LOG(warn) << "mismatch";
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const std::string k = xlua_tolstring(L, -2);
            if ("name" == k) {
                names.push_back(xlua_tolstring(L, -1));
                myRawFromLua.fieldNames.push_back(xlua_tolstring(L, -1));
            } else if ("type" == k) {
                types.push_back(static_cast<enum_field_types>(luaL_checkint(L, -1)));
                myRawFromLua.fieldTypes.push_back(static_cast<enum_field_types>(luaL_checkint(L, -1)) );
            } else {
                LOG(warn) << "unknown key " << k;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    assert(names.size() == types.size());

    /* iterate over the rows argument */
    std::vector<std::vector<Item *> > rows;
    lua_pushnil(L);

    //没有kv对了, 则退出.
    while (lua_next(L, rows_index)) {
        if (!lua_istable(L, -1))
            LOG(warn) << "mismatch";

        /* initialize all items to NULL, since Lua skips
           nil array entries */
        std::vector<Item *> row = itemNullVector(types.size());
        std::vector<std::string> curRow;
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const int key = luaL_checkint(L, -2) - 1;
            assert(key >= 0 && static_cast<uint>(key) < types.size());
            const std::string data = xlua_tolstring(L, -1);
            curRow.push_back(data);
            row[key] = MySQLFieldTypeToItem(types[key], data);
            lua_pop(L, 1);
        }
        rows.push_back(row);
        myRawFromLua.rowValues.push_back(curRow);
        lua_pop(L, 1);

    }
    //printrawReturnValue(myRawFromLua);

    return ResType(status, lua_tointeger(L, affected_rows_index),
                   lua_tointeger(L, insert_id_index), std::move(names),
                   std::move(types), std::move(rows));
}

static void
nilBuffer(lua_State *const L, size_t count)
{
    while (count--) {
        lua_pushnil(L);
    }

    return;
}

/*
 *return mete for dectypting data.
 * */
static void 
parseReturnMeta(const ReturnMeta & rtm){
}


static int
next(lua_State *const L) {
    // LOG(debug) << "++++++++ current_thd in next =" << current_thd;
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());
    //查找client
    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        xlua_pushlstring(L, "error");
        xlua_pushlstring(L, "unknown client");
         lua_pushinteger(L,  100);
        xlua_pushlstring(L, "12345");

        nilBuffer(L, 1);
        mysql_thread_end();
        return 5;
    }

    WrapperState *const c_wrapper = clients[client];

    assert(EXECUTE_QUERIES);
    ProxyState *const ps = thread_ps = c_wrapper->ps.get();
    assert(ps);
    // LOG(debug) << ">>>>>>>>>>>>>>>>> call safeCreateEmbeddedTHD in mysqlproxy/ConnectWrapper.cc/next()";
    ps->safeCreateEmbeddedTHD();

    const ResType &res = getResTypeFromLuaTable(L, 2, 3, 4, 5, 6);


    const std::unique_ptr<QueryRewrite> &qr = c_wrapper->getQueryRewrite();
    // TODO？？
    parseReturnMeta(qr->rmeta);
    try {
        NextParams nparams(*ps, c_wrapper->default_db, c_wrapper->last_query);

        c_wrapper->selfKill(KillZone::Where::Before);
        // 执行next操作
        // 执行加密sql/或者解析查询结果等
        // LOG(debug) << "begin next, current_thd =" << current_thd;
        const auto &new_results = qr->executor->next(res, nparams);
        // LOG(debug) << "done next, current_thd =" << current_thd;
        c_wrapper->selfKill(KillZone::Where::After);

        const auto &result_type = new_results.first;
        if (result_type != AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN) {
            // set the killzone when we are done with this query
            // > a given killzone will only apply to the next query translation
            c_wrapper->setKillZone(qr->kill_zone);
        }
        switch (result_type) {
        case AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN: {
            // LOG(debug) << "return again, current_thd =" << current_thd;
            // 返回sql语句给lua脚本，执行结果再次进入该函数处理
            // more to do before we have the client's results
            xlua_pushlstring(L, "again");
            const auto &output =
                std::get<1>(new_results)->extract<std::pair<bool, std::string> >();
            // output.first=true
            const auto &want_interim = output.first;
            lua_pushboolean(L, want_interim);
            // output.second=string(query)，加密SQL
            const auto &next_query = output.second;
            xlua_pushlstring(L, next_query);
            nilBuffer(L, 2);
            return 5;
        }
        case AbstractQueryExecutor::ResultType::QUERY_USE_RESULTS: {
            // LOG(debug) << "return query_results, current_thd =" << current_thd;
            // 返回sql语句给lua脚本，执行结果直接返回客户端
            // the results of executing this query should be send directly
            // back to the client
            xlua_pushlstring(L, "query-results");
            const auto &new_query =
                std::get<1>(new_results)->extract<std::string>();
            xlua_pushlstring(L, new_query);
            nilBuffer(L, 3);
            // ADD: 返回结果后清空相关线程信息
            ps->dumpTHDs();
            mysql_thread_end();
            
            return 5;
        }
        case AbstractQueryExecutor::ResultType::RESULTS: {
            // LOG(debug) << "return results, current_thd =" << current_thd;
            // 返回解密结果给lua脚本，并由mysql-proxy返回给客户端
            // ready to return results to the client
            xlua_pushlstring(L, "results");
            const auto &res = new_results.second->extract<ResType>();
            returnResultSet(L, res);        // pushes 4 items on stack
            // ADD: 返回结果后清空相关线程信息
            ps->dumpTHDs();
            mysql_thread_end();
            return 5;
        }
        default:
            // ADD: 不是任何类型，代表为空是不是也可以清空线程空间？
            ps->dumpTHDs();
            mysql_thread_end();
            assert(false);
        }
        
        if (new_results.second) {
            delete new_results.second;
        }
        
    } catch (const ErrorPacketException &e) {
        // lua_pop(L, lua_gettop(L));
        xlua_pushlstring(L, "error");
        xlua_pushlstring(L, e.getMessage());
         lua_pushinteger(L, e.getErrorCode());
        xlua_pushlstring(L, e.getSQLState());

        nilBuffer(L, 1);
        // ADD: 错误清空相关线程信息
        ps->dumpTHDs();
        mysql_thread_end();
        return 5;
    }
    ps->dumpTHDs();
    mysql_thread_end();
    return 5;
}

static void
returnResultSet(lua_State *const L, const ResType &rd)
{
    TEST_GenericPacketException(true == rd.ok, "something bad happened");

    lua_pushinteger(L, rd.affected_rows);
    lua_pushinteger(L, rd.insert_id);

    /* return decrypted result set */
    lua_createtable(L, (int)rd.names.size(), 0);
    int const t_fields = lua_gettop(L);

    for (uint i = 0; i < rd.names.size(); i++) {
        lua_createtable(L, 0, 1);
        int const t_field = lua_gettop(L);

        /* set name for field */
        xlua_pushlstring(L, rd.names[i]);       // plaintext fields
        lua_setfield(L, t_field, "name");
        /* insert field element into fields table at i+1 */
        lua_rawseti(L, t_fields, i+1);
    }

    lua_createtable(L, static_cast<int>(rd.rows.size()), 0);
    int const t_rows = lua_gettop(L);
    for (uint i = 0; i < rd.rows.size(); i++) {
        lua_createtable(L, static_cast<int>(rd.rows[i].size()), 0);
        int const t_row = lua_gettop(L);

        for (uint j = 0; j < rd.rows[i].size(); j++) {
            if (NULL == rd.rows[i][j]) {
                lua_pushnil(L);                 // plaintext rows
            } else {
                xlua_pushlstring(L,             // plaintext rows
                                 ItemToString(*rd.rows[i][j]));
            }
            lua_rawseti(L, t_row, j+1);
        }

        lua_rawseti(L, t_rows, i+1);
    }

    return;
}

static const struct luaL_reg
cryptdb_lib[] = {
#define F(n) { #n, n }
    F(connect),
    F(disconnect),
    F(rewrite),
    F(next),
    { 0, 0 },
};


void cleanupFunction() {
    // clear_embeddedmysql();
    Rewriter::cleanup();
}

extern "C" int lua_cryptdb_init(lua_State * L);

int
lua_cryptdb_init(lua_State *const L) {
    // SetNumThreads(AvailableThreads());
    SetNumThreads(1);
    if (atexit(cleanupFunction) != 0) {
        LOG(error) << "无法注册清理函数";
        return 0;
    }
    luaL_openlib(L, "CryptDB", cryptdb_lib, 0);
    return 1;
}
