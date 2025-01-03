#include <csignal>

#include <main/ddl_handler.hh>
#include <main/rewrite_util.hh>
#include <main/rewrite_main.hh>
#include <main/alter_sub_handler.hh>
#include <main/dispatcher.hh>
#include <main/macro_util.hh>
#include <main/metadata_tables.hh>
#include <parser/lex_util.hh>

#include <util/yield.hpp>


//a list of ddl handlers, buildddlhandler, and ddlexecutor

//################################################################Create table handler#########################################################################################
AbstractQueryExecutor *
        CreateTableHandler::rewriteAndUpdate(Analysis &a, LEX *lex, const Preamble &pre) const
    {
        assert(a.deltas.size() == 0);

        TEST_DatabaseDiscrepancy(pre.dbname, a.getDatabaseName());
        LEX *const new_lex = copyWithTHD(lex);

        TEST_Text(DB_TYPE_INNODB == lex->create_info.db_type->db_type,
                  "InnoDB is the only supported ENGINE")

        // partitioning information is not currently printed by our LEX
        // stringifications algorithms which leads to a truncated query
        TEST_Text(NULL == lex->part_info,
                  "CryptDB does not support partitioning");

        //TODO: support for "create table like"
        TEST_Text(
                !(lex->create_info.options & HA_LEX_CREATE_TABLE_LIKE),
                "No support for create table like yet. "
                "If you see this, please implement me");

        // Create the table regardless of 'IF NOT EXISTS' if the table
        // doesn't exist.
        if (false == a.tableMetaExists(pre.dbname, pre.table)) {
            // TODO: Use appropriate values for has_sensitive and has_salt.
            std::unique_ptr<TableMeta> tm(new TableMeta(true, true));

            // -----------------------------
            //         Rewrite TABLE
            // -----------------------------
            // HACK.
            // > We know that there is only one table.
            // > We do not currently support CREATE + SELECT syntax
            //   ! CREATE TABLE t2 SELECT * FROM t1;
            // > We also know that Analysis does not have a reference to
            //   the table as it depends on SchemaInfo.
            // > And we know that the table we want is tm with name table.
            // > This will _NOT_ gracefully handle a malformed CREATE TABLE
            // query.
            TEST_Text(1 == new_lex->select_lex.table_list.elements,
                      "we do not support multiple tables in a CREATE"
                      " TABLE queries");
            // Take the table name straight from 'tm' as
            // Analysis::getAnonTableName relies on SchemaInfo.
            TABLE_LIST *const tbl =
                rewrite_table_list(new_lex->select_lex.table_list.first,
                                   tm->getAnonTableName());
            //new table_list only contain one element
            new_lex->select_lex.table_list =
                *oneElemListWithTHD<TABLE_LIST>(tbl);

            // collect the keys (and their types) as they may affect the onion
            // layout we use
            const auto &key_data = collectKeyData(*lex);

            //这里给出了create的获取. alter_info代码被摘出来了. 代表了要创建的一系列column以及index
            auto it =
                List_iterator<Create_field>(lex->alter_info.create_list);

	        //对现有的每个field, 如id,name, 都在内部通过createAndRewriteField函数扩展成多个洋葱+salt.
	        //其中洋葱有多个层, 其通过newCreateField函数, 决定了类型, 而新的field的名字, 就是洋葱的名字传过去的.
            //扩展以后, 就是新的Create_field类型了, 这里返回的list是被继续传到引用参数里面的, 很奇怪的用法.
            //key data在这里的作用是, 决定是不是unique, 从而选择和是的洋葱层次.
            new_lex->alter_info.create_list =
                accumList<Create_field>(it,
                    [&a, &tm, &key_data] (List<Create_field> out_list,
                                          Create_field *const cf) {
                        return createAndRewriteField(a, cf, tm.get(),
                                                     true, key_data, out_list);
                });
            // -----------------------------
            //         Rewrite INDEX
            // -----------------------------
            highLevelRewriteKey(*tm.get(), *lex, new_lex, a);
            highLevelRewriteForeignKey(*tm.get(), *lex, new_lex, a,pre.table);
            // -----------------------------
            //         Update TABLE
            // -----------------------------
            //建立了db=>table的关系, 作为delta实现. 然后delta会apply. 这里使用了createDelta
            a.deltas.push_back(std::unique_ptr<Delta>(
                            new CreateDelta(std::move(tm),
                                            a.getDatabaseMeta(pre.dbname),
                                            IdentityMetaKey(pre.table))));
        } else { // Table already exists.
            // already exists.
            const bool test =
                lex->create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS;
            TEST_TextMessageError(test,
                                "Table " + pre.table + " already exists!");
            //why still rewrite here???
        }
	    //在handler的第一阶段, 通过analysis搜集delta以及执行计划等内容, 然后在第二阶段, 实行delta以及
        //执行计划, 新的lex里面包含了改写以后的语句, 直接转化成string就可以用了.
        return new DDLQueryExecutor(*new_lex, std::move(a.deltas));
    }





//################################################################Alter table handler#########################################################################################
// mysql does not support indiscriminate add-drops
// ie,
//      mysql> create table pk (x integer);
//      Query OK, 0 rows affected (0.09 sec)
//
//      mysql> alter table pk drop column x, add column x integer,
//                            drop column x;
//      ERROR 1091 (42000): Can't DROP 'x'; check that column/key exists
//
//      mysql> alter table pk drop column x, add column x integer;
//      Query OK, 0 rows affected (0.03 sec)
//      Records: 0  Duplicates: 0  Warnings: 0
class AlterTableHandler : public DDLHandler {
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *lex, const Preamble &pre) const
    {
        assert(a.deltas.size() == 0);

        TEST_Text(sub_dispatcher->canDo(lex),
                  "your ALTER TABLE query may require at least one"
                  " unsupported feature");
        const std::vector<AlterSubHandler *> &handlers =
            sub_dispatcher->dispatch(lex);
        assert(handlers.size() > 0);

        LEX *new_lex = copyWithTHD(lex);

        for (auto it : handlers) {
            new_lex = it->transformLex(a, new_lex);
        }

        // -----------------------------
        //         Rewrite TABLE
        // -----------------------------
        // > Rewrite after doing the transformations as the handlers
        // expect the original table name to be intact.
        new_lex->select_lex.table_list =
            rewrite_table_list(new_lex->select_lex.table_list, a, true);

        return new DDLQueryExecutor(*new_lex, std::move(a.deltas));
    }

    const std::unique_ptr<AlterDispatcher> sub_dispatcher;

public:
    AlterTableHandler() : sub_dispatcher(buildAlterSubDispatcher()) {}
};



//################################################################drop table handler#########################################################################################


class DropTableHandler : public DDLHandler {
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *lex, const Preamble &pre) const
    {
        assert(a.deltas.size() == 0);

        LEX *const final_lex = rewrite(a, lex);
        update(a, lex);

        return new DDLQueryExecutor(*final_lex, std::move(a.deltas));
    }
    
    LEX *rewrite(Analysis &a, LEX *lex) const
    {
        LEX *const new_lex = copyWithTHD(lex);
        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a, true);

        return new_lex;
    }

    void update(Analysis &a, LEX *lex) const
    {
        TABLE_LIST *tbl = lex->select_lex.table_list.first;
        for (; tbl; tbl = tbl->next_local) {
            char* table  = tbl->table_name;

            TEST_DatabaseDiscrepancy(tbl->db, a.getDatabaseName());
            if (lex->drop_if_exists) {
                if (false == a.tableMetaExists(tbl->db, table)) {
                    continue;
                }
            }

            // Remove from *Meta structures.
            TableMeta const &tm = a.getTableMeta(tbl->db, table);
            a.deltas.push_back(std::unique_ptr<Delta>(
                            new DeleteDelta(tm,
                                            a.getDatabaseMeta(tbl->db))));
        }
    }
};


//################################################################Create db handler#########################################################################################

class CreateDBHandler : public DDLHandler {
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *const lex, const Preamble &pre)
            const
    {
        assert(a.deltas.size() == 0);

        const std::string &dbname =
            convert_lex_str(lex->name);
        if (false == a.databaseMetaExists(dbname)) {
            std::unique_ptr<DatabaseMeta> dm(new DatabaseMeta());
            //可以看到, 建立数据库的时候,和建立表的时候类型, 使用了createdelta, 添加了从db到schema的映射过程.
            a.deltas.push_back(std::unique_ptr<Delta>(
                        new CreateDelta(std::move(dm), a.getSchema(),
                                        IdentityMetaKey(dbname))));
        } else {
           
            const bool test =
                lex->create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS;
            TEST_TextMessageError(test,
                                "Database " + dbname + " already exists!");
        }
        return new DDLQueryExecutor(*copyWithTHD(lex), std::move(a.deltas));
    }
};


//################################################################change db handler#########################################################################################

class ChangeDBHandler : public DDLHandler {
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *const lex, const Preamble &pre)
            const
    {
        assert(a.deltas.size() == 0);
        return new SimpleExecutor();
    }
};


//################################################################drop db handler#########################################################################################
class DropDBHandler : public DDLHandler {
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *const lex, const Preamble &pre)
            const
    {
        assert(a.deltas.size() == 0);

        const std::string &dbname = convert_lex_str(lex->name);
        const DatabaseMeta &dm = a.getDatabaseMeta(dbname);
        a.deltas.push_back(std::unique_ptr<Delta>(
                                    new DeleteDelta(dm, a.getSchema())));

        return new DDLQueryExecutor(*copyWithTHD(lex), std::move(a.deltas));
    }
};


//################################################################lock table handler#########################################################################################
class LockTablesHandler : public DDLHandler {
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *const lex, const Preamble &pre)
            const
    {
        assert(a.deltas.size() == 0);

        LEX *const new_lex = copyWithTHD(lex);
        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a);

        return new DDLQueryExecutor(*new_lex, std::move(a.deltas));
    }
};


//################################################################Create index handler#########################################################################################
class CreateIndexHandler : public DDLHandler {
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *const lex, const Preamble &pre)
            const
    {
        assert(a.deltas.size() == 0);

        LEX *const new_lex = copyWithTHD(lex);

        // rewrite table
        new_lex->select_lex.table_list =
            rewrite_table_list(lex->select_lex.table_list, a);

        TEST_DatabaseDiscrepancy(pre.dbname, a.getDatabaseName());
        TableMeta const &tm = a.getTableMeta(pre.dbname, pre.table);

        highLevelRewriteKey(tm, *lex, new_lex, a);

        return new DDLQueryExecutor(*new_lex, std::move(a.deltas));
    }
};


static std::string
empty_if_null(const char *const p)
{
    if (p) return std::string(p);

    return std::string("");
}

AbstractQueryExecutor *DDLHandler::
transformLex(Analysis &a, LEX *lex) const
{

    assert(a.deltas.size() == 0);

    AssignOnce<std::string> db;
    AssignOnce<std::string> table;

    if (lex->select_lex.table_list.first) {
        db = empty_if_null(lex->select_lex.table_list.first->db);
        table =
            empty_if_null(lex->select_lex.table_list.first->table_name);
    } else {
        db =  "", table = "";
    }
    auto executor =
        this->rewriteAndUpdate(a, lex, Preamble(db.get(), table.get()));
    assert(a.deltas.size() == 0);

    return executor;
}

// FIXME: Add test to make sure handler added successfully.
SQLDispatcher *buildDDLDispatcher()
{
    DDLHandler *h;
    SQLDispatcher *dispatcher = new SQLDispatcher();

    h = new CreateTableHandler();
    dispatcher->addHandler(SQLCOM_CREATE_TABLE, h);

    h = new AlterTableHandler();
    dispatcher->addHandler(SQLCOM_ALTER_TABLE, h);

    h = new DropTableHandler();
    dispatcher->addHandler(SQLCOM_DROP_TABLE, h);

    h = new CreateDBHandler();
    dispatcher->addHandler(SQLCOM_CREATE_DB, h);

    h = new ChangeDBHandler();
    dispatcher->addHandler(SQLCOM_CHANGE_DB, h);

    h = new DropDBHandler();
    dispatcher->addHandler(SQLCOM_DROP_DB, h);

    h = new LockTablesHandler();
    dispatcher->addHandler(SQLCOM_LOCK_TABLES, h);

    h = new CreateIndexHandler();
    dispatcher->addHandler(SQLCOM_CREATE_INDEX, h);

    return dispatcher;
}

std::pair<AbstractQueryExecutor::ResultType, AbstractAnything *>
DDLQueryExecutor::
nextImpl(const ResType &res, const NextParams &nparams)
{
    reenter(this->corot) {
        yield {
            {
                uint64_t embedded_completion_id;
                TEST_ErrPkt(
                    deltaOutputBeforeQuery(nparams.ps.getEConn(),
                                           nparams.original_query,
                                           this->new_query, this->deltas,
                                           CompletionType::DDL,
                                           &embedded_completion_id),
                    "deltaOutputBeforeQuery failed for DDL");
                this->embedded_completion_id = embedded_completion_id;
            }
            return CR_QUERY_AGAIN(this->new_query);
        }
        TEST_ErrPkt(res.success(), "DDL query failed");
        // save the results so we can return them to the client
        this->ddl_res = res;

        yield {
            return CR_QUERY_AGAIN(
                " INSERT INTO " + MetaData::Table::remoteQueryCompletion() +
                "   (embedded_completion_id, completion_type) VALUES"
                "   (" + std::to_string(this->embedded_completion_id.get()) + ","
                "    '"+TypeText<CompletionType>::toText(CompletionType::Onion)+"'"
                "   );");
        }

        TEST_ErrPkt(res.success(), "failed issuing ddl completion");

        // execute the original query against the embedded database
        // > this is a ddl query so do not put it into a transaction
        /**
         * 这样的设计选择是基于DDL操作的性质及其对数据库结构直接影响的考虑。
         * 这种做法有利于简化操作流程，减少性能开销，并且对于DDL操作来说，通常是合理的，
         * 因为一旦决定修改数据库结构，这些更改往往是需要立即应用并且不可逆的。
         */
        TEST_ErrPkt(nparams.ps.getEConn()->execute(nparams.original_query),
                   "Failed to execute DDL query against embedded database!");

        TEST_ErrPkt(deltaOutputAfterQuery(nparams.ps.getEConn(), this->deltas,
                                          this->embedded_completion_id.get()),
                   "deltaOuputAfterQuery failed for DDL");
        yield return CR_RESULTS(this->ddl_res.get());
    }

    assert(false);
}

