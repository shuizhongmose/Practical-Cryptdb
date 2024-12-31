#pragma once

/**
 * @brief 论文《Popa R A, Redfield C M S, Zeldovich N, et al. CryptDB: Protecting confidentiality with encrypted query processing[C]//
 * Proceedings of the twenty-third ACM symposium on operating systems principles. 2011: 85-100.》中改进OPE的实现
 * 使用该OPE时，用ope-PRZB11.*替换ope.*的内容
 */

#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <cstdint>

#include <crypto/prng.hh>
#include <crypto/aes.hh>
#include <crypto/sha.hh>
#include <crypto/online_ope.hh>
// not use min macros in mysql-src
#undef min
#include <crypto/ffx.hh>

#include <NTL/ZZ.h>

using namespace std;
using namespace NTL;



typedef uint64_t NBITS_TYPE;
const static uint nBITS = sizeof(NBITS_TYPE)*8;

class OPE {
 public:
    OPE(const std::string &keyarg, size_t plainbits, size_t cipherbits)
    : key(keyarg), pbits(plainbits), cbits(cipherbits), aesk("") {
        // 使用 std::call_once 确保静态成员只初始化一次
        std::call_once(init_flag, &OPE::initialize_static_members, key);
    }

    ~OPE() = default; // 不需要手动删除静态成员

    NTL::ZZ encrypt(const NTL::ZZ &ptext);
    NTL::ZZ decrypt(const NTL::ZZ &ctext);

 private:
    NTL::ZZ conv_u64_to_zz(const std::vector<uint64_t> resBytes);
    std::vector<uint64_t> conv_zz_to_u64(const NTL::ZZ &zz);
    std::string key;
    size_t pbits;
    size_t cbits;
    std::string aesk;
    
    static void initialize_static_members(const std::string &keyarg) {
        bf = std::unique_ptr<blowfish>(new blowfish(keyarg));

        fk = std::unique_ptr<ffx2_block_cipher<blowfish, nBITS>>(
            new ffx2_block_cipher<blowfish, nBITS>(bf.get(), {}));

        ope_serv = std::unique_ptr<ope_server<NBITS_TYPE>>(
            new ope_server<NBITS_TYPE>());

        ope_clnt = std::unique_ptr<ope_client<NBITS_TYPE, ffx2_block_cipher<blowfish, nBITS>>>(
            new ope_client<NBITS_TYPE, ffx2_block_cipher<blowfish, nBITS>>(fk.get(), ope_serv.get()));
    }

    // 用于确保静态成员只初始化一次
    static std::once_flag init_flag;
    // 静态成员变量，使用智能指针管理内存
    static std::unique_ptr<blowfish> bf;
    static std::unique_ptr<ffx2_block_cipher<blowfish, nBITS>> fk;
    static std::unique_ptr<ope_server<NBITS_TYPE>> ope_serv;
    static std::unique_ptr<ope_client<NBITS_TYPE, ffx2_block_cipher<blowfish, nBITS>>> ope_clnt;
};