#include <crypto/ope.hh>
#include <crypto/prng.hh>
#include <crypto/hgd.hh>
#include <crypto/aes.hh>
#include <crypto/sha.hh>
#include <crypto/hmac.hh>
#include <util/zz.hh>
#include <util/util.hh>
#include <sys/time.h>
#include <stdlib.h>
#include <util/cryptdb_log.hh>

using namespace std;
using namespace NTL;

// 静态成员变量的定义
std::unique_ptr<blowfish> OPE::bf = nullptr;
std::unique_ptr<ffx2_block_cipher<blowfish, nBITS>> OPE::fk = nullptr;
std::unique_ptr<ope_server<NBITS_TYPE>> OPE::ope_serv = nullptr;
std::unique_ptr<ope_client<NBITS_TYPE, ffx2_block_cipher<blowfish, nBITS>>> OPE::ope_clnt = nullptr;
std::once_flag OPE::init_flag;

ZZ
OPE::encrypt(const ZZ &ptext)
{
    LOG(debug) << "-----> origin text is " << ptext;
    std::vector<uint64_t> pbytes = conv_zz_to_u64(ptext);

    std::vector<uint64_t> cbytes;
    for (size_t i = 0; i < pbytes.size(); ++i) {
        LOG(debug) << "begin to encrypt byte: " << pbytes[i]; 
        cbytes.push_back(ope_clnt->encrypt(pbytes[i]));
    }
    
    auto res = conv_u64_to_zz(cbytes);
    LOG(debug) << "------> OPE encrypt result is " << res;
    return res;
}

ZZ
OPE::decrypt(const ZZ &ctext)
{
    LOG(debug) << ">>>>>> begin to decrypt " << ctext;
    std::vector<uint64_t> cbytes = conv_zz_to_u64(ctext);

    std::vector<uint64_t> pbytes; 
    for (size_t i=0; i < cbytes.size(); ++i) {
        pbytes.push_back(ope_clnt->decrypt(cbytes[i]));
    }

    auto res = conv_u64_to_zz(pbytes);
    LOG(debug) << ">>>>>> decrypt result is " << res;
    return res;
}

ZZ 
OPE::conv_u64_to_zz(const std::vector<uint64_t> u64_bytes) {
    // 获取 unsigned char* 指针
    size_t total_bytes = u64_bytes.size() * sizeof(uint64_t);
    std::unique_ptr<unsigned char[]> byte_ptr(new unsigned char[total_bytes]);
    std::memcpy(byte_ptr.get(), u64_bytes.data(), total_bytes);
    // 计算总字节数
    return ZZFromBytes(reinterpret_cast<const uint8_t *>(byte_ptr.get()),
                       total_bytes);
}

std::vector<uint64_t> 
OPE::conv_zz_to_u64(const NTL::ZZ &zz) {
    std::vector<uint64_t> result;

    // 计算 ZZ 的字节数
    size_t total_bytes = NTL::NumBytes(zz);

    // 将 ZZ 转换为字节数组
    std::unique_ptr<unsigned char[]> byte_ptr(new unsigned char[total_bytes]);
    BytesFromZZ(byte_ptr.get(), zz, total_bytes);  // 使用 BytesFromZZ 将 ZZ 转换为字节

    // 使用指针转换将字节数组转换为 uint64_t 数组
    uint64_t* ptr = reinterpret_cast<uint64_t*>(byte_ptr.get());

    size_t num_elements = total_bytes / sizeof(uint64_t);
    for (size_t i = 0; i < num_elements; ++i) {
        result.push_back(ptr[i]);
    }

    // 如果剩余的字节不满 64 位（即不足 8 字节），需要处理剩余的部分
    size_t remaining_bytes = total_bytes % sizeof(uint64_t);
    if (remaining_bytes > 0) {
        uint64_t last_val = 0;
        std::memcpy(&last_val, byte_ptr.get() + num_elements * sizeof(uint64_t), remaining_bytes);
        result.push_back(last_val);
    }

    // 如果 ZZ 为零，则至少添加一个元素 0
    if (result.empty()) {
        result.push_back(0);
    }

    return result;
}