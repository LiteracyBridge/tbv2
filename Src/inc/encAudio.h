// encAudio.h
//   encryption of data for export to Amplio
//
#ifndef _ENC_AUDIO_H_
#define _ENC_AUDIO_H_

#include <stdbool.h>
#include <stdint.h>

#include "buffers.h"
#include "tbook.h"

#define ENCRYPT_QUEUE_LENGTH 8
struct EncryptCb {
    char                fname[MAX_PATH];
    FILE                *encrypteFile;
    size_t              fileSize;
    osMessageQueueId_t  encryptQueueId;
    uint32_t            mq_mem[osRtxMessageQueueMemSize(ENCRYPT_QUEUE_LENGTH, sizeof(Buffer_t *))/4U];
};
extern struct EncryptCb encryptCb;

/**
 * When true, a 'uf.der' file was found and successfully used to initialize encryption.
 */
extern bool encUfAudioEnabled;

/**
 * Called one time to initialize the encryption sub-system.
 */
extern void encUfAudioInit(void);

/**
 *  Encryption loop, called from the FileOps thread. Iteratively encrypts blocks of data,
 *  and writes the encrypted blocks to a file.
 */
extern void encUfAudioLoop(void);

/**
 * Prepares to encrypt a file of the given name. Allocates a random session key, encrypts
 * it with the public key, and saves the encrypted key and IV to ${fname-base}.key. Creates a
 * file ${fname-base}.enc to receive the encrypted blocks.
 * @param fname Full name of the audio file to be encrypted.
 */
extern void encUfAudioPrepare(char *fname);

/**
 * Encrypts the next blcok of data and appends the result the the .enc file.
 * @param pData The data to be encrypted and saved.
 * @param dataLen Lenth of the data.
 */
extern void encUfAudioAppend(Buffer_t *pData, size_t dataLen);

/**
 * Called after all of the data has been given. Closes the .enc file, appends the length value
 * to the .key file.
 * @param fileLen The actual length of the un-encrypted data.
 */
extern void encUfAudioFinalize(size_t fileLen);

// Helper also used by Random.
extern void TlsErr(const char *msg, int cd);

#endif // _ENC_AUDIO_H_
