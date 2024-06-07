/*
 * Handlers for rewriting fields. 
 */

#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <stdio.h>
#include <typeinfo>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/CryptoHandlers.hh>
#include <util/cryptdb_log.hh>
#include <util/enum_text.hh>
#include <parser/lex_util.hh>


/*gives names to classes and objects we don't care to know the name of */
#define ANON                ANON_NAME(__anon_id_f_)

CItemTypesDir itemTypes = CItemTypesDir();
CItemFuncDir funcTypes = CItemFuncDir();
CItemFuncNameDir funcNames = CItemFuncNameDir();
CItemSumFuncDir sumFuncTypes = CItemSumFuncDir();


/*
 * Unclear whether this is the correct formulation for subqueries that
 * are not contained in SELECT queries. Refer to Name_resolution_context
 * definition for more information.
 */
static std::string
deductPlainTableName(const std::string &field_name,
                     Name_resolution_context *const context,
                     Analysis &a) {
    assert(context);

    const TABLE_LIST *current_table =
        context->first_name_resolution_table;
    do {
        TEST_DatabaseDiscrepancy(current_table->db, a.getDatabaseName());
        const TableMeta &tm =
            a.getTableMeta(current_table->db,
                           current_table->table_name);
        if (tm.childExists(IdentityMetaKey(field_name))) {
            return std::string(current_table->table_name);
        }

        current_table = current_table->next_local;
    } while (current_table != context->last_name_resolution_table);

    return deductPlainTableName(field_name, context->outer_context, a);
}

//对于select的选择域, 由于是FIELD_ITEM类型, 会使用到这个类的.
class ANON : public CItemSubtypeIT<Item_field, Item::Type::FIELD_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_field &i, Analysis &a) const
    {
        // LOG(cdb_v) << "do_gather_type" << i;
        const std::string fieldname = i.field_name;
        const std::string table =
            i.table_name ? i.table_name :
                            deductPlainTableName(i.field_name,
                                                 i.context, a);
        FieldMeta &fm =
            a.getFieldMeta(a.getDatabaseName(), table, fieldname);
        const EncSet es = EncSet(a, &fm);

        const std::string why = "is a field";
        reason rsn(es, why, i);

        return new RewritePlan(es, rsn);
    }

    //select的选择域rewrite的时候会用到, 从全局的rewrite调用.
    virtual Item *
    do_rewrite_type(const Item_field &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        // LOG(debug) << "***************begin do_rewrite_type **************";
        // LOG(debug) << "do_rewrite_type for " << i << ", and table name of i is " << i.table_name;
        const std::string &db_name = a.getDatabaseName();
        const std::string plain_table_name =
            i.table_name ? i.table_name :
                            deductPlainTableName(i.field_name,
                                                 i.context, a);
        // LOG(debug) << "begin get FieldMeta .....";
        const FieldMeta &fm =
            a.getFieldMeta(db_name, plain_table_name, i.field_name);
        //assert(constr.key == fm);

        //check if we need onion adjustment
        // LOG(debug) << "begin get OnionMeta .....";
        const OnionMeta &om =
            a.getOnionMeta(db_name, plain_table_name, i.field_name,
                           constr.o);
        // LOG(debug) << "begin get Onion Level .....";
        const SECLEVEL onion_level = a.getOnionLevel(om);
        assert(onion_level != SECLEVEL::INVALID);

        // LOG(debug) << "begin check adjustment .....";
        if (constr.l < onion_level) {
            //need adjustment, throw exception
            const TableMeta &tm =
                a.getTableMeta(db_name, plain_table_name);
            throw OnionAdjustExcept(tm, fm, constr.o, constr.l);
        }

        // LOG(debug) << "begin getAnonTableName .....";
        bool is_alias;
        const std::string anon_table_name =
            a.getAnonTableName(db_name, plain_table_name, &is_alias);
        const std::string anon_field_name = om.getAnonOnionName();

        // LOG(debug) << "begin make_item_field .....";
        Item_field * const res =
            make_item_field(i, anon_table_name, anon_field_name);

        // HACK: to get aliases to work in DELETE FROM statements
        if (a.inject_alias && is_alias) {
            res->db_name = NULL;
        }

        // This information is only relevant if it comes from a
        // HAVING clause.
        // FIXME: Enforce this semantically.
        a.item_cache[&i] = std::make_pair(res, constr);

        // This rewrite may be inside of an ON DUPLICATE KEY UPDATE...
        // where the query is using the VALUES(...) function.
        if (isItem_insert_value(i)) {
            const Item_insert_value &insert_i =
                static_cast<const Item_insert_value &>(i);
            return make_item_insert_value(insert_i, res);
        }

        // LOG(debug) << "***************return res**************";
        return res;
    }

    virtual void
    do_rewrite_insert_type(const Item_field &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l) const
    {
        const std::string anon_table_name =
            a.getAnonTableName(a.getDatabaseName(), i.table_name);

        Item_field *new_field = NULL;
        for (auto it : fm.orderedOnionMetas()) {
            const std::string anon_field_name =
                it.second->getAnonOnionName();
            new_field =
                make_item_field(i, anon_table_name, anon_field_name);
            l->push_back(new_field);
        }
        if (fm.getHasSalt()) {
            assert(new_field); // need an anonymized field as template to
                               // create salt item
            l->push_back(make_item_field(*new_field, anon_table_name,
                                  fm.getSaltName()));
        }
    }
} ANON;
