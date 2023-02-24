/*
// encAudio.h
//   encryption of data for export to Amplio
//
 */

#include <arm_compat.h>

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
#include "fileOps.h"
#include "tbook.h"

static const int AES_KEY_BITS = 256;
static const int AES_KEY_LENGTH = AES_KEY_BITS / 8;
static const char *DER_FILE_NAME = "/system/uf.der";

static const int AES_BLOCK_SIZE = 16;

bool encUfAudioEnabled = false;

struct EncryptCb encryptCb;
struct EncryptCb *pEncryptCb = &encryptCb;

typedef struct EncryptState {
    bool initialized;
    bool enabled;
    mbedtls_aes_context aes_context;
    uint8_t iv[16];
    mbedtls_pk_context pk_context;
    mbedtls_ctr_drbg_context drbg_context;
    mbedtls_entropy_context entropy_context;
    char keyFileName[MAX_PATH];
} EncryptState_t;
EncryptState_t encryptState;

void startEncrypt(char *fname );
void encryptBlock( const uint8_t *in, uint8_t *out, int len );
void endEncrypt( size_t fSize );

void TlsErr(char *msg, int cd) {
    char ebuf[100];
    mbedtls_strerror(cd, ebuf, sizeof(ebuf));
    tbErr("tlsErr: %s => %s", msg, ebuf);
}

//region getRandom
#if defined( STM32F412Vx )
#include "stm32f412vx.h"

int getRandom( void *pRNG, unsigned char *pBuffer, size_t buffer_len );
void initRandom( char *pers ) {   // reset RNG before encrypt or decrypt
    mbedtls_ctr_drbg_init( &encryptState.drbg_context );
    int ret = mbedtls_ctr_drbg_seed( &encryptState.drbg_context, getRandom, &encryptState.entropy_context, (const unsigned char *) pers, strlen( pers ));
    if ( ret != 0 ) TlsErr( "drbg_seed", ret );
}

/**
 * Gets random bytes. The prototype is defined by mbed, so we hae the pRNG parameter, even though we don't need
 * it here.
 * @param pRNG  Unused
 * @param pBuffer The buffer to receive the random bytes.
 * @param buffer_len sizeof(buffer)
 * @return 0 on success, -1 on error.
 */
int getRandom( void *pRNG, unsigned char *pBuffer, size_t buffer_len ) {         // fill pBuffer with true random data from STM32F412 RNG dev
    int cnt = 0;

    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;      // enable RNG peripheral clock
    // RNG->CR |= RNG_CR_IE;  // polling, no interrupts
    RNG->CR |= RNG_CR_RNGEN;    // enable RNG

    if ( RNG->SR & ( RNG_SR_SECS | RNG_SR_CECS )) { // if SR.SEIS or CEIS -- seed or clock error
        __breakpoint( 0 );
        return -1;
    }

    uint32_t *pWords = (uint32_t *) pBuffer;
    size_t   numWords    = buffer_len / 4;
    size_t   extraBytes = buffer_len - numWords * 4;
    for (int i       = 0; i <= numWords; i++) {
        // spin 'till ready
        while (( RNG->SR & RNG_SR_DRDY ) == 0) 
            cnt++;

        uint32_t random = RNG->DR; // next 4 bytes of random bits
        if ( i < numWords ) {
            pWords[i] = random;
        } else if ( extraBytes > 0 ) {
            for (int j = 0; j < extraBytes; j++)
                pBuffer[i * 4 + j] = ( random >> j * 8 ) & 0xFF;
        }
    }
    return 0;
}

#else

void initRandom(char *pers) {   // reset RNG before encrypt or decrypt
    mbedtls_entropy_init(&encryptState.entropy_context);
    mbedtls_ctr_drbg_init(&encryptState.drbg_context);
    int ret = mbedtls_ctr_drbg_seed(&encryptState.drbg_context, mbedtls_entropy_func, &encryptState.entropy_context,
                                    (const unsigned char *) pers, strlen(pers));
    if (ret != 0) TlsErr("drbg_seed", ret);
}

int getRandom(void *pRNG, unsigned char *pBuffer, size_t buffer_len) {         // fill pBuffer with true random data from STM32F412 RNG dev
    int ret = mbedtls_ctr_drbg_random(&encryptState.drbg_context, pBuffer, buffer_len);
    if (ret != 0) TlsErr("drbg_rand", ret);
    return 0;
}

#endif
//endregion


void encUfAudioInit() {
    if (encryptState.initialized) {
        return;
    }
    encryptState.initialized = true;
    int derSize = fsize(DER_FILE_NAME);
    if (derSize > 0) {
        FILE *derFile = tbFopen(DER_FILE_NAME, "rb");
        if (derFile == NULL) {
            return;
        }
        uint8_t *derBytes = tbAlloc(derSize, "der");
        int result = fread(derBytes, 1, derSize, derFile);
        tbFclose(derFile);
        if (result == derSize) {
            // We have a public key, so initialize the RNG and the PK encryption system.
            initRandom("TBook entropy generation seed for STM32F412");
            mbedtls_pk_init(&encryptState.pk_context);
            result = mbedtls_pk_parse_public_key(&encryptState.pk_context, derBytes, derSize);
            encUfAudioEnabled = encryptState.enabled = (result == 0);
            if (!encryptState.enabled) {
                // log that we couldn't parse the .der file.
            } else {
            };
        }
        free(derBytes);
    } 
}

#define STATIC_ENCRYPT_BUFFER 0

void encUfAudioLoop() {
    osMutexAcquire(fileOpMutex, osWaitForever);
#if STATIC_ENCTYPE_BUFFER
    static uint8_t encrypted[2048];
#else
    uint8_t *encrypted = tbAlloc(2048, "der");
#endif
    char basename[MAX_PATH];
    strcpy( basename, pEncryptCb->fname );   // copy *.wav path
    char *pdot = strrchr( basename, '.' );
    *pdot = 0;

    logEvtFmt( "startEnc", "%s", basename );

    startEncrypt( basename);       // write basenm.key & initialize AES encryption
    strcat( basename, ".enc" );
    FILE *fEncrypted = tbOpenWriteBinary( basename );

    int nWritten = 0;
    uint32_t waitFlags = kAnyEncryptRequest;
    while (1) {
        uint32_t eventFlags = osEventFlagsWait(fileOpRequestEventId, waitFlags, osFlagsWaitAny, osWaitForever);
        Buffer_t *pBuffer;
        while (osMessageQueueGet(pEncryptCb->encryptQueueId, &pBuffer, 0, 0) == osOK) {
            encryptBlock((const uint8_t *)pBuffer, encrypted, 2048);
            int result = fwrite(encrypted, 1, 2048, fEncrypted);
            releaseBuffer(pBuffer);
            nWritten++;
        }
        if (eventFlags & kEncryptFinishRequest) {
            endEncrypt(pEncryptCb->fileSize);
            tbFclose(fEncrypted);
            break;
        }
    }
#if !STATIC_ENCRYPT_BUFFER
    free(encrypted);
#endif
    osEventFlagsSet(fileOpResultEventId, kEncryptResult);
    osMutexRelease(fileOpMutex);
}

// EncryptAudio:  startEncrypt, encryptBlock, endEncrypt
/**
 * Prepare to encrypt a file.
 * @param fname Base name of file being encrypted. ${fname}.key will get the key, iv, and (eventually) length.
 */
void startEncrypt(char *fname) {
    FileSysPower(true);

    // Allocate the key and iv for this encryption session, and initialize mbed_aes
    uint8_t session_key[AES_KEY_LENGTH];
    int result = getRandom(NULL, session_key, AES_KEY_LENGTH);
    if (result != 0) TlsErr("RNG gen", result);
    result = getRandom(NULL, encryptState.iv, sizeof(encryptState.iv));
    if (result != 0) TlsErr("RNG gen", result);
    mbedtls_aes_init(&encryptState.aes_context);
    result = mbedtls_aes_setkey_enc(&encryptState.aes_context, session_key, AES_KEY_BITS);
    if (result != 0) TlsErr("AesSetkey", result);

    // Encrypt the session key using the public key.
    static uint8_t encrypted_session_key[256];
    size_t encrypted_key_length;
    result = mbedtls_pk_encrypt(&encryptState.pk_context, session_key, AES_KEY_LENGTH, encrypted_session_key, &encrypted_key_length, sizeof(encrypted_session_key), getRandom, NULL);
    if (result != 0) TlsErr("pkEncrypt", result);

    // Create the key file.
    sprintf(encryptState.keyFileName, "%s.key", fname);
    FILE *f = tbFopen(encryptState.keyFileName, "w");

    // Encode the encrypted session key as base64, and write to the key file.
    // It's wrong that this is 'unsigned char', because it receives character data, but that's how mbed typed it.
    static uint8_t base64_encoded[512]; 
    size_t base64_encoded_length;
    result = mbedtls_base64_encode(base64_encoded, sizeof(base64_encoded), &base64_encoded_length, encrypted_session_key, encrypted_key_length);
    if (result != 0) TlsErr("b64Encode", result);
    fwrite(base64_encoded, 1, base64_encoded_length, f);

    // Encode the iv as base64, and write to the key file.
    result = mbedtls_base64_encode(base64_encoded, sizeof(base64_encoded), &base64_encoded_length, encryptState.iv, sizeof(encryptState.iv));
    if (result != 0) TlsErr("b64Encode", result);
    fwrite("\n", 1, strlen("\n"), f);
    fwrite(base64_encoded, 1, base64_encoded_length, f);

    tbFclose(f);  //res = fclose( f );
}

/**
 * Encrypt the next block of data.
 * @param in plain text data.
 * @param out encrypted data.
 * @param len Length of the data. Must be a multiple of key size (32 bytes).
 */
void encryptBlock(const uint8_t *in, uint8_t *out, int len) {   // AES CBC encrypt in[0..len] => out[] (len%16===0)
    if (!encryptState.initialized) tbErr("encrypt not init");
    if (len % AES_BLOCK_SIZE != 0)
        tbErr("encBlock len not mult of %d", AES_BLOCK_SIZE);

    int res = mbedtls_aes_crypt_cbc(&encryptState.aes_context, MBEDTLS_AES_ENCRYPT, len, (uint8_t * ) & encryptState.iv, in,
                                    out);
    if (res != 0) tbErr("CbcEnc => %d", res);
}

/**
 * End the encryption and add the file length to the .key file.
 * @param fSize of the data (encrypted or plain text). Will be appended to the .key file.
 */
void endEncrypt(size_t fSize) {
    // clear session state
    memset(&encryptState.iv, 0, sizeof(encryptState.iv));
    memset(&encryptState.aes_context, 0, sizeof(encryptState.aes_context));
    
    FILE *f = tbFopen(encryptState.keyFileName, "a");
    
    // Store the actual length of the source file.
    char size_buf[32];
    sprintf(size_buf, "\n%d\n", fSize);
    fwrite(size_buf, 1, strlen(size_buf), f);

    tbFclose(f);  //res = fclose( f );
}

extern void encUfAudioPrepare(char *fname) {
    osMutexAcquire(fileOpMutex, osWaitForever);
    strcpy(encryptCb.fname, fname);

    osEventFlagsClear(fileOpRequestEventId, kAnyRequest);
    osEventFlagsClear(fileOpResultEventId, kAnyResult);
    osEventFlagsSet(fileOpRequestEventId, kEncryptPrepareRequest);

    osMutexRelease(fileOpMutex);

    printf("File encryption ready\n");

}
void encUfAudioAppend(Buffer_t *pData, size_t dataLen) {
    osMessageQueuePut(encryptCb.encryptQueueId, &pData, 0, osWaitForever);
    osEventFlagsSet(fileOpRequestEventId, kEncryptBlockRequest);
}
void encUfAudioFinalize(size_t fileLen) {
    encryptCb.fileSize = fileLen;
    osEventFlagsSet(fileOpRequestEventId, kEncryptFinishRequest);
    osEventFlagsWait(fileOpResultEventId, kEncryptResult, osFlagsWaitAny, osWaitForever);
}


