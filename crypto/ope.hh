#pragma once

#include <string>
#include <map>

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
    : key(keyarg), pbits(plainbits), cbits(cipherbits), aesk(""), ope_clnt(nullptr) {

        bf = new blowfish(key);
        fk = new ffx2_block_cipher<blowfish, nBITS>(bf,{});

        // Initialize ope_clnt with the block cipher
        ope_serv = new ope_server<NBITS_TYPE>();
        ope_clnt = new ope_client<NBITS_TYPE, ffx2_block_cipher<blowfish, nBITS>>(fk, ope_serv);
    }

    ~OPE() {
        delete bf;
        delete fk;
        delete ope_serv;
        delete ope_clnt; // Clean up allocated memory
    }

    NTL::ZZ encrypt(const NTL::ZZ &ptext);
    NTL::ZZ decrypt(const NTL::ZZ &ctext);

 private:
    NTL::ZZ conv_u64_to_zz(const std::vector<uint64_t> resBytes);
    std::vector<uint64_t> conv_zz_to_u64(const NTL::ZZ &zz);
    std::string key;
    size_t pbits;
    size_t cbits;
    std::string aesk;
    
    blowfish *bf;
    ffx2_block_cipher<blowfish, nBITS> *fk;
    ope_server<NBITS_TYPE> *ope_serv;
    ope_client<NBITS_TYPE, ffx2_block_cipher<blowfish, nBITS>> *ope_clnt;
};

