#include <crypto/ope.hh>
#include <crypto/prng.hh>
#include <crypto/hgd.hh>
#include <crypto/aes.hh>
#include <crypto/sha.hh>
#include <crypto/hmac.hh>
#include <util/zz.hh>

/*
 * A gap is represented by the next integer value _above_ the gap.
 */
static ZZ
domain_gap(const ZZ &ndomain, const ZZ &nrange, const ZZ &rgap, PRNG *prng)
{
    return HGD(rgap, ndomain, nrange-ndomain, prng);
}

/**
递归方案
 */
// template<class CB>
// ope_domain_range 
// OPE::lazy_sample(const ZZ &d_lo, const ZZ &d_hi,
//                                   const ZZ &r_lo, const ZZ &r_hi,
//                                   CB go_low, blockrng<AES> *prng) {
//     ZZ ndomain = d_hi - d_lo + 1;
//     ZZ nrange  = r_hi - r_lo + 1;
//     throw_c(nrange >= ndomain);

//     if (ndomain == 1)
//         return ope_domain_range(d_lo, r_lo, r_hi);

//     auto v = hmac<sha256>::mac(StringFromZZ(d_lo) + "/" +
//                                StringFromZZ(d_hi) + "/" +
//                                StringFromZZ(r_lo) + "/" +
//                                StringFromZZ(r_hi), key);
//     v.resize(AES::blocksize);
//     prng->set_ctr(v);

//     ZZ rgap = nrange / 2;
//     ZZ dgap;

//     // auto ci = dgap_cache.find(r_lo + rgap);
//     // if (ci == dgap_cache.end()) {
//     //     dgap = domain_gap(ndomain, nrange, nrange / 2, prng);
//     //     dgap_cache[r_lo + rgap] = dgap;
//     // } else {
//     //     dgap = ci->second;
//     // }

//     // 使用 get 方法查找键
//     if (!dgap_cache.get(r_lo + rgap, dgap)) {
//         dgap = domain_gap(ndomain, nrange, nrange / 2, prng);
//         dgap_cache.put(r_lo + rgap, dgap);  // 插入缓存
//     }

//     if (go_low(d_lo + dgap, r_lo + rgap))
//         return lazy_sample(d_lo, d_lo + dgap - 1, r_lo, r_lo + rgap - 1, go_low, prng);
//     else
//         return lazy_sample(d_lo + dgap, d_hi, r_lo + rgap, r_hi, go_low, prng);
// }

/**
迭代方案
 */ 
template<class CB>
ope_domain_range 
OPE::lazy_sample(const ZZ &d_lo, const ZZ &d_hi,
                const ZZ &r_lo, const ZZ &r_hi,
                CB go_low, blockrng<AES> *prng) {
    ZZ current_d_lo = d_lo;
    ZZ current_d_hi = d_hi;
    ZZ current_r_lo = r_lo;
    ZZ current_r_hi = r_hi;

    std::string serialized;
    serialized.reserve(4 * NumBytes(current_d_lo)); // 预留足够的空间

    while (true) {
        ZZ ndomain = current_d_hi - current_d_lo + 1;
        ZZ nrange  = current_r_hi - current_r_lo + 1;

        if (!(nrange >= ndomain)) {
            throw std::invalid_argument("nrange must be >= ndomain");
        }

        if (ndomain == 1)
            return ope_domain_range(current_d_lo, current_r_lo, current_r_hi);

        // 重置序列化字符串
        serialized.clear();
        serialized.reserve(4 * NumBytes(current_d_lo));

        serialized += StringFromZZ(current_d_lo)+"/";
        serialized += StringFromZZ(current_d_hi)+"/";
        serialized += StringFromZZ(current_r_lo)+"/";
        serialized += StringFromZZ(current_r_hi);

        // HMAC 计算
        auto v = hmac<sha256>::mac(serialized, key);
        v.resize(AES::blocksize);
        prng->set_ctr(v);

        ZZ rgap = nrange / 2;
        ZZ dgap;

        // 使用当前循环的键进行缓存查找
        ZZ current_cache_key = current_r_lo + rgap;
        if (!dgap_cache.get(current_cache_key, dgap)) {
            dgap = domain_gap(ndomain, nrange, rgap, prng);
            dgap_cache.put(current_cache_key, dgap);
        }

        if (go_low(current_d_lo + dgap, current_r_lo + rgap)) {
            current_d_hi = current_d_lo + dgap - 1;
            current_r_hi = current_r_lo + rgap - 1;
        } else {
            current_d_lo = current_d_lo + dgap;
            current_r_lo = current_r_lo + rgap;
        }
    }
}

template<class CB>
ope_domain_range
OPE::search(CB go_low)
{
    blockrng<AES> r(aesk);

    return lazy_sample(to_ZZ(0), to_ZZ(1) << pbits,
                       to_ZZ(0), to_ZZ(1) << cbits,
                       go_low, &r);
}

ZZ
OPE::encrypt(const ZZ &ptext)
{
    ope_domain_range dr =
        search([&ptext](const ZZ &d, const ZZ &) { return ptext < d; });

    auto v = sha256::hash(StringFromZZ(ptext));
    v.resize(16);

    blockrng<AES> aesrand(aesk);
    aesrand.set_ctr(v);

    ZZ nrange = dr.r_hi - dr.r_lo + 1;
    return dr.r_lo + aesrand.rand_zz_mod(nrange);
}

ZZ
OPE::decrypt(const ZZ &ctext)
{
    ope_domain_range dr =
        search([&ctext](const ZZ &, const ZZ &r) { return ctext < r; });
    return dr.d;
}
