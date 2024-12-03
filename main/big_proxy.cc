#include "big_proxy.hh"

using std::cout;
using std::cin;
using std::endl;
using std::string;

//global map, for each client, we have one WrapperState which contains ProxyState.

//Return values got by using directly the MySQL c Client
struct rawReturnValue{
    std::vector<std::vector<std::string> > rowValues;
    std::vector<std::string> fieldNames;
    std::vector<int> fieldTypes;
};

//must be static, or we get "no previous declaration"
//execute the query and getthe rawReturnVale, this struct can be copied.
static 
rawReturnValue executeAndGetResultRemote(Connect * curConn,std::string query){
    std::unique_ptr<DBResult> dbres;
    curConn->execute(query, &dbres);
    rawReturnValue myRaw;
    
    if(dbres==nullptr||dbres->n==NULL){
        LOG(debug)<<"no results"<<std::endl;
        return myRaw;
    }

    int num = mysql_num_rows(dbres->n);
    if(num!=0)
        LOG(debug) <<"num of rows: "<<num<<std::endl;

    int numOfFields = mysql_num_fields(dbres->n);
    if(numOfFields!=0)
        LOG(debug) <<"num of fields: "<<numOfFields<<std::endl;

    MYSQL_FIELD *field;
    MYSQL_ROW row;

    if(num!=0){
        while( (row = mysql_fetch_row(dbres->n)) ){
	    unsigned long * fieldLen = mysql_fetch_lengths(dbres->n);
            std::vector<std::string> curRow;
            for(int i=0;i<numOfFields;i++){
                if (i == 0) {
                    while( (field = mysql_fetch_field(dbres->n)) ) {
                        myRaw.fieldNames.push_back(std::string(field->name));
                        myRaw.fieldTypes.push_back(field->type);
                    }
                }
                if(row[i]==NULL) curRow.push_back("NULL");
                else curRow.push_back(std::string(row[i],fieldLen[i]));
            }
            myRaw.rowValues.push_back(curRow);
        }
    }
    return myRaw;
}

//print RawReturnValue for testing purposes.
static
void printrawReturnValue(rawReturnValue & cur) {
    int len = cur.fieldTypes.size();
    if(len==0){
        //LOG(debug)<<"zero output"<<std::endl;
        return ;
    }

    if(static_cast<int>(cur.fieldNames.size())!=len||static_cast<int>(cur.rowValues[0].size())!=len){
        LOG(debug)<<RED_BEGIN<<"size mismatch in printrawReturnValue"<<COLOR_END<<std::endl;
        return ;
    }

    for(int i=0;i<len;i++){
        LOG(debug)<<cur.fieldNames[i]<<":"<<cur.fieldTypes[i]<<"\t";
    }

    LOG(debug)<<std::endl;
    for(auto row:cur.rowValues){
        for(auto rowItem:row){
            LOG(debug)<<rowItem<<"\t";
        }
        LOG(debug)<<std::endl;
    }
}

//helper function for transforming the rawReturnValue
static Item_null *
make_null(const std::string &name = ""){
    // LOG(debug) << "-------- current_thd in big_proxy make_null =" << current_thd;
    char *const n = current_thd->strdup(name.c_str());
    return new Item_null(n);
}
//helper function for transforming the rawReturnValue
static std::vector<Item *>
itemNullVector(unsigned int count)
{
    std::vector<Item *> out;
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(make_null());
    }
    return out;
}

//transform rawReturnValue to ResType
static 
ResType MygetResTypeFromLuaTable(bool isNULL,rawReturnValue *inRow = NULL,int in_last_insert_id = 0){
    std::vector<std::string> names;
    std::vector<enum_field_types> types;
    std::vector<std::vector<Item *> > rows;
    //return NULL restype 
    if(isNULL){
        return ResType(true,0,0,std::move(names),
                      std::move(types),std::move(rows));
    } else {
        for(auto inNames:inRow->fieldNames){
            names.push_back(inNames);
        }
        for(auto inTypes:inRow->fieldTypes){
            types.push_back(static_cast<enum_field_types>(inTypes));
        }
        for(auto inRows:inRow->rowValues) {
            std::vector<Item *> curTempRow = itemNullVector(types.size());
            for(int i=0;i< (int)(inRows.size());i++){
                curTempRow[i] = (MySQLFieldTypeToItem(types[i],inRows[i]) );
            }
            rows.push_back(curTempRow);
        }
	    // LOG(debug)<<GREEN_BEGIN<<"Affected rows: "<<afrow<<COLOR_END<<std::endl;
        return ResType(true, 0 ,
                               in_last_insert_id, std::move(names),
                                   std::move(types), std::move(rows));
    }
}

//printResType for testing purposes
static 
void parseResType(const ResType &rd) {
    for(auto name:rd.names){
        LOG(debug)<<name<<"\t";
    }
    LOG(debug)<<std::endl;    
    for(auto row:rd.rows){
        for(auto item:row){
            LOG(debug)<<ItemToString(*item)<<"\t";
        }
            LOG(debug)<<std::endl;
    }
}

static pthread_mutex_t big_lock;
static SharedProxyState * shared_ps = NULL;
big_proxy::big_proxy(std::string db){

    assert(0 == mysql_thread_init());

    client="192.168.1.1:1234";
    //one Wrapper per user.
    clients[client] = new WrapperState();   
    LOG(debug)  << "new client " << clients[client] ;
    //Connect phase
    ConnectionInfo const ci("localhost", "root", "letmein",3306);
    //const std::string master_key = "113341234";
    const std::string master_key = "113341234";
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){  
        perror("getcwd error");  
    }
    embeddedDir = std::string(buffer)+"/shadow";
    free(buffer);

    if(!shared_ps){
        shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    }
        
    //we init embedded database here.
    clients[client]->ps = std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));

    clients[client]->ps->safeCreateEmbeddedTHD();
    LOG(debug) << ">>>>>>>>>>>>>>>>> call safeCreateEmbeddedTHD in big_proxy::big_proxy, current_thd = " << current_thd;
    //Connect end!!
    globalConn = new Connect(ci.server, ci.user, ci.passwd, ci.port);
    targetDb=db;
    curQuery = string("use ")+targetDb;
    _thread_id = globalConn->get_thread_id();
}

big_proxy::~big_proxy() {
    if (globalConn) {
        delete globalConn;
    }
    
    // 遍历 clients 容器中的每个元素，手动释放动态分配的内存
    for (auto& pair : clients) {
        delete pair.second;  // 释放 WrapperState 对象
    }
    clients.clear();  // 可选：清空容器
    
    if (shared_ps) {
        delete shared_ps;
    }
}


void big_proxy::myNext(std::string client,bool isFirst,ResType inRes) {
    assert(0 == mysql_thread_init());

    WrapperState *const c_wrapper = clients[client];
    ProxyState *const ps = c_wrapper->ps.get();
    assert(ps);
    
    ps->safeCreateEmbeddedTHD();
    // LOG(debug) << ">>>>>>>>>>>>>>>>> call safeCreateEmbeddedTHD in big_proxy::myNext, current_thd = " << current_thd;
   
    const ResType &res = inRes;
    const std::unique_ptr<QueryRewrite> &qr = c_wrapper->getQueryRewrite();

    try{
        NextParams nparams(*ps, c_wrapper->default_db, c_wrapper->last_query);
        c_wrapper->selfKill(KillZone::Where::Before);
        const auto &new_results = qr->executor->next(res, nparams);
        c_wrapper->selfKill(KillZone::Where::After);

        const auto &result_type = new_results.first;
        LOG(debug) << "execute the query, fetch the results, and call next again";
        
        if (result_type != AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN) {
            // set the killzone when we are done with this query
            // > a given killzone will only apply to the next query translation
            c_wrapper->setKillZone(qr->kill_zone);
        }

        switch (result_type){
            //execute the query, fetch the results, and call next again
        case AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN: {
            LOG(debug)<<RED_BEGIN<<"case one"<<COLOR_END<<std::endl;
            const auto &output =
                std::get<1>(new_results)->extract<std::pair<bool, std::string> >();
            const auto &next_query = output.second;
            //here we execute the query against the remote database, and get rawReturnValue
            rawReturnValue resRemote = executeAndGetResultRemote(globalConn,next_query);
            // printrawReturnValue(resRemote);
            //transform rawReturnValue first
            const auto &againGet = MygetResTypeFromLuaTable(false,&resRemote);            
            myNext(client,false,againGet);
            break;
        }

        //only execute the query, without processing the retults
        case AbstractQueryExecutor::ResultType::QUERY_USE_RESULTS:{
            const auto &new_query =
                std::get<1>(new_results)->extract<std::string>();
            auto resRemote = executeAndGetResultRemote(globalConn,new_query);
            printrawReturnValue(resRemote);
            break;
        }

        //return the results to the client directly 
        case AbstractQueryExecutor::ResultType::RESULTS:{
            const auto &res = new_results.second->extract<ResType>(); 
            parseResType(res);
            break;
        }

        default:{
            LOG(debug)<<"case default"<<std::endl;
        }
        }
        if (new_results.second) {
            delete new_results.second;
        }
    }catch(...){
        LOG(debug)<<"next error"<<std::endl;
    }
}


void big_proxy::batchTogether(std::string client, std::string curQuery,unsigned long long _thread_id) {
    //the first step is to Rewrite, we abort this session if we fail here.
    size_t before = getCurrentRSS();
    bool resMyRewrite =  myRewrite(curQuery,_thread_id,client);
    size_t after = getCurrentRSS();
    LOG(debug) << "Total memory: " << after << " bytes, Memory in myRewrite usage change : " << (after - before) << " bytes";
    if(!resMyRewrite){
         return ; 
    }
    before = getCurrentRSS();
    myNext(client,true, MygetResTypeFromLuaTable(true));
    after = getCurrentRSS();
    LOG(debug) << "Total memory: " << after << " bytes, Memory in myNext usage change : " << (after - before) << " bytes";
}


bool big_proxy::myRewrite(std::string curQuery,unsigned long long _thread_id,std::string client) {
    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

    WrapperState *const c_wrapper = clients[client];
    ProxyState *const ps = c_wrapper->ps.get();
    assert(ps);

    LOG(debug) << "---> query: "<< curQuery;
    c_wrapper->last_query = curQuery;
    try{
        TEST_Text(retrieveDefaultDatabase(_thread_id, ps->getConn(),
                                              &c_wrapper->default_db),
                                  "proxy failed to retrieve default database!");
        LOG(debug) <<"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^";
        const std::shared_ptr<const SchemaInfo> &schema =  ps->getSchemaInfo();
        c_wrapper->schema_info_refs.push_back(schema);

        ps->dumpTHDs();

        size_t before = getCurrentRSS();
        std::unique_ptr<QueryRewrite> qr =
            std::unique_ptr<QueryRewrite>(new QueryRewrite(
                    Rewriter::rewrite(curQuery, *schema.get(),
                                      c_wrapper->default_db, *ps)));
        assert(qr);
        size_t after = getCurrentRSS();
        LOG(debug) <<"====> Total, after new QueryRewrite, total memory: "<< after << " bytes, Memory usage change: " << (after - before) << " bytes";
        
        c_wrapper->setQueryRewrite(std::move(qr));
        
        LOG(debug) <<"vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv";
        }catch (const std::exception &e) {
            // 捕获标准库异常并输出异常信息
            LOG(debug) << "Rewrite exception: " << e.what() << std::endl;
            return false;
}       catch(...){
            LOG(debug) <<"rewrite exception!!!"<<std::endl;
            return false;
        }
        return true;
}

void big_proxy::go(std::string query){
        batchTogether(client,query,_thread_id);
}


