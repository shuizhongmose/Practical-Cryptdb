#include <crypto/ope.hh>
#include <crypto/prng.hh>
#include <crypto/hgd.hh>
#include <crypto/aes.hh>
#include <crypto/sha.hh>
#include <crypto/hmac.hh>
#include <util/zz.hh>
#include <sys/time.h>
#include <stdlib.h>
#include <util/cryptdb_log.hh>

using namespace std;
using namespace NTL;

ZZ
OPE::encrypt(const ZZ &ptext)
{
    std::vector<uint64_t> pbytes = conv_zz_to_u64(ptext);

    std::vector<uint64_t> cbytes;
    for (size_t i = 0; i < pbytes.size(); ++i) {
        cbytes.push_back(ope_clnt->encrypt(pbytes[i]));
    }

    return conv_u64_to_zz(cbytes);
}

ZZ
OPE::decrypt(const ZZ &ctext)
{
    std::vector<uint64_t> cbytes = conv_zz_to_u64(ctext);

    std::vector<uint64_t> pbytes; 
    for (size_t i=0; i < cbytes.size(); ++i) {
        pbytes.push_back(ope_clnt->decrypt(cbytes[i]));
    }

    return conv_u64_to_zz(pbytes);
}

ZZ 
OPE::conv_u64_to_zz(const std::vector<uint64_t> u64_bytes) {
    NTL::ZZ zz;
    for (auto val : u64_bytes) {
        NTL::ZZ temp(val);
        zz <<= 64; // 左移 64 位，为下一个 uint64_t 腾出空间
        zz += temp; // 将当前 uint64_t 加到 ZZ 中
    }
    return zz;
}

std::vector<uint64_t> 
OPE::conv_zz_to_u64(const NTL::ZZ &zz) {
    std::vector<uint64_t> result;
    
    // Convert each digit of zz to uint64_t and store in the vector
    NTL::ZZ x = zz;
    while (x != 0) {
        result.push_back(NTL::to_ulong(x & NTL::ZZ(0xFFFFFFFFFFFFFFFFULL))); // Extract least significant bits
        x >>= 64; // Shift right by 64 bits
    }
    
    // If zz is zero, add at least one element to the result vector
    if (result.empty()) {
        result.push_back(0);
    }
    
    return result;
}