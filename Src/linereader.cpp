//
// Created by bill on 3/15/2023.
//

#include "linereader.h"

LineReader::LineReader(const char *fname, const char *ftag) {
    LineReader::ftag     = ftag;
    LineReader::errCount = 0;
    LineReader::lineNum  = 0;
    LineReader::fname    = fname;
    LineReader::inFile   = tbOpenRead(fname);
    *line = '\0';
}

void *LineReader::error(const char *msg) {
    errLog("%s:L%d %s ", ftag, lineNum, msg);
    errCount++;
    return NULL;
}

const char *LineReader::readLine(const char *tag) {
    if (errCount > 0) {
        line[0] = '\0';
        return NULL;
    }

    while (fscanf(inFile, "%201[^\n]%*[\n]", line) == 1) {
        lineNum++;
        if (strlen(line) > 200)
            return static_cast<const char *>(error("Line too long"));
        // Truncate line at first '#'
        char *hash = strchr(line, '#');
        if (hash != NULL) *hash = '\0';
        // Trim trailing whitespace.
        int lastChar = strlen(line) - 1;
        while (lastChar > 0 && isspace(line[lastChar])) --lastChar;
        if (lastChar < 0)
            continue; // empty line, get next
        line[lastChar + 1] = '\0';
        // Trim leading whitespace.
        const char *pFirst = line;
        while (isspace(*pFirst)) ++pFirst;
        if (pFirst > line)
            strcpy(line, pFirst);
        // Got anything?
        if (strlen(line))
            return line;
    }
    return static_cast<const char *>(error(tag));  // no str found (EOF)
}

const char *LineReader::readString(const char *tag) {
    if (const char *pLine = readLine(tag))
        return allocStr(line, tag);
    return "";
}

int LineReader::readInt(const char *tag) {
    readLine(tag);    // line  gets next line (no comments, or lead/trail white space)
    int v;
    if (sscanf(line, "%d", &v) == 1)
        return v;

    error(tag);
    return 0;
}

char LineReader::line[202];
