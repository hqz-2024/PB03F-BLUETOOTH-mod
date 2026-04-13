/*
    Counter with CBC-MAC (CCM) with AES

    Copyright (c) 2010-2012, Jouni Malinen <j@w1.fi>

    This software may be distributed under the terms of the BSD license.
    See README for more details.
*/

#include <string.h>

#include "aes.h"
#include "ll_enc.h"

#define PUT_BE16(a, val)            \
    do {                    \
        (a)[0] = ((u16) (val)) >> 8;    \
        (a)[1] = ((u16) (val)) & 0xff;  \
    } while (0)


static void xor_aes_block(u8* dst, const u8* src)
{
    #ifdef HAVE_ALIGNED_MEM_OPERATION
    u32* d = (u32*) dst;
    u32* s = (u32*) src;
    *d++ ^= *s++;
    *d++ ^= *s++;
    *d++ ^= *s++;
    *d++ ^= *s++;
    #else /* HAVE_ALIGNED_MEM_OPERATION */
    u32 i;

    for (i = 0; i < 16; i++)
    {
        *dst++ ^= *src++;
    }

    #endif /* HAVE_ALIGNED_MEM_OPERATION */
}


static void aes_ccm_auth_start(void* aes, size_t M, size_t L, const u8* nonce,
                               const u8* aad, size_t aad_len, size_t plain_len,
                               u8* x)
{
    u8 aad_buf[2 * AES_BLOCK_SIZE];
    u8 b[AES_BLOCK_SIZE];
    /* Authentication */
    /* B_0: Flags | Nonce N | l(m) */
    b[0] = aad_len ? 0x40 : 0 /* Adata */;
    b[0] |= (((M - 2) / 2) /* M' */ << 3);
    b[0] |= (L - 1) /* L' */;
    memcpy(&b[1], nonce, 15 - L);
    PUT_BE16(&b[AES_BLOCK_SIZE - L], plain_len);
    aes_encrypt(aes, b, x); /* X_1 = E(K, B_0) */

    if (!aad_len)
        return;

    PUT_BE16(aad_buf, aad_len);
    memcpy(aad_buf + 2, aad, aad_len);
    memset(aad_buf + 2 + aad_len, 0, sizeof(aad_buf) - 2 - aad_len);
    xor_aes_block(aad_buf, x);
    aes_encrypt(aes, aad_buf, x); /* X_2 = E(K, X_1 XOR B_1) */

    if (aad_len > AES_BLOCK_SIZE - 2)
    {
        xor_aes_block(&aad_buf[AES_BLOCK_SIZE], x);
        /* X_3 = E(K, X_2 XOR B_2) */
        aes_encrypt(aes, &aad_buf[AES_BLOCK_SIZE], x);
    }
}


static void aes_ccm_auth(void* aes, const u8* data, size_t len, u8* x)
{
    size_t last = len % AES_BLOCK_SIZE;
    size_t i;

    for (i = 0; i < len / AES_BLOCK_SIZE; i++)
    {
        /* X_i+1 = E(K, X_i XOR B_i) */
        xor_aes_block(x, data);
        data += AES_BLOCK_SIZE;
        aes_encrypt(aes, x, x);
    }

    if (last)
    {
        /* XOR zero-padded last block */
        for (i = 0; i < last; i++)
            x[i] ^= *data++;

        aes_encrypt(aes, x, x);
    }
}


static void aes_ccm_encr_start(size_t L, const u8* nonce, u8* a)
{
    /* A_i = Flags | Nonce N | Counter i */
    a[0] = L - 1; /* Flags = L' */
    memcpy(&a[1], nonce, 15 - L);
}


static void aes_ccm_encr(void* aes, size_t L, const u8* in, size_t len, u8* out,
                         u8* a)
{
    size_t last = len % AES_BLOCK_SIZE;
    size_t i;

    /* crypt = msg XOR (S_1 | S_2 | ... | S_n) */
    for (i = 1; i <= len / AES_BLOCK_SIZE; i++)
    {
        PUT_BE16(&a[AES_BLOCK_SIZE - 2], i);
        /* S_i = E(K, A_i) */
        aes_encrypt(aes, a, out);
        xor_aes_block(out, in);
        out += AES_BLOCK_SIZE;
        in += AES_BLOCK_SIZE;
    }

    #if 0

    if (last)
    {
        PUT_BE16(&a[AES_BLOCK_SIZE - 2], i);
        aes_encrypt(aes, a, out);

        /* XOR zero-padded last block */
        for (i = 0; i < last; i++)
            *out++ ^= *in++;
    }

    #else /* 0 */

    if (last)
    {
        u8 tout[AES_BLOCK_SIZE];
        PUT_BE16(&a[AES_BLOCK_SIZE - 2], i);
        aes_encrypt(aes, a, tout);

        /* XOR zero-padded last block */
        for (i = 0; i < last; i++)
            *out++ = tout[i] ^ *in++;
    }

    #endif /* 0 */
}


static void aes_ccm_encr_auth(void* aes, size_t M, u8* x, u8* a, u8* auth)
{
    size_t i;
    u8 tmp[AES_BLOCK_SIZE];
    /* U = T XOR S_0; S_0 = E(K, A_0) */
    PUT_BE16(&a[AES_BLOCK_SIZE - 2], 0);
    aes_encrypt(aes, a, tmp);

    for (i = 0; i < M; i++)
        auth[i] = x[i] ^ tmp[i];
}


static void aes_ccm_decr_auth(void* aes, size_t M, u8* a, const u8* auth, u8* t)
{
    size_t i;
    u8 tmp[AES_BLOCK_SIZE];
    /* U = T XOR S_0; S_0 = E(K, A_0) */
    PUT_BE16(&a[AES_BLOCK_SIZE - 2], 0);
    aes_encrypt(aes, a, tmp);

    for (i = 0; i < M; i++)
        t[i] = auth[i] ^ tmp[i];
}

static void xor128bit(uint8_t * x,uint8_t  *y,uint8_t  *z)
{
    for(int i=0;i<16;i++)
    {
        z[i]=x[i]^y[i];
    }
}

void aes_ccm_cbc_mic(uint8_t* key,uint8_t* b, uint8_t* din, uint32_t dLen,uint8_t* cbcOut,uint8_t macLen)
{
    int loopNum = (dLen+15)/16;
    int resLen  = dLen-(loopNum-1)*16;
    int i;
    uint8_t  bx[16];
    uint8_t  y[16];
    uint8_t  t[16];

    memcpy(bx,b,16);

    for(i=0;i<loopNum;i++)
    {
        LL_ENC_AES128_Encrypt(key,bx,y);

        if(i<loopNum-1)
        {
            xor128bit(y,din+i*16,bx);
        }
        else
        {
            memcpy(t,din+i*16,resLen);
            memset(t+resLen,0,16-resLen);
            xor128bit(y,t,bx);
        }
    }

    LL_ENC_AES128_Encrypt(key,bx,y);
    memcpy(cbcOut,y,macLen);
}

void aes_ccm_cnt_mod(uint8_t incCnt,uint8_t* key, uint8_t* a, uint8_t* din, uint32_t dLen,uint8_t* dout,uint8_t* micOut,uint8_t micLen)
{
    int loopNum = (dLen+15)/16;
    int resLen  = dLen-(loopNum-1)*16;
    int i;
    uint8_t  ax[16];
    uint8_t  s[16];
    uint8_t  t[16];
    uint8_t  to[16];
    uint16_t cnt = 0;
    memcpy(ax,a,16);

    if(dout)
    {
        for(i=0;i<loopNum;i++)
        {
            cnt=cnt+incCnt;
            ax[14] = (cnt>>8)&0xffff;
            ax[15] = (cnt&0xffff);
            LL_ENC_AES128_Encrypt(key,ax,s);

            if(i<loopNum-1)
            {
                xor128bit(s,din+i*16,dout+i*16);
            }
            else
            {
                memcpy(t,din+i*16,resLen);
                memset(t+resLen,0,16-resLen);
                xor128bit(s,t,to);
                memcpy(dout+i*16,to,resLen);
            }
        }
    }
    //micOut
    LL_ENC_AES128_Encrypt(key,a,s);

    if(micOut)
    {
        for(i=0;i<micLen;i++)
            micOut[i]=micOut[i]^s[i];
    }

}


void aes_ccm_encrypt(uint8_t mod,uint8_t* key, uint8_t* nounce, uint8_t* aad, uint8_t aadLen,uint8_t bFlg,uint8_t aFlg,\
                uint8_t* din, uint32_t dLen,uint8_t* dout,uint8_t* micOut,uint8_t macLen)
{
    uint8_t bx[16];
    uint8_t ax[16];
    uint8_t y[16];
    uint8_t cbcOut[8];
    uint8_t incCnt = (mod==0) ?1:13;

    //------------------------------------------------------
    // cipher block chain for mic
    //b1 init
    bx[0] = bFlg;
    memcpy(bx+1,nounce,13);
    if(mod==0)
    {
        bx[14]= (dLen>>8)&0xff;
        bx[15]= (dLen)&0xff;
        LL_ENC_AES128_Encrypt(key,bx,y);
        if(aadLen)
        {
            ax[0]=0x00;
            ax[1]=aadLen;
            memcpy(ax+2,aad,aadLen);
            memset(ax+2+aadLen,0,16-2-aadLen);
            xor128bit(y,ax,bx);
        }
    }
    else
    {
        bx[14]= (dLen>>12)&0xff;
        bx[15]= (dLen>>4)&0xff;
    }

    aes_ccm_cbc_mic(key,bx, din, dLen,cbcOut,macLen);


    //------------------------------------------------------
    // counter mode for encrypt
    //a1 init
    ax[0]=aFlg;
    memcpy(ax+1,nounce,13);
    ax[14]=0;
    ax[15]=0;
    aes_ccm_cnt_mod(incCnt,key,ax, din, dLen,dout,cbcOut,macLen);

    memcpy(micOut,cbcOut,macLen);
    
}
int aes_ccm_decrypt(uint8_t mod,uint8_t* key, uint8_t* nounce, uint8_t* aad, uint8_t aadLen,uint8_t bFlg,uint8_t aFlg,\
                uint8_t* din, uint32_t dLen,uint8_t* dout,uint8_t* micIn,uint8_t macLen)
{
    uint8_t bx[16];
    uint8_t ax[16];
    uint8_t y[16];
    uint8_t cbcOut[8];
    uint8_t incCnt = (mod==0) ?1:13;

    //------------------------------------------------------
    // counter mode for decrypt
    //a1 init
    ax[0]=aFlg;
    memcpy(ax+1,nounce,13);
    ax[14]=0;
    ax[15]=0;
    memcpy(cbcOut,micIn,macLen);
    aes_ccm_cnt_mod(incCnt,key,ax, din, dLen,dout,cbcOut,macLen);

    //------------------------------------------------------
    // cipher block chain for mic
    //b1 init
    bx[0] = bFlg;
    memcpy(bx+1,nounce,13);

    if(mod == 0)
    {
        bx[14]= (dLen>>8)&0xff;
        bx[15]= (dLen)&0xff;
        
        
        LL_ENC_AES128_Encrypt(key,bx,y);
        if(aadLen)
        {
            ax[0]=0x00;
            ax[1]=aadLen;
            memcpy(ax+2,aad,aadLen);
            memset(ax+2+aadLen,0,16-2-aadLen);
            xor128bit(y,ax,bx);
        }
    }
    else
    {
        bx[14]= (dLen>>12)&0xff;
        bx[15]= (dLen>>4)&0xff;
    }
    aes_ccm_cbc_mic(key,bx, dout, dLen,y,macLen);

    for(int i=0;i<macLen;i++)
    {
        if(y[i]!=cbcOut[i])
            return 0;
    }
    return 1;
    
}



/* AES-CCM with fixed L=2 and aad_len <= 30 assumption */
int aes_ccm_ae(const u8* key, size_t key_len, const u8* nonce,
               size_t M, const u8* plain, size_t plain_len,
               const u8* aad, size_t aad_len, u8* crypt, u8* auth)
{
    const size_t L = 2;
    void* aes;
    u8 x[AES_BLOCK_SIZE], a[AES_BLOCK_SIZE];

    if (aad_len > 30 || M > AES_BLOCK_SIZE)
        return -1;

    aes = aes_encrypt_init(key, key_len);

    if (aes == NULL)
        return -1;

    aes_ccm_auth_start(aes, M, L, nonce, aad, aad_len, plain_len, x);
    aes_ccm_auth(aes, plain, plain_len, x);
    /* Encryption */
    aes_ccm_encr_start(L, nonce, a);
    aes_ccm_encr(aes, L, plain, plain_len, crypt, a);
    aes_ccm_encr_auth(aes, M, x, a, auth);
    aes_encrypt_deinit(aes);
    return 0;
}


/* AES-CCM with fixed L=2 and aad_len <= 30 assumption */
int aes_ccm_ad(const u8* key, size_t key_len, const u8* nonce,
               size_t M, const u8* crypt, size_t crypt_len,
               const u8* aad, size_t aad_len, const u8* auth, u8* plain)
{
    const size_t L = 2;
    void* aes;
    u8 x[AES_BLOCK_SIZE], a[AES_BLOCK_SIZE];
    u8 t[AES_BLOCK_SIZE];

    if (aad_len > 30 || M > AES_BLOCK_SIZE)
        return -1;

    aes = aes_encrypt_init(key, key_len);

    if (aes == NULL)
        return -1;

    /* Decryption */
    aes_ccm_encr_start(L, nonce, a);
    aes_ccm_decr_auth(aes, M, a, auth, t);
    /* plaintext = msg XOR (S_1 | S_2 | ... | S_n) */
    aes_ccm_encr(aes, L, crypt, crypt_len, plain, a);
    aes_ccm_auth_start(aes, M, L, nonce, aad, aad_len, crypt_len, x);
    aes_ccm_auth(aes, plain, crypt_len, x);
    aes_encrypt_deinit(aes);

    if (memcmp(x, t, M) != 0)
    {
        return -1;
    }

    return 0;
}
