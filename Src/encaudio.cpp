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
#include "random.h"
#include "tbook.h"

static const int AES_KEY_BITS = 256;
static const int AES_KEY_LENGTH = AES_KEY_BITS / 8;
static const char *DER_FILE_NAME = "/system/uf.der";

static const int AES_BLOCK_SIZE = 16;

bool encUfAudioEnabled = false;

// TODO: combine EncryptCb and EncryptState.
struct EncryptCb encryptCb;
struct EncryptCb *pEncryptCb = &encryptCb;

typedef struct EncryptState {
    bool initialized;
    bool enabled;
    mbedtls_aes_context aes_context;
    uint8_t iv[16];
    mbedtls_pk_context pk_context;
    char keyFileName[MAX_PATH];
    char encFileName[MAX_PATH];
} EncryptState_t;
EncryptState_t encryptState;

void startEncrypt(char *fname );
void encryptBlock( const uint8_t *in, uint8_t *out, int len );
void endEncrypt( size_t fSize );

void TlsErr(const char *msg, int cd) {
    char ebuf[100];
    mbedtls_strerror(cd, ebuf, sizeof(ebuf));
    tbErr("tlsErr: %s => %s", msg, ebuf);
}

// non-public export from Random.
extern int getRandom( void *pRNG, unsigned char *pBuffer, size_t buffer_len );

void encUfAudioInit() {
    if (encryptState.initialized) {
        return;
    }
    encryptState.initialized = true;
    random.initialize();
    int derSize = fsize(DER_FILE_NAME);
    if (derSize > 0) {
        FILE *derFile = tbFopen(DER_FILE_NAME, "rb");
        if (derFile == NULL) {
            return;
        }
        uint8_t *derBytes = static_cast<uint8_t*>(tbAlloc(derSize, "der"));
        int result = fread(derBytes, 1, derSize, derFile);
        tbFclose(derFile);
        if (result == derSize) {
            // We have a public key, so initialize the PK encryption system.
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

/**
 * Called from the FileOps thread to encrypt a file. Buffers of data are inserted by the MediaPlayer thread
 * into a queue, and read here, then encrypted and written to a file.
 */
void encUfAudioLoop() {
    osMutexAcquire(fileOpMutex, osWaitForever);
#if STATIC_ENCTYPE_BUFFER
    static uint8_t encrypted[2048];
#else
    uint8_t *encrypted = static_cast<uint8_t *>(tbAlloc(2048, "der"));
#endif
    strcpy( encryptState.encFileName, pEncryptCb->fname );   // copy *.wav path
    char *pdot = strrchr( encryptState.encFileName, '.' );
    *pdot = 0;

    logEvtFmt( "startEnc", "%s", encryptState.encFileName );

    startEncrypt( encryptState.encFileName);       // write basenm.key & initialize AES encryption
    strcat( encryptState.encFileName, ".enc" );
    FILE *fEncrypted = tbOpenWriteBinary( encryptState.encFileName );

    int nWritten = 0;
    uint32_t waitFlags = kAnyEncryptRequest;
    while (1) {
        // Wait for any event.
        uint32_t eventFlags = osEventFlagsWait(fileOpRequestEventId, waitFlags, osFlagsWaitAny, osWaitForever);
        // Process any buffers in the encryption queue.
        Buffer_t *pBuffer;
        while (osMessageQueueGet(pEncryptCb->encryptQueueId, &pBuffer, 0, 0) == osOK) {
            encryptBlock((const uint8_t *)pBuffer, encrypted, 2048);
            int result = fwrite(encrypted, 1, 2048, fEncrypted);
            releaseBuffer(pBuffer);
            nWritten++;
        }
        // If we have a finish request, finish. This event is sent from encUfAudioFinalize.
        if (eventFlags & kEncryptFinishRequest) {
            endEncrypt(pEncryptCb->fileSize);
            tbFclose(fEncrypted);
            break;
        }
    }
#if !STATIC_ENCRYPT_BUFFER
    free(encrypted);
#endif
    // Let MediaPlayer know that encryption is finished.
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
    int result = random.getRandomBytes(session_key, AES_KEY_LENGTH);
    if (result != 0) TlsErr("RNG gen", result);
    result = random.getRandomBytes(encryptState.iv, sizeof(encryptState.iv));
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

