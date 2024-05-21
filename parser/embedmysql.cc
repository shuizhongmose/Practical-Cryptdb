#include <assert.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#include <parser/embedmysql.hh>
#include <sql_base.h>
#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>
#include <sql_parse.h>
#include <handler.h>


#include <parser/stringify.hh>

#include <util/errstream.hh>
#include <util/rob.hh>

using namespace std;

extern "C" void *create_embedded_thd(int client_flag);

void
query_parse::cleanup() {
    if (t) {
        // LOG(debug) << "-------- close current_thd (" << current_thd << ") in query_parse::cleanup";

        // 获取并清理LEX对象
        LEX *lex = t->lex;
        if (lex) {
            lex->unit.cleanup();  // 清理LEX单位
            lex->destroy_query_tables_list();  // 销毁查询表列表
        }

        // 清理语句和查询后的内容
        t->end_statement();
        t->cleanup_after_query();

        // 关闭线程表
        close_thread_tables(t);

        // 释放事务锁
        t->mdl_context.release_transactional_locks();

        // 结束MySQL线程
        mysql_thread_end();

        // 删除THD对象
        delete t;
        t = nullptr;  // 设置指针为nullptr防止悬空指针

        // 减少线程计数
        --thread_count;
    }
}

query_parse::~query_parse() {
    cleanup();
}

LEX *
query_parse::lex()
{
    return t->lex;
}

/*
 * For the whys and hows;
 * look at plugin_thdvar_init(...) in sql/sql_plugin.cc, there we see the THD
 * table_plugin (the default plugin for queries run on a given THD) is taken
 * from the global_system_variables.table_plugin which can presumably be
 * toggled with the 'storage_engine' runtime variable (testing this possiblity
 * in cryptDB however shows that setting the runtime variable doesn't
 * propagate the change to the global_system_variables.table_plugin pointer);
 * this variable is initially set to the MyISAM engine
 *
 * since doing "SET storage_engine='InnoDB'" doesn't work; we need to manually
 * hack the correct default table_plugin into the THD
 *
 * continuing, we can see sql/sql_yacc.yy where
 *   lex->create_info.db_type = ha_default_handlerton(thd);
 * ha_default_handlerton(...) is in sql/handler.cc and it calls
 * ha_default_plugin(...). this function returns thd->variables.table_plugin.
 *
 * so if we change thd->variables.table_plugin to InnoDB, it will become our
 * default storage engine
 */
static plugin_ref
getInnoDBPlugin()
{
    static const LEX_STRING name = string_to_lex_str("InnoDB");
    // singleton
    static plugin_ref innodb_plugin = ha_resolve_by_name(0, &name);
    assert(innodb_plugin);

    return innodb_plugin;
}

query_parse::query_parse(const std::string &db, const std::string &q)
{
    assert(create_embedded_thd(0));
    //类内自带的THD* t结构.
    t = current_thd;
    // LOG(debug) << "-------- current_thd in query_parse::query_parse =" << current_thd;;
    assert(t != NULL);

    //if first word of query is CRYPTDB, we can't use the embedded db
    //  set annotation to true and return

    if (strncmp(toLowerCase(q).c_str(), "cryptdb", 7) == 0) {
        //do not use Annotation now
        return;
    }
    try {
        //set db
        t->set_db(db.data(), db.length());
        //reset tdb, what does cur_thd conrespond to??
        mysql_reset_thd_for_next_command(t);
        t->stmt_arena->state = Query_arena::STMT_INITIALIZED;

        // default engine should always be InnoDB
        t->variables.table_plugin = getInnoDBPlugin();
        assert(t->variables.table_plugin);

        char buf[q.size() + 1];
        memcpy(buf, q.c_str(), q.size());
        buf[q.size()] = '\0';
        size_t len = q.size();
        // 为解析SQL分配足够的内存
        alloc_query(t, buf, len + 1);

        // 为 SQL 语句的解析准备环境和上下文
        if (ps.init(t, buf, len))
            throw CryptDBError("Parser_state::init");

//  This is a wrapper of MYSQLparse(). All the code should call parse_sql()
//  instead of MYSQLparse().
        // SQL转化为lex格式
        if (parse_sql(t, &ps, 0))
            throw CryptDBError("parse_sql");

        LEX *lex = t->lex;

        switch (lex->sql_command) {
        case SQLCOM_SHOW_DATABASES:
        case SQLCOM_SHOW_TABLES:
        case SQLCOM_SHOW_FIELDS:
        case SQLCOM_SHOW_KEYS:
        case SQLCOM_SHOW_VARIABLES:
        case SQLCOM_SHOW_STATUS:
        case SQLCOM_SHOW_ENGINE_LOGS:
        case SQLCOM_SHOW_ENGINE_STATUS:
        case SQLCOM_SHOW_ENGINE_MUTEX:
        case SQLCOM_SHOW_STORAGE_ENGINES:
        case SQLCOM_SHOW_PROCESSLIST:
        case SQLCOM_SHOW_MASTER_STAT:
        case SQLCOM_SHOW_SLAVE_STAT:
        case SQLCOM_SHOW_GRANTS:
        case SQLCOM_SHOW_CREATE:
        case SQLCOM_SHOW_CHARSETS:
        case SQLCOM_SHOW_COLLATIONS:
        case SQLCOM_SHOW_CREATE_DB:
        case SQLCOM_SHOW_TABLE_STATUS:
        case SQLCOM_SHOW_TRIGGERS:
        case SQLCOM_LOAD:
        case SQLCOM_SET_OPTION:
        case SQLCOM_LOCK_TABLES:
        case SQLCOM_UNLOCK_TABLES:
        case SQLCOM_GRANT:
        case SQLCOM_CHANGE_DB:
        case SQLCOM_CREATE_DB:
        case SQLCOM_DROP_DB:
        case SQLCOM_ALTER_DB:
        case SQLCOM_REPAIR:
        case SQLCOM_ROLLBACK:
        case SQLCOM_ROLLBACK_TO_SAVEPOINT:
        case SQLCOM_COMMIT:
        case SQLCOM_SAVEPOINT:
        case SQLCOM_RELEASE_SAVEPOINT:
        case SQLCOM_SLAVE_START:
        case SQLCOM_SLAVE_STOP:
        case SQLCOM_BEGIN:
        case SQLCOM_CREATE_TABLE:
        case SQLCOM_CREATE_INDEX:
        case SQLCOM_ALTER_TABLE:
        case SQLCOM_DROP_TABLE:
        case SQLCOM_DROP_INDEX:
            return;
        case SQLCOM_INSERT:
        case SQLCOM_REPLACE:
        case SQLCOM_DELETE:
            if (string(lex->select_lex.table_list.first->table_name).substr(0, PWD_TABLE_PREFIX.length()) == PWD_TABLE_PREFIX) {
                return;
            }
        default:
            break;
        }

        /*
         * Helpful in understanding what's going on: JOIN::prepare(),
         * handle_select(), and mysql_select() in sql_select.cc.  Also
         * initial code in mysql_execute_command() in sql_parse.cc.
         */

        // 此函数的主要作用是在 SQL 查询的 FROM 子句中解析和验证指定的表名和别名
        // 确保它们在查询的当前上下文中是有效的，并且能够被正确引用。
        lex->select_lex.context.resolve_in_table_list_only(
            lex->select_lex.table_list.first);

        // 用于处理 SQL 查询中的派生表（也称为子查询或内联视图）。
        // 派生表是从 SELECT 语句生成的临时表，通常在 FROM 子句中使用
        if (t->fill_derived_tables())
            throw CryptDBError("fill_derived_tables");

        // 打开（即准备访问）查询中引用的普通表和派生表（子查询生成的临时表或视图）
        // 确保了在执行查询前，所有必要的表都已经被识别、准备好，并且可用
        if (open_normal_and_derived_tables(t, lex->query_tables, 0))
            throw CryptDBError("open_normal_and_derived_tables");

        if (SQLCOM_SELECT == lex->sql_command) {
            if (!lex->select_lex.master_unit()->is_union() &&
                !lex->select_lex.master_unit()->fake_select_lex)
            {
                // 检查是否是简单的 SELECT 查询（非 UNION 和非 fake_select_lex）。
                // 为这种查询创建一个 JOIN 对象，并准备查询
                // 如果是 UNION 查询或存在 fake_select_lex，则抛出异常或执行另一种处理流程
                JOIN *j = new JOIN(t, lex->select_lex.item_list,
                                   lex->select_lex.options, 0);
                if (j->prepare(&lex->select_lex.ref_pointer_array,
                               lex->select_lex.table_list.first,
                               lex->select_lex.with_wild,
                               lex->select_lex.where,
                               lex->select_lex.order_list.elements
                                   + lex->select_lex.group_list.elements,
                               lex->select_lex.order_list.first,
                               lex->select_lex.group_list.first,
                               lex->select_lex.having,
                               lex->proc_list.first,
                               &lex->select_lex,
                               &lex->unit))
                    throw CryptDBError("JOIN::prepare");
                delete j;
                j = nullptr;
            } else {
                thrower() << "skip unions for now (union="
                          << lex->select_lex.master_unit()->is_union()
                          << ", fake_select_lex="
                          << lex->select_lex.master_unit()->fake_select_lex
                          << ")";
                if (lex->unit.prepare(t, 0, 0))
                    throw CryptDBError("UNIT::prepare");

                /* XXX unit->cleanup()? */

                /* XXX
                 * for unions, it is insufficient to just print
                 * lex->select_lex, because there are other
                 * select_lex's in the unit..
                 */
            }
        } else if (SQLCOM_DELETE == lex->sql_command) {
            // 对 DELETE 操作进行准备（调用 mysql_prepare_delete）
            // 设置引用数组和排序条件。
            if (mysql_prepare_delete(t, lex->query_tables,
                                     &lex->select_lex.where))
                throw CryptDBError("mysql_prepare_delete");

            if (lex->select_lex.setup_ref_array(t,
                                    lex->select_lex.order_list.elements))
                throw CryptDBError("setup_ref_array");

            List<Item> fields;
            List<Item> all_fields;
            if (setup_order(t, lex->select_lex.ref_pointer_array,
                            lex->query_tables, fields, all_fields,
                            lex->select_lex.order_list.first))
                throw CryptDBError("setup_order");
        } else if (SQLCOM_DELETE_MULTI == lex->sql_command) {
            // 准备多表删除操作，设置条件（setup_conds）
            if (mysql_multi_delete_prepare(t)
                || t->is_fatal_error)
                throw CryptDBError("mysql_multi_delete_prepare");

            if (setup_conds(t, lex->auxiliary_table_list.first,
                            lex->select_lex.leaf_tables,
                            &lex->select_lex.where))
                throw CryptDBError("setup_conds");
        } else if (lex->sql_command ==  SQLCOM_INSERT
                   || lex->sql_command == SQLCOM_REPLACE) {
            // 为 INSERT 或 REPLACE 操作准备数据，包括处理多值插入情况
            // 逐个处理每组插入的值，为它们设置字段（setup_fields）
            List_iterator_fast<List_item> its(lex->many_values);
            List_item *values = its++;

            if (mysql_prepare_insert(t, lex->query_tables,
                                     lex->query_tables->table,
                                     lex->field_list, values,
                                     lex->update_list, lex->value_list,
                                     lex->duplicates,
                                     &lex->select_lex.where,
                                     /* select_insert */ 0,
                                     0, 0))
                throw CryptDBError("mysql_prepare_insert");

            for (;;) {
                values = its++;
                if (!values)
                    break;

                if (setup_fields(t, 0, *values, MARK_COLUMNS_NONE, 0, 0))
                    throw CryptDBError("setup_fields");
            }
        } else if (lex->sql_command ==  SQLCOM_UPDATE) {
            // 准备 UPDATE 操作
            // 设置要更新的字段和值，包括处理内部引用和字段的设置
            if (mysql_prepare_update(t, lex->query_tables,
                                     &lex->select_lex.where,
                                     lex->select_lex.order_list.elements,
                                     lex->select_lex.order_list.first))
                throw CryptDBError("mysql_prepare_update");

            if (setup_fields_with_no_wrap(t, 0, lex->select_lex.item_list,
                                          MARK_COLUMNS_NONE, 0, 0))
                throw CryptDBError("setup_fields_with_no_wrap");

            if (setup_fields(t, 0, lex->value_list,
                             MARK_COLUMNS_NONE, 0, 0))
                throw CryptDBError("setup_fields");

            List<Item> all_fields;
            if (fix_inner_refs(t, all_fields, &lex->select_lex,
                               lex->select_lex.ref_pointer_array))
                throw CryptDBError("fix_inner_refs");
        } else {
            thrower() << "don't know how to prepare command "
                      << lex->sql_command;
        }
    } catch (...) {
        cleanup();
        throw;
    }
}

