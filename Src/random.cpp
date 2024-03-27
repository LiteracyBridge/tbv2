//
// Created by bill on 9/26/2023.
//
#include <arm_compat.h>

#include <assert.h>

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "encAudio.h"
#include "inc/random.h"

// Forwards for intrinsic RNG.
int getRandom( void *pRNG, unsigned char *pBuffer, size_t buffer_len );
void initRandom( const char *pers );

bool Random::initialized = false;

struct EntropyState {
    mbedtls_ctr_drbg_context drbg_context;
    mbedtls_entropy_context entropy_context;
};
EntropyState entropyState;


Random::Random(void) {
}

void Random::initialize() {
    if (!initialized) {
        initRandom("TBook entropy generation seed for STM32F412");
        initialized = true;
    }
}

int Random::getRandomBytes(uint8_t *bytes, size_t numBytes) {
    assert(initialized);
    return getRandom(NULL, bytes, numBytes);
}

int Random::getRandomUUID(uint8_t uuid_buf[16]) {
    int result = getRandomBytes(uuid_buf, 16);
    if (result == 0) {
        uuid_buf[6] = uuid_buf[6] & 0x0f | 0x40;
        uuid_buf[8] = uuid_buf[8] & 0x3f | 0x80;
    }
    return result;
}

const char *Random::formatUUID(uint8_t uuid[16]) {
    static char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
           uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
           uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    return uuid_str;
}


Random random = Random();

//region getRandom
#if defined( STM32F412Vx )
#include "stm32f412vx.h"

int getRandom( void *pRNG, unsigned char *pBuffer, size_t buffer_len );
void initRandom( const char *pers ) {   // reset RNG before encrypt or decrypt
    mbedtls_ctr_drbg_init( &entropyState.drbg_context );
    int ret = mbedtls_ctr_drbg_seed( &entropyState.drbg_context, getRandom, &entropyState.entropy_context, (const unsigned char *) pers, strlen( pers ));
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

        uint32_t randomBits = RNG->DR; // next 4 bytes of random bits
        if ( i < numWords ) {
            pWords[i] = randomBits;
        } else if ( extraBytes > 0 ) {
            for (int j = 0; j < extraBytes; j++)
                pBuffer[i * 4 + j] = ( randomBits >> j * 8 ) & 0xFF;
        }
    }
    return 0;
}

#else

void initRandom(char *pers) {   // reset RNG before encrypt or decrypt
    mbedtls_entropy_init(&entropyState.entropy_context);
    mbedtls_ctr_drbg_init(&entropyState.drbg_context);
    int ret = mbedtls_ctr_drbg_seed(&entropyState.drbg_context, mbedtls_entropy_func, &entropyState.entropy_context,
                                    (const unsigned char *) pers, strlen(pers));
    if (ret != 0) TlsErr("drbg_seed", ret);
}

int getRandom(void *pRNG, unsigned char *pBuffer, size_t buffer_len) {         // fill pBuffer with true random data from STM32F412 RNG dev
    int ret = mbedtls_ctr_drbg_random(&entropyState.drbg_context, pBuffer, buffer_len);
    if (ret != 0) TlsErr("drbg_rand", ret);
    return 0;
}

#endif
//endregion

