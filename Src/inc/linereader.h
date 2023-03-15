//
// Created by bill on 3/15/2023.
//

#ifndef TBV2_LINEREADER_H
#define TBV2_LINEREADER_H

#include "tbook.h"

class LineReader {
public:
    LineReader(const char *fname, const char *ftag);

    void *error(const char *msg);

    const char *readLine(const char *tag);

    const char *readString(const char *tag);

    int readInt(const char *tag);

    ~LineReader() {
        if (inFile!=NULL) {
            tbFclose(inFile);
            inFile = NULL;
        }
    }

    void closeInFile() {
        if (inFile!=NULL) {
            tbFclose(inFile);
            inFile = NULL;
        }
    }

    FILE *getFile() { return inFile; }

    const char *getLine() { return line; }
protected:
    const char  *fname;
    const char  *ftag;
    int         errCount;
    int         lineNum;
    FILE        *inFile;
    static char line[202];
};

#endif //TBV2_LINEREADER_H
