/*
 * Handlers for rewriting constants, integers, strings, decimals, date, etc. 
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

/* class/object names we don't care to know the name of */
#define ANON                ANON_NAME(__anon_id_const)

// encrypts a constant item based on the information in Analysis
static Item *
encrypt_item(const Item &i, const OLK &olk, Analysis &a) {
    assert(!RiboldMYSQL::is_null(i));

    FieldMeta * const fm = olk.key;
    // HACK + BROKEN.
    if (!fm && oPLAIN == olk.o) {
        return RiboldMYSQL::clone_item(i);
    }
    assert(fm);

    const onion o = olk.o;
    // LOG(cdb_v) << fm->getFieldName() << " " << fm->getChildren().size();

    const auto it = a.salts.find(fm);
    const salt_type IV = (it == a.salts.end()) ? 0 : it->second;
    OnionMeta * const om = fm->getOnionMeta(o);
    Item * const ret_i = encrypt_item_layers(i, o, *om, a, IV);

    return ret_i;
}

// 这段代码定义了一个匿名的静态类，并继承自 CItemSubtypeIT<Item_string, Item::Type::STRING_ITEM>。其中 ANON 是用宏生成的一个唯一的类名。
static class ANON : public CItemSubtypeIT<Item_string,
                                          Item::Type::STRING_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_string &i, Analysis &a) const
    {
        // LOG(cdb_v) << "do_gather_type" << i;
        const std::string why = "is a string constant";
        reason rsn(FULL_EncSet_Str, why, i);
        return new RewritePlan(FULL_EncSet_Str, rsn);
    }

    virtual Item * do_optimize_type(Item_string *i, Analysis & a) const {
        return i;
    }

    virtual Item *
    do_rewrite_type(const Item_string &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        // LOG(debug) << "do_rewrite_type " << i;
        return encrypt_item(i, constr, a);
    }

    virtual void
    do_rewrite_insert_type(const Item_string &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l,
                           THD* thd=nullptr, 
                           pthread_mutex_t *memRootMutex=nullptr) const
    {
        typical_rewrite_insert_type(i, fm, a, l);
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_float,
                                          Item::Type::REAL_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_float &i, Analysis &a) const
    {
        // LOG(cdb_v) << "do_gather_type" << i;
        const std::string why = "is a float constant";
        reason rsn(PLAIN_EncSet, why, i);
        return new RewritePlan(PLAIN_EncSet, rsn);
    }

    virtual Item *
    do_rewrite_type(const Item_float &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        // LOG(debug) << "do_rewrite_type " << i;
        return encrypt_item(i, constr, a);
    }

    virtual void
    do_rewrite_insert_type(const Item_float &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l,
                           THD* thd=nullptr, 
                           pthread_mutex_t *memRootMutex=nullptr) const
    {
        typical_rewrite_insert_type(i, fm, a, l);
    }
} ANON;


static class ANON : public CItemSubtypeIT<Item_int, Item::Type::INT_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_int &i, Analysis &a) const
    {
        // LOG(cdb_v) << "do_gather_type" << i;
        const std::string why = "is an int constant";
        reason rsn(FULL_EncSet_Int, why, i);
        return new RewritePlan(FULL_EncSet_Int, rsn);
    }

    virtual Item * do_optimize_type(Item_int *i, Analysis & a) const
    {
        return i;
    }

    virtual Item *
    do_rewrite_type(const Item_int &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        // LOG(cdb_v) << "do_rewrite_type " << i;

        return encrypt_item(i, constr, a);
    }

    virtual void
    do_rewrite_insert_type(const Item_int &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l,
                           THD* thd=nullptr, 
                           pthread_mutex_t *memRootMutex=nullptr) const
    {
        typical_rewrite_insert_type(i, fm, a, l);
    }
} ANON;

static class ANON : public CItemSubtypeIT<Item_decimal,
                                          Item::Type::DECIMAL_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_decimal &i, Analysis &a) const
    {
        // LOG(cdb_v) << "do_gather_type" << i;

        const std::string why = "is a decimal constant";
        reason rsn(FULL_EncSet, why, i);
        return new RewritePlan(FULL_EncSet_Int, rsn);
    }

    virtual Item * do_optimize_type(Item_decimal *i, Analysis & a) const
    {
        return i;
    }

    virtual Item *
    do_rewrite_type(const Item_decimal &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) const
    {
        // LOG(cdb_v) << "do_rewrite_type " << i;

        return encrypt_item(i, constr, a);
    }

    virtual void
    do_rewrite_insert_type(const Item_decimal &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l,
                           THD* thd=nullptr, 
                           pthread_mutex_t *memRootMutex=nullptr) const
    {
        typical_rewrite_insert_type(i, fm, a, l);
    }
} ANON;
