#pragma once

#include <stdexcept>
#include <ostream>
#include <openssl/bn.h>
#include <openssl/crypto.h>

#include <util/errstream.hh>

class _bignum_ctx {
 public:
    _bignum_ctx() { c = BN_CTX_new(); }
    ~_bignum_ctx() { BN_CTX_free(c); }
    BN_CTX *ctx() { return c; }

    static BN_CTX *the_ctx() {
        static _bignum_ctx cx;
        return cx.ctx();
    }

 private:
    BN_CTX *c;
};

class bignum {
 public:
    bignum() {
        b = BN_new();  // 使用 BN_new 替代 BN_init
        if (b == nullptr) throw std::runtime_error("BN_new failed");
    }

    bignum(unsigned long v) {
        b = BN_new();  // 使用 BN_new
        if (b == nullptr) throw std::runtime_error("BN_new failed");
        if (!BN_set_word(b, v)) throw std::runtime_error("BN_set_word failed");
    }

    bignum(const bignum &other) {
        b = BN_new();  // 使用 BN_new
        if (b == nullptr) throw std::runtime_error("BN_new failed");
        if (!BN_copy(b, other.bn())) throw std::runtime_error("BN_copy failed");
    }

    bignum(const uint8_t *buf, size_t nbytes) {
        b = BN_new();  // 使用 BN_new
        if (b == nullptr) throw std::runtime_error("BN_new failed");
        if (!BN_bin2bn(buf, nbytes, b)) throw std::runtime_error("BN_bin2bn failed");
    }

    bignum(const std::string &v) {
        b = BN_new();  // 使用 BN_new
        if (b == nullptr) throw std::runtime_error("BN_new failed");
        if (!BN_bin2bn((uint8_t*) v.data(), v.size(), b)) throw std::runtime_error("BN_bin2bn failed");
    }

    ~bignum() {
        if (b != nullptr) {
            BN_free(b);  // 释放内存
        }
    }

    BIGNUM *bn() { return b; }
    const BIGNUM *bn() const { return b; }

    unsigned long word() const {
        unsigned long v = BN_get_word(b);
        if (v == 0xffffffffL)
            throw std::runtime_error("out of range");
        return v;
    }

#define op(opname, func, args...)                               \
    bignum opname(const bignum &mod) {                          \
        bignum res;                                             \
        if (!func(res.bn(), b, mod.bn(), ##args)) {             \
            throw std::runtime_error(#func " failed");           \
        }                                                        \
        return res;                                             \
    }

    op(operator+, BN_add)
    op(operator-, BN_sub)
    op(operator%, BN_mod, _bignum_ctx::the_ctx())
    op(operator*, BN_mul, _bignum_ctx::the_ctx())
#undef op

#define pred(predname, cmp)                                     \
    bool predname(const bignum &other) {                        \
        return BN_cmp(b, other.bn()) cmp;                       \
    }

    pred(operator<,  <  0)
    pred(operator<=, <= 0)
    pred(operator>,  >  0)
    pred(operator>=, >= 0)
    pred(operator==, == 0)
#undef pred

    bignum invmod(const bignum &mod) {
        bignum r;
        if (!BN_mod_inverse(r.bn(), b, mod.bn(), _bignum_ctx::the_ctx())) {
            throw std::runtime_error("BN_mod_inverse failed");
        }
        return r;
    }

 private:
    BIGNUM *b;  // 在 OpenSSL 1.1.x 中，BIGNUM 是一个指针
};

static inline std::ostream&
operator<<(std::ostream &out, const bignum &bn)
{
    char *s = BN_bn2dec(bn.bn());
    if (s == nullptr) throw std::runtime_error("BN_bn2dec failed");
    out << s;
    OPENSSL_free(s);
    return out;
}
