
/**
    \file cry.c

    Interface implementation file for cryptographic functions that will be used
    from other IP modules
*/

/*
    Copyright (C) 2013. Mindtree Limited.
    All rights reserved.
*/

/* --------------------------------------------- Header File Inclusion */
#include "cry.h"
#include "ll.h"
/* Platform and Internal Crypto Includes */
#include "aes_cmac.h"
#include "crypto.h"

/* Third Party Crypto Includes TODO: Temporary */
#if   defined ( __CC_ARM )
    #include "aes.h"
#endif
#include "P256-cortex-ecdh.h"
#include "rf_phy_driver.h"
/* --------------------------------------------- Global Defines */
#define BSWAP_32(x) ((uint32_t) ((((x) >> 24) & 0xff) | \
				   (((x) >> 8) & 0xff00) | \
				   (((x) & 0xff00) << 8) | \
				   (((x) & 0xff) << 24)))

/* --------------------------------------------- External Global Variables */

/* --------------------------------------------- Exported Global Variables */

/* --------------------------------------------- Static Global Variables */
/** Context for AES-CMAC */
DECL_STATIC AES_CMAC_CONTEXT cry_aescmac;

#ifndef  PHYPLUS_ECDH_ENABLE
    DECL_STATIC ssp_prkey_t cry_ecpvtkey;
    DECL_STATIC ssp_pukey_t cry_ecpubkey;

    DECL_STATIC ssp_pukey_t cry_prpubkey;
    DECL_STATIC ssp_dhkey_t cry_ecdhkey;

    DECL_STATIC void (*cry_ecdh_cb)(UCHAR*);
#endif
/* --------------------------------------------- Functions */

void cry_reverse_bytestream_endianness(UCHAR* src, UCHAR* dst, UINT16 len)
{
    UINT16 i;

    for (i = 0; i < len; i++)
    {
        dst[i] = src[(len - 1) - i];
    }
}

void cry_reverse_bytestream_in_place(UCHAR* src, UINT16 len)
{
    UINT16 i;
    UCHAR  temp;

    for (i = 0; i < len/2; i++)
    {
        temp = src[i];
        src[i] = src[(len - 1) - i];
        src[(len - 1) - i] = temp;
    }
}

INT32 cry_rand_generate(UCHAR* prand, UINT16 randlen)
{
    TRNG_Rand(prand, randlen);
    return 0;
}

INT32 cry_aes_128_encrypt
(
    UCHAR* pdata,
    UCHAR* pkey,
    UCHAR* pencdata
)
{
    LL_Encrypt (pkey, pdata, pencdata);
    return CRY_AES128_DATALEN;
}

INT32 cry_aes_128_decrypt
(
    UCHAR* pencdata,
    UCHAR* pkey,
    UCHAR* pdata
)
{
    return -1;
}

INT32 cry_aes_128_ccm_encrypt
(
    UCHAR* pkey,
    UCHAR* pnonce,
    UINT16  noncelen,
    UCHAR* pdata,
    UINT16  datalen,
    UCHAR* paddata,
    UINT16  addatalen,
    UCHAR* pencdata,
    UCHAR* pmac,
    UINT16  maclen
)
{
    UCHAR mac[CRY_AES128CMAC_MACLEN];
    INT32 ciphertext_len;
    int ret;
    (void)noncelen;
    /* Validate input length check */
    ciphertext_len = -1;
    ret = aes_ccm_ae
          (
              pkey,
              CRY_AES128_KEYLEN,
              pnonce,
              maclen,
              pdata,
              datalen,
              paddata,
              addatalen,
              pencdata,
              mac
          );

    /* TODO: Check if this is the success return */
    if (0 == ret)
    {
        ciphertext_len = datalen;

        if (NULL != pmac)
        {
            EM_mem_copy (pmac, mac, maclen);
        }
    }

    return ciphertext_len;
}

INT32 cry_aes_128_ccm_decrypt
(
    UCHAR* pkey,
    UCHAR* pnonce,
    UINT16  noncelen,
    UCHAR* pencdata,
    UINT16  encdatalen,
    UCHAR* paddata,
    UINT16  addatalen,
    UCHAR* pdata,
    UCHAR* pmac,
    UINT16  maclen
)
{
    int plaintext_len;
    int ret;
    (void)noncelen;
    /* Validate input length check */
    plaintext_len = -1;
    ret = aes_ccm_ad
          (
              pkey,
              CRY_AES128_KEYLEN,
              pnonce,
              maclen,
              pencdata,
              encdatalen,
              paddata,
              addatalen,
              pmac,
              pdata
          );

    if (0 == ret)
    {
        plaintext_len = encdatalen;
    }

    return plaintext_len;
}


INT32 cry_aes_128_cmac
(
    UCHAR   op,
    UCHAR* pdata,
    UINT16  datalen,
    UCHAR* pkey,
    UCHAR* pmac,
    UINT16  maclen
)
{
    INT32 ret;
    UCHAR mac[CRY_AES128_KEYLEN];
    EM_RESULT retval;
    UCHAR lkey[CRY_AES128_KEYLEN];
    UCHAR* ldata;
    /* Allocate local data memory */
    ldata = EM_alloc_mem (datalen);

    if (NULL == ldata)
    {
        return -1;
    }

    /* Form the data for our required endianness */
    cry_reverse_bytestream_endianness (pdata, ldata, datalen);
    cry_reverse_bytestream_endianness (pkey, lkey, CRY_AES128_KEYLEN);
    /* Initialize the AES-CMAC context */
    aes_cmac_context_init (&cry_aescmac);
    ret = -1;
    /* Populate the context for operation */
    cry_aescmac.key = lkey;
    cry_aescmac.data = ldata;
    cry_aescmac.datalen = datalen;
    cry_aescmac.mac = mac;
    cry_aescmac.maclen = sizeof(mac);
    cry_aescmac.action = (CRY_AES_128_CMAC_SIGN == op)?
                         AES_CMAC_MAC_GENERATE: AES_CMAC_MAC_VERIFY;
    /* Call to perform the operation */
    retval = aes_cmac (&cry_aescmac);

    if (EM_SUCCESS == retval)
    {
        ret = 0;
        /* Get back the input endianness */
        cry_reverse_bytestream_in_place (mac, CRY_AES128_KEYLEN);

        if (CRY_AES_128_CMAC_SIGN == op)
        {
            EM_mem_copy(pmac, mac, maclen);
        }
        else if (CRY_AES_128_CMAC_VERIFY == op)
        {
            if (0 != EM_mem_cmp(pmac, mac, maclen))
            {
                ret = -1;
            }
        }
    }

    /* Free the local data memory allocated */
    EM_free_mem (ldata);
    return ret;
}

INT32 cry_ecdh_init(void)
{
    #ifdef PHYPLUS_ECDH_ENABLE
    return (phyplus_ecdh_init());
    #else
    UINT8 ret;
    ssp_init();

    do
    {
        ret = ssp_get_ecdh_keypair(&cry_ecpvtkey, &cry_ecpubkey);
    }
    while (0 == ret);

    ssp_shutdown();
    return 0;
    #endif
}

INT32 cry_ecdh_get_public_key(UCHAR* pubkey)
{
    #ifdef PHYPLUS_ECDH_ENABLE
    return (phyplus_ecdh_get_public_key(pubkey));
    #else
    /* TODO: Check Endianness */
    cry_reverse_bytestream_endianness(cry_ecpubkey.x, pubkey, DHKEY_LEN);
    cry_reverse_bytestream_endianness(cry_ecpubkey.y, (pubkey + DHKEY_LEN), DHKEY_LEN);
    return 0;
    #endif
}

INT32 cry_ecdh_generate_secret
(
    UCHAR* peer_pubkey,
    UCHAR* secret,
    UINT16 secret_len,
    void (*ecdh_cb)(UCHAR*)
)
{
    #ifdef PHYPLUS_ECDH_ENABLE
    return (phyplus_ecdh_generate_secret(peer_pubkey,secret,secret_len));
    #else
    UINT8 ret;
    INT32 retval;
    #ifdef CRY_ECDH_TIMESLICE

    /* Validate if block is idle */
    if (NULL != cry_ecdh_cb)
    {
        return -1;
    }

    #endif /* CRY_ECDH_TIMESLICE */
    /* Reverse Endianness */
    cry_reverse_bytestream_endianness(peer_pubkey, cry_prpubkey.x, DHKEY_LEN);
    cry_reverse_bytestream_endianness((peer_pubkey + DHKEY_LEN), cry_prpubkey.y, DHKEY_LEN);
    ssp_init();
    #ifndef CRY_ECDH_TIMESLICE

    do
    {
    #endif /* CRY_ECDH_TIMESLICE */
        ret = ssp_get_dhkey
              (
                  &cry_ecpvtkey,
                  &cry_prpubkey,
                  &cry_ecdhkey
              );
        #ifndef CRY_ECDH_TIMESLICE
    }
    while (0 == ret);

        #endif /* CRY_ECDH_TIMESLICE */

    /* Check if valid dhkey */
    if (2 == ret)
    {
        /* Invalid DHKey. Return Failure */
        retval = -1;
    }

    #ifdef CRY_ECDH_TIMESLICE
    else if (0 == ret)
    {
        if (NULL == ecdh_cb)
        {
            /* Procedure cannot be completed */
            retval = -2;
        }
        else
        {
            /* Save the callback */
            cry_ecdh_cb = ecdh_cb;
            /* DHKey calculation pending. */
            retval = 1;
            /* Schedule at platform for sliced processing */
            BLE_ecdh_yield();
        }
    }

    #endif /* CRY_ECDH_TIMESLICE */
    else
    {
        /* Reverse Endianness */
        cry_reverse_bytestream_endianness(cry_ecdhkey, secret, DHKEY_LEN);
        ssp_shutdown();
        /* Valid DHKey. Return Success */
        retval = 0;
    }

    return retval;
    #endif
}

#ifdef CRY_ECDH_TIMESLICE
INT32 cry_ecdh_process_secret(void)
{
    UCHAR secret[DHKEY_LEN];
    UINT8 ret;
    INT32 retval;
    void (*ecdh_cb)(UCHAR*);

    /* Validate if block is idle */
    if (NULL == cry_ecdh_cb)
    {
        return -1;
    }

    ret = ssp_get_dhkey
          (
              &cry_ecpvtkey,
              &cry_prpubkey,
              &cry_ecdhkey
          );

    if (0 == ret)
    {
        /* DHKey calculation pending. */
        retval = 1;
        /* Schedule at platform for sliced processing */
        BLE_ecdh_yield();
    }
    else
    {
        /* Reverse Endianness */
        cry_reverse_bytestream_endianness(cry_ecdhkey, secret, DHKEY_LEN);
        ssp_shutdown();
        /* Valid DHKey. Return Success */
        retval = 0;
        /* Invoke the corresponding callback */
        ecdh_cb = cry_ecdh_cb;
        cry_ecdh_cb = NULL;
        ecdh_cb (secret);
    }

    return retval;
}
#endif /* CRY_ECDH_TIMESLICE */
INT32 cry_set_ecdh_debug_keypair(UCHAR* pvtkey, UCHAR* pubkey)
{
    (void)pvtkey;
    (void)pubkey;
    return -1;
}

#define PHY_PLUS_ECDH_KEYGEN_RETRY_LIMT 64

int cry_generate_random_bytes(uint8* p, uint32_t size)
{
    LL_Rand(p,(uint8)size);
    return 0;
}

DECL_STATIC ssp_prkey_t phy_ecpvtkey;
DECL_STATIC ssp_pukey_t phy_ecpubkey;
DECL_STATIC uCrypto_RNG_Function g_rng_function = 0;

void uCrypto_set_rng(uCrypto_RNG_Function rng_function)
{
    g_rng_function = rng_function;
}
uCrypto_RNG_Function uCrypto_get_rng(void)
{
    return g_rng_function;
}

/* based on Core Specification 4.2 Vol 3. Part H 2.3.5.6.1 */
// uint8_t debug_private_key_be[32] = {
// 	0x3f, 0x49, 0xf6, 0xd4, 0xa3, 0xc5, 0x5f, 0x38,
// 	0x74, 0xc9, 0xb3, 0xe3, 0xd2, 0x10, 0x3f, 0x50,
// 	0x4a, 0xff, 0x60, 0x7b, 0xeb, 0x40, 0xb7, 0x99,
// 	0x58, 0x99, 0xb8, 0xa6, 0xcd, 0x3c, 0x1a, 0xbd,
// };

void public_key_copy(uint8 *publicKeyX,uint8 *publicKeyY,uint8 *privateKey)
{
    EM_mem_copy(publicKeyX,phy_ecpubkey.x,DHKEY_LEN);
    EM_mem_copy(publicKeyY,phy_ecpubkey.y,DHKEY_LEN);
    EM_mem_copy(privateKey,phy_ecpvtkey,DHKEY_LEN);

}
INT32 phyplus_ecdh_init(void)
{
//    uint32 T1;
    uint8 my_public_point0[64], my_private_key0[32];
    uint32 cnt=0;
    uCrypto_RNG_Function gen_rng;

    if(uCrypto_get_rng()==0)
        uCrypto_set_rng(cry_generate_random_bytes);

    gen_rng =uCrypto_get_rng();

    do
    {
//        #warning "ecdh shift msb and lsb value to x01,make sure value blelow r/2"

        gen_rng(my_private_key0, 32);

//        debug_private_key_be[0]=0x01;
//        debug_private_key_be[31]=0x01;
		// cry_reverse_bytestream_endianness(debug_private_key_be,my_private_key0,DHKEY_LEN);
        if(++cnt>PHY_PLUS_ECDH_KEYGEN_RETRY_LIMT)
            return cnt;
    }
    while (!P256_ecdh_keygen(my_public_point0, my_private_key0));

//
    EM_mem_copy(phy_ecpvtkey,my_private_key0,DHKEY_LEN);
    EM_mem_copy(phy_ecpubkey.x,my_public_point0,DHKEY_LEN);
    EM_mem_copy(phy_ecpubkey.y,my_public_point0+DHKEY_LEN,DHKEY_LEN);
    return 0;
}

INT32 gfp_ecdh_init(void)
{
//    uint32 T1;
    uint8 my_public_point0[64], my_private_key0[32];
    // extern const uint8_t m_spoofing_priv_key[];
    
    // osal_memcpy( my_private_key0, m_spoofing_priv_key, 32);
    P256_ecdh_keygen(my_public_point0, my_private_key0);

//
    EM_mem_copy(phy_ecpvtkey,my_private_key0,DHKEY_LEN);
    EM_mem_copy(phy_ecpubkey.x,my_public_point0,DHKEY_LEN);
    EM_mem_copy(phy_ecpubkey.y,my_public_point0+DHKEY_LEN,DHKEY_LEN);
    LOG("copy key init.");
    LOG("my priv key:");
    LOG_DUMP_BYTE( phy_ecpvtkey, DHKEY_LEN);
    LOG("my key x:");
    LOG_DUMP_BYTE( phy_ecpubkey.x, DHKEY_LEN);
    LOG("my key y:");
    LOG_DUMP_BYTE( phy_ecpubkey.y, DHKEY_LEN);
    LOG("my key copy end.");

//    cry_reverse_bytestream_endianness(my_private_key0,cry_ecpvtkey,DHKEY_LEN);
//
//    cry_reverse_bytestream_endianness(my_public_point0,cry_ecpubkey.x,DHKEY_LEN);
//    cry_reverse_bytestream_endianness(my_public_point0+DHKEY_LEN,cry_ecpubkey.y,DHKEY_LEN);
    return 0;
}

INT32 phyplus_ecdh_generate_secret(UCHAR* peer_pubkey, UCHAR* secret, UINT16 secret_len)
{
    uint8 pubkey[64];
    uint8 dhkey[32];
    /* TODO: Check Endianness */
    cry_reverse_bytestream_endianness(peer_pubkey, pubkey, DHKEY_LEN);
    cry_reverse_bytestream_endianness((peer_pubkey + DHKEY_LEN), (pubkey+DHKEY_LEN), DHKEY_LEN);

    if (!P256_ecdh_shared_secret(dhkey, pubkey, phy_ecpvtkey))
    {
        // The other part sent an invalid public point, so abort
        return 1;
    }
    else
    {
        cry_reverse_bytestream_endianness(dhkey, secret, DHKEY_LEN);
        // The shared_secret is now the same for both parts and may be used for cryptographic purposes
        return 0;
    }
}

INT32 phyplus_ecdh_get_public_key(UCHAR* pubkey)
{
    /* TODO: Check Endianness */
    cry_reverse_bytestream_endianness(phy_ecpubkey.x, pubkey, DHKEY_LEN);
    cry_reverse_bytestream_endianness(phy_ecpubkey.y, (pubkey + DHKEY_LEN), DHKEY_LEN);
//    EM_mem_copy(pubkey,             phy_ecpubkey.x,DHKEY_LEN);
//    EM_mem_copy(pubkey+DHKEY_LEN,phy_ecpubkey.y,DHKEY_LEN);
    return 0;
}

//#define cry_dump_key(a,b,c)     do{cry_printf(a);for(int i=0;i<b;i++){cry_printf("%02x ",c[i]);}cry_printf("\n");}while(0)
#if 0
void test_verify_phyplus_ecdh(void)
{
    UCHAR pubkey0[64];
    UCHAR pubkey1[64];
    UCHAR pubKeyT[64];
    UCHAR sec0[32];
    UCHAR sec1[32];
    int ret;
    ret=cry_ecdh_init();
    //cry_printf(" cry ini %d\n",ret);
    ret=phyplus_ecdh_init();
    //cry_printf(" phy ini %d\n",ret);
    ret=cry_ecdh_get_public_key(pubKeyT);
    cry_reverse_bytestream_endianness(pubKeyT,pubkey0,DHKEY_LEN);
    cry_reverse_bytestream_endianness(pubKeyT+DHKEY_LEN,pubkey0+DHKEY_LEN,DHKEY_LEN);
    ret=phyplus_ecdh_get_public_key(pubKeyT);
    cry_reverse_bytestream_endianness(pubKeyT,pubkey1,DHKEY_LEN);
    cry_reverse_bytestream_endianness(pubKeyT+DHKEY_LEN,pubkey1+DHKEY_LEN,DHKEY_LEN);
    ret=cry_ecdh_generate_secret(pubkey1, sec0, 32, NULL);
    //cry_printf(" cry sec %d\n",ret);
    ret=phyplus_ecdh_generate_secret(pubkey0, sec1, 32);
    //cry_printf(" phy sec %d\n",ret);
//    cry_dump_key("[cry_pvt]\n",32,cry_ecpvtkey);
//    cry_dump_key("[cry_pub_x]\n",32,cry_ecpubkey.x);
//    cry_dump_key("[cry_pub_y]\n",32,cry_ecpubkey.y);
//
//    cry_dump_key("[phy_pvt]\n",32,phy_ecpvtkey);
//    cry_dump_key("[phy_pub_x]\n",32,phy_ecpubkey.x);
//    cry_dump_key("[phy_pub_y]\n",32,phy_ecpubkey.y);
//
//    cry_dump_key("[pubKey0]\n",64,pubkey0);
//    cry_dump_key("[pubKey1]\n",64,pubkey1);
//
//    cry_dump_key("[sec0]\n",32,sec0);
//    cry_dump_key("[sec1]\n",32,sec1);
//    cry_printf("[sec0] \n");
//    for(int i=0;i<DHKEY_LEN;i++)
//    {
//        cry_printf("%02x ",sec0[i]);
//    }
//    cry_printf("[sec1] \n");
//    for(int i=0;i<DHKEY_LEN;i++)
//    {
//        cry_printf("%02x ",sec1[i]);
//    }
}

#endif

//void aes_cmac_cbc_mic(uint8_t* key,uint8_t* din, uint32_t dLen,uint8_t* cbcOut,uint8_t cbcLen)
int bt_crypto_f4(uint8_t *u, uint8_t *v, uint8_t *x, uint8_t z, uint8_t res[16])
{
	uint8_t xs[16];
	uint8_t cmac_out[16];
	uint8_t m[65];

	/*
	 * U, V and Z are concatenated and used as input m to the function
	 * AES-CMAC and X is used as the key k.
	 *
	 * Core Spec 4.2 Vol 3 Part H 2.2.5
	 *
	 * note:
	 * bt_smp_aes_cmac uses BE data and smp_f4 accept LE so we swap
	 */
	cry_reverse_bytestream_endianness(u,m,32);
	cry_reverse_bytestream_endianness(v,m+32,32);
	m[64] = z;
	
	cry_reverse_bytestream_endianness(x,xs,16);
	aes_cmac_cbc_mic(xs,m,65,cmac_out,16);

	cry_reverse_bytestream_endianness(cmac_out,res,16);

	return 0;
}

// a1[0]:device a1 addr type
// a2[0]:device a2 addr type
int bt_crypto_f5(	uint8_t *w, uint8_t *n1, uint8_t *n2, uint8_t *a1,
		 				uint8_t *a2, uint8_t *mackey, uint8_t *ltk)
{
	uint8_t salt[16] = {0x6c, 0x88, 0x83, 0x91, 0xaa, 0xf5, 0xa5, 0x38,
					 0x60, 0x37, 0x0b, 0xdb, 0x5a, 0x60, 0x83, 0xbe};
	uint8_t m[53] = {0x00,						 /* counter */
			 0x62, 0x74, 0x6c, 0x65,			 /* keyID */
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*n1*/
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*2*/
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* a1 */
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* a2 */
			 0x01, 0x00 /* length */};
	uint8_t t[16], ws[32],tmp[16];

	cry_reverse_bytestream_endianness(w,ws,32);
	aes_cmac_cbc_mic(salt,ws,32,t,16);
	cry_reverse_bytestream_endianness(n1,m+5,16);
	cry_reverse_bytestream_endianness(n2,m+21,16);

	m[37] = a1[0];
	cry_reverse_bytestream_endianness(&a1[1],m + 38,6);
	m[44] = a2[0];
	cry_reverse_bytestream_endianness(&a2[1],m + 45,6);

	// mackey generate
	aes_cmac_cbc_mic(t,m,sizeof(m),tmp,16);
	cry_reverse_bytestream_endianness(tmp,mackey,16);

	// ltk generate	
	/* counter for ltk is 1 */
	m[0] = 0x01;
	aes_cmac_cbc_mic(t,m,sizeof(m),tmp,16);
	cry_reverse_bytestream_endianness(tmp,ltk,16);
	return 0;
}

int bt_crypto_f6(uint8_t *w, uint8_t *n1, uint8_t *n2, uint8_t *r,
						 uint8_t *iocap, uint8_t *a1, uint8_t *a2,
						 uint8_t *check)
{
	uint8_t ws[16],ex[16];
	uint8_t m[65];

	cry_reverse_bytestream_endianness(n1,m,16);
	cry_reverse_bytestream_endianness(n2,m+16,16);
	cry_reverse_bytestream_endianness(r,m+32,16);
	cry_reverse_bytestream_endianness(iocap,m+48,3);


	m[51] = a1[0];
	memcpy(m + 52, &a1[1], 6);
	cry_reverse_bytestream_endianness(&a1[1],m + 52,6);

	m[58] = a2[0];
	memcpy(m + 59, &a2[1], 6);
	cry_reverse_bytestream_endianness(&a2[1],m + 59,6);

	cry_reverse_bytestream_endianness(w, ws, 16);

	aes_cmac_cbc_mic(ws,m,sizeof(m),ex,16);
	
	cry_reverse_bytestream_endianness(ex,check, 16);

	return 0;
}

int bt_crypto_g2(uint8_t u[32], uint8_t v[32], uint8_t x[16], uint8_t y[16],
		 uint32_t *passkey)
{
	uint8_t m[80], xs[16];
	cry_reverse_bytestream_endianness(u,m,32);
	cry_reverse_bytestream_endianness(v,m+32,32);
	cry_reverse_bytestream_endianness(y,m+64,16);
	cry_reverse_bytestream_endianness(x,xs,16);

	/* reuse xs (key) as buffer for result */
	aes_cmac_cbc_mic(xs,m,sizeof(m),xs,16);
	
	memcpy(passkey, xs + 12, 4);
	*passkey = BSWAP_32(*passkey) % 1000000;

	return 0;
}

//int bt_crypto_h6(const uint8_t w[16], const uint8_t key_id[4], uint8_t res[16])
//{
////	uint8_t ws[16];
////	uint8_t key_id_s[4];
////	int err;
////
////	LOG_DBG("w %s", bt_hex(w, 16));
////	LOG_DBG("key_id %s", bt_hex(key_id, 4));
////
////	sys_memcpy_swap(ws, w, 16);
////	sys_memcpy_swap(key_id_s, key_id, 4);
////
////	err = bt_crypto_aes_cmac(ws, key_id_s, 4, res);
////	if (err) {
////		return err;
////	}
////
////	LOG_DBG("res %s", bt_hex(res, 16));
////
////	sys_mem_swap(res, 16);

//	return 0;
//}

//int bt_crypto_h7(const uint8_t salt[16], const uint8_t w[16], uint8_t res[16])
//{
////	uint8_t ws[16];
////	uint8_t salt_s[16];
////	int err;
////
////	LOG_DBG("w %s", bt_hex(w, 16));
////	LOG_DBG("salt %s", bt_hex(salt, 16));
////
////	sys_memcpy_swap(ws, w, 16);
////	sys_memcpy_swap(salt_s, salt, 16);
////
////	err = bt_crypto_aes_cmac(salt_s, ws, 16, res);
////	if (err) {
////		return err;
////	}
////
////	LOG_DBG("res %s", bt_hex(res, 16));
////
////	sys_mem_swap(res, 16);

//	return 0;
//}

//int bt_crypto_h8(const uint8_t k[16], const uint8_t s[16], const uint8_t key_id[4], uint8_t res[16])
//{
////	uint8_t key_id_s[4];
////	uint8_t iks[16];
////	uint8_t ks[16];
////	uint8_t ss[16];
////	int err;
////
////	LOG_DBG("k %s", bt_hex(k, 16));
////	LOG_DBG("s %s", bt_hex(s, 16));
////	LOG_DBG("key_id %s", bt_hex(key_id, 4));
////
////	sys_memcpy_swap(ks, k, 16);
////	sys_memcpy_swap(ss, s, 16);
////
////	err = bt_crypto_aes_cmac(ss, ks, 16, iks);
////	if (err) {
////		return err;
////	}
////
////	sys_memcpy_swap(key_id_s, key_id, 4);
////
////	err = bt_crypto_aes_cmac(iks, key_id_s, 4, res);
////	if (err) {
////		return err;
////	}
////
////	LOG_DBG("res %s", bt_hex(res, 16));
////
////	sys_mem_swap(res, 16);

//	return 0;
//}



