/*
// encAudio.h
//   encryption of data for export to Amplio
//
 */

#include <stdio.h>
#include <time.h>
#include <string.h>

#include "mbedtls/aes.h"
#include "mbedtls/pk.h"
#include "mbedtls/base64.h"
#include "mbedtls/error.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "encAudio.h"
#include "tbook.h"

const int      KEY_BYTES      = AES_KEY_WDS * 4;
const int      SESS_SIZE      = ( AES_KEY_WDS + 1 ) * 4;  // including 'Sess'
const int      AES_BLOCK_SIZE = 16;
const uint32_t SessMARKER     = 'S' | 'e' << 8 | 's' << 16 | 's' << 24;

encryptState_t Enc;                         // audioEncrypt state record
static uint8_t buff1[BUFF_SIZ];
static uint8_t buff2[BUFF_SIZ];

void TlsErr( char *msg, int cd ) {
    char ebuf[100];
    mbedtls_strerror( cd, ebuf, sizeof( ebuf ));
    tbErr( "tlsErr: %s => %s", msg, ebuf );
}

#if defined( STM32F412Vx )
#include "stm32f412vx.h"

void initRandom( char *pers ) {   // reset RNG before encrypt or decrypt
    mbedtls_ctr_drbg_init( &Enc.drbg );
    int ret = mbedtls_ctr_drbg_seed( &Enc.drbg, genRand, &Enc.entr, (const unsigned char *) pers, strlen( pers ));
    if ( ret != 0 ) TlsErr( "drbg_seed", ret );
}

int genRand( void *pRNG, unsigned char *output, size_t out_len ) {         // fill output with true random data from STM32F412 RNG dev
    int cnt = 0;

    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;      // enable RNG peripheral clock
    // RNG->CR |= RNG_CR_IE;  // polling, no interrupts
    RNG->CR |= RNG_CR_RNGEN;    // enable RNG

    if ( RNG->SR & ( RNG_SR_SECS | RNG_SR_CECS )) { // if SR.SEIS or CEIS -- seed or clock error
        __breakpoint( 0 );
        return -1;
    }

    uint32_t *outwds = (uint32_t *) output;
    size_t   wdsz    = out_len / 4, bytsz = out_len - wdsz * 4;
    for (int i       = 0; i <= wdsz; i++) {
        while (( RNG->SR & RNG_SR_DRDY ) == 0) // wait till ready
            cnt++;

        uint32_t rnd = RNG->DR;
        if ( i < wdsz )
            outwds[i] = rnd;
        else if ( bytsz > 0 ) {
            for (int j = 0; j < bytsz; j++)
                output[i * 4 + j] = ( rnd >> j * 8 ) & 0xFF;
        }
    }
    return 0;
}

#else

void initRandom( char *pers ) {   // reset RNG before encrypt or decrypt
    mbedtls_entropy_init( &Enc.entr );
    mbedtls_ctr_drbg_init( &Enc.drbg );
    int ret = mbedtls_ctr_drbg_seed( &Enc.drbg, mbedtls_entropy_func, &Enc.entr, (const unsigned char *) pers, strlen( pers ));
    if ( ret != 0 ) TlsErr( "drbg_seed", ret );
}

int genRand( void *pRNG, unsigned char *output, size_t out_len ) {         // fill output with true random data from STM32F412 RNG dev
    int ret = mbedtls_ctr_drbg_random( &Enc.drbg, output, out_len );
    if ( ret != 0 ) TlsErr( "drbg_rand", ret );
    return 0;
}

#endif

// EncryptAudio:  startEncrypt, encryptBlock, endEncrypt
void startEncrypt( char *fname ) {   // init session key & save public encrypted copy in 'fname.key'
    FileSysPower( true );  // make sure FS is powered
    initRandom( "TBook entropy generation seed for STM32F412" );
    uint8_t *sess_key = ( uint8_t * ) & Enc.sessionkey;

    int res = genRand( NULL, sess_key, KEY_BYTES );
    if ( res != 0 ) TlsErr( "RNG gen", res );
    Enc.sessionkey[6] = SessMARKER;
    memset( &Enc.iv, 0, sizeof( Enc.iv ));

    mbedtls_aes_init( &Enc.aes );
    mbedtls_aes_setkey_enc( &Enc.aes, sess_key, 192 );
    if ( res != 0 ) TlsErr( "AesSetkey", res );
    Enc.initialized = true;

    mbedtls_pk_init( &Enc.pk );
    // @formatter:off
    uint8_t amplioPublicKey[] = { // amplio_pub.der
            0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01,   // 00000000:
            0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00, 0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01,   // 00000010:
            0x00, 0xBB, 0x8B, 0xBD, 0x74, 0x1D, 0x67, 0x10, 0x83, 0x07, 0x8E, 0x40, 0x83, 0x8C, 0x30, 0x92,   // 00000020:
            0x4F, 0xDD, 0x96, 0x7C, 0xDF, 0x81, 0x9A, 0xEB, 0x40, 0x08, 0xCF, 0x03, 0xFB, 0xCE, 0xE1, 0xF0,   // 00000030:
            0x0D, 0x33, 0x17, 0xB0, 0x4B, 0x48, 0x24, 0xE6, 0xCF, 0x66, 0xEB, 0x4D, 0xDD, 0x8D, 0x25, 0xED,   // 00000040:
            0xBA, 0x0B, 0x8B, 0xDF, 0x46, 0x15, 0x9A, 0xCE, 0xEF, 0xA7, 0x04, 0x00, 0x02, 0x9F, 0x14, 0xA7,   // 00000050:
            0x06, 0xE4, 0xE1, 0xE5, 0x21, 0x8D, 0x8D, 0x08, 0x31, 0x7E, 0xD6, 0xA8, 0x5F, 0xFF, 0x9A, 0xC3,   // 00000060:
            0xA6, 0xB8, 0x33, 0xE6, 0xEA, 0x21, 0x8D, 0x4F, 0xB7, 0x9B, 0xFC, 0x32, 0x09, 0x8E, 0x50, 0xE1,   // 00000070:
            0xB7, 0x8A, 0x93, 0xD5, 0x0A, 0xE1, 0x96, 0x86, 0xED, 0x4C, 0x93, 0x6F, 0xEF, 0x85, 0x51, 0x31,   // 00000080:
            0x90, 0xA9, 0xB2, 0xA3, 0x1D, 0x05, 0x57, 0x9F, 0x15, 0xEE, 0x2D, 0xFA, 0xE9, 0x55, 0xCB, 0xC5,   // 00000090:
            0x17, 0xDC, 0xDE, 0x85, 0xBE, 0xA9, 0x98, 0x1C, 0x7C, 0x33, 0x2E, 0x54, 0x0E, 0x3F, 0x4F, 0xDC,   // 000000a0:
            0xCF, 0xC5, 0x25, 0x41, 0xB4, 0x78, 0x96, 0x03, 0x0D, 0xB2, 0x0A, 0xFE, 0x93, 0xA8, 0xE2, 0x29,   // 000000b0:
            0xDF, 0x9A, 0x5D, 0x06, 0xEE, 0xC4, 0x43, 0xD4, 0x06, 0x44, 0x63, 0x40, 0x73, 0xE9, 0x0C, 0x0A,   // 000000c0:
            0xCA, 0x51, 0xBD, 0x13, 0xEF, 0x74, 0x0C, 0xE3, 0x45, 0x18, 0x74, 0x3E, 0xBF, 0x1C, 0xB2, 0x48,   // 000000d0:
            0x1B, 0xE2, 0x95, 0x3D, 0x79, 0xCF, 0xC5, 0xBA, 0x18, 0xE5, 0xCB, 0x66, 0xB6, 0x84, 0x90, 0xE3,   // 000000e0:
            0xB2, 0x68, 0x1C, 0x30, 0xCE, 0xA4, 0x0C, 0xC1, 0xE7, 0x0D, 0x03, 0x70, 0x99, 0x22, 0xC3, 0xAD,   // 000000f0:
            0x5C, 0xB4, 0xD7, 0x08, 0x25, 0x9E, 0x44, 0x89, 0x3F, 0x7D, 0xD9, 0x74, 0xA6, 0x28, 0x20, 0x6D,   // 00000100:
            0x86, 0xC4, 0xD6, 0x6E, 0x94, 0x63, 0xBF, 0xBA, 0xC6, 0x72, 0xBA, 0xED, 0xF3, 0x42, 0xC8, 0x59,   // 00000110:
            0x7F, 0x02, 0x03, 0x01, 0x00, 0x01                                  // 00000120:
            // amplio_pub.pem:
            //-----BEGIN PUBLIC KEY-----
            //MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAu4u9dB1nEIMHjkCDjDCS
            //T92WfN+BmutACM8D+87h8A0zF7BLSCTmz2brTd2NJe26C4vfRhWazu+nBAACnxSn
            //BuTh5SGNjQgxftaoX/+aw6a4M+bqIY1Pt5v8MgmOUOG3ipPVCuGWhu1Mk2/vhVEx
            //kKmyox0FV58V7i366VXLxRfc3oW+qZgcfDMuVA4/T9zPxSVBtHiWAw2yCv6TqOIp
            //35pdBu7EQ9QGRGNAc+kMCspRvRPvdAzjRRh0Pr8cskgb4pU9ec/Fuhjly2a2hJDj
            //smgcMM6kDMHnDQNwmSLDrVy01wglnkSJP33ZdKYoIG2GxNZulGO/usZyuu3zQshZ
            //fwIDAQAB
            //-----END PUBLIC KEY-----
    };
    // @formatter:on


    res = mbedtls_pk_parse_public_key( &Enc.pk, amplioPublicKey, sizeof( amplioPublicKey ));
    if ( res != 0 ) TlsErr( "pubDecode", res );

    uint8_t *enc_sess_key = buff1;
    memset( enc_sess_key, 0, BUFF_SIZ );

    uint8_t *enc_sess_b64 = buff2;
    memset( enc_sess_b64, 0, BUFF_SIZ );
    size_t enc_len = 0;

    res = mbedtls_pk_encrypt( &Enc.pk, sess_key, SESS_SIZE, enc_sess_key, &enc_len, BUFF_SIZ, genRand, NULL );
    if ( res < 0 ) TlsErr( "RsaEnc", enc_len );

    size_t b64_len = BUFF_SIZ;
    res = mbedtls_base64_encode( enc_sess_b64, BUFF_SIZ, &b64_len, enc_sess_key, enc_len );
    if ( res != 0 ) TlsErr( "b64enc", res );

    char txtnm[40];
    sprintf( txtnm, "%s.key", fname );
    FILE *f = tbOpenWrite( txtnm ); //fopen( txtnm, "w" );
    fwrite( enc_sess_b64, 1, b64_len, f );
    tbFclose( f );  //res = fclose( f );
    //if ( res != 0 ) tbErr(".key fclose => %d", res );
}

void encryptBlock( const uint8_t *in, uint8_t *out, int len ) {   // AES CBC encrypt in[0..len] => out[] (len%16===0)
    if ( !Enc.initialized ) tbErr( "encrypt not init" );
    if ( len % AES_BLOCK_SIZE != 0 )
        tbErr( "encBlock len not mult of %d", AES_BLOCK_SIZE );

    int res = mbedtls_aes_crypt_cbc( &Enc.aes, MBEDTLS_AES_ENCRYPT, len, ( uint8_t * ) & Enc.iv, in, out );
    if ( res != 0 ) tbErr( "CbcEnc => %d", res );
}

void endEncrypt() {
    // clear session key
    memset( &Enc.sessionkey, 0, sizeof( Enc.sessionkey ));
    memset( &Enc.iv, 0, sizeof( Enc.iv ));
}
