/* Optional test-output compression. Hashes every state byte; it neither
   writes EE memory nor changes simulation inputs. Raw output stays default.
   Windows CNG is checked against Python hashlib on the raw oracle fixture. */
#ifndef GT4_TEST_STATE_DIGEST_H
#define GT4_TEST_STATE_DIGEST_H
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#endif
static int state_write(FILE *output,const void *data,unsigned size)
{
    if(!getenv("PHYS_STATE_DIGEST"))return fwrite(data,1,size,output)==size;
#ifdef _WIN32
    static BCRYPT_ALG_HANDLE algorithm;
    static BCRYPT_HASH_HANDLE hash;
    unsigned char digest[32];
    if(!algorithm && BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)return 0;
    if(!hash && BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,BCRYPT_HASH_REUSABLE_FLAG)<0)return 0;
    if(BCryptHashData(hash,(PUCHAR)data,size,0)<0 || BCryptFinishHash(hash,digest,sizeof digest,0)<0)return 0;
    return size>=0x6d8 && fwrite(digest,1,sizeof digest,output)==sizeof digest
        && fwrite((const unsigned char *)data+0x6d4,1,4,output)==4;
#else
    fputs("PHYS_STATE_DIGEST requires Windows CNG; use raw output here.\n",stderr);return 0;
#endif
}
#endif
