//
// Created by bill on 9/26/2023.
//

#ifndef TBV2_RANDOM_H
#define TBV2_RANDOM_H


class Random {
public:
    Random(void);

    void initialize();

    int getRandomBytes(uint8_t *bytes, size_t numBytes);
    int getRandomUUID(uint8_t uuid_buf[16]);

    /**
     * Formats a UUID and returns the buffer in which it is formatted.
     * NOTE: static buffer shared by all calls. Not thread safe.
     * @param uuid to be formatted.
     * @return pointer to static buffer with formatted UUID.
     */
    const char *formatUUID(uint8_t uuid[16]);

private:
    static bool initialized;
    static void initialize(const char *seedStr);
};

extern Random random;

#endif //TBV2_RANDOM_H
