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
    std::vector<uint64_t> pbytes = conv_zz_to_u64(ptext);

    std::vector<uint64_t> cbytes;
    for (size_t i = 0; i < pbytes.size(); ++i) { 
        cbytes.push_back(ope_clnt->encrypt(pbytes[i]));
    }
    
    auto res = conv_u64_to_zz(cbytes);
    return res;
}

ZZ
OPE::decrypt(const ZZ &ctext)
{
    std::vector<uint64_t> cbytes = conv_zz_to_u64(ctext);

    std::vector<uint64_t> pbytes; 
    for (size_t i=0; i < cbytes.size(); ++i) {
        pbytes.push_back(ope_clnt->decrypt(cbytes[i]));
    }

    auto res = conv_u64_to_zz(pbytes);
    return res;
}

// 辅助函数定义
void uint64_to_big_endian_bytes(uint64_t value, unsigned char* bytes) {
    for (int i = 0; i < 8; ++i) {
        bytes[i] = static_cast<unsigned char>((value >> (56 - 8 * i)) & 0xFF);
    }
}

uint64_t big_endian_bytes_to_uint64(const unsigned char* bytes) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
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
    // 计算 ZZ 的字节数
    size_t total_bytes = NTL::NumBytes(zz);
    
    // 将 ZZ 转换为字节数组（大端字节序）
    std::unique_ptr<unsigned char[]> byte_ptr(new unsigned char[total_bytes]);
    BytesFromZZ(byte_ptr.get(), zz, total_bytes);

    // 计算uint64_t数组的大小
    size_t num_elements = total_bytes / sizeof(uint64_t);
    if (total_bytes % sizeof(uint64_t) > 0) {
        num_elements += 1;
    }
    // 拷贝到uint64_t数组
    std::vector<uint64_t> result(num_elements);
    std::memcpy(result.data(), byte_ptr.get(), total_bytes);

    return result;
}