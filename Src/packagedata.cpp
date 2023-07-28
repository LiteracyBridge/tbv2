// TBookV2  packageData.c
//   Gene Ball  Sept2021
#include <ctype.h>

#include "encaudio.h"
#include "linereader.h"
#include "tbook.h"
#include "csm.h"
#include "packageData.h"

/***************************   read package_data.txt to get structure & audio files for loaded content  ***********************/

extern const char * const preferredAudioExtensions[];
extern const int numAudioExtensions;

Deployment *Deployment::instance = NULL;          // extern cnt & ptrs to info for each loaded content Package

/**
 * Gets the AudioFile object for the given message.
 * @param iMsg Index of message for which AudioFile is needed.
 * @return A pointer to the AudioFile.
 */
AudioFile *Subject::getMessage(int iMsg) {
    if (iMsg < 0 || iMsg >= messages.size())
        errLog("gMsg(%s.%d) bad idx", name, iMsg);
    AudioFile *audioFile = messages[iMsg];
    if (audioFile == NULL)
        errLog("gMsg(%s.%d) bad msg file", name, iMsg);
    return audioFile;
}

/**
 * Constructs a dynamic playlist object.
 * @param n The allocation size for the vector of messages.
 * @param pathIx The index in the Package's list of paths of the path that contains / will contain
 *      this playlist's message audio files.
 * @param listFileName The name of the file that contains / will contain the messages that are
 *      added to this playlist.
 */
DynamicSubject::DynamicSubject(int n, int pathIx, const char *listFileName) : Subject(n) {
    this->pathIx = pathIx;
    this->listFileName = listFileName;
}

/**
 * Adds a message to the dynamic playlist. The message filename is also persisted in the dynamic
 * playlist's list file.
 * @param recordingFileName The name of the audio file to be added to the playlist.
 */
void DynamicSubject::addRecording(const char *recordingFileName) {
    const char *fname = strrchr(recordingFileName, '/');
    // Skip past last '/'. If none, use given file name.
    fname = (fname == NULL) ? recordingFileName : fname + 1;
    // Add the file to the playlist.
    AudioFile *audioFile = new("uf") AudioFile(pathIx, allocStr(fname, "uf"));
    messages.insert(messages.end(), audioFile);
    // Persist the file in the playlist list file.
    FILE *listFile = tbFopen(listFileName, "a");
    fwrite(fname, 1, strlen(fname), listFile);
    fwrite("\n", 1, strlen("\n"), listFile);
    tbFclose(listFile);
}

/**
 * Load the list of previously added messages. Called at startup after the rest of
 * package_data has been loaded.
 */
void DynamicSubject::readListFile() {
    LineReader lr = LineReader(listFileName, "uf list");
    while (lr.readLine("uf") != NULL) {
        AudioFile *audioFile = new("uf") AudioFile(pathIx, allocStr(lr.getLine(), "uf"));
        messages.insert(messages.end(), audioFile);
    }
}

/**
 * Returns a Subject, by index, from the Package.
 * @param iSubj Index of the desired Subject.
 * @return A pointer to the Subject.
 */
Subject *Package::getSubject(int iSubj) {
    if (subjects == NULL)
        errLog("cSubj[%d] -- bad pkg", iSubj);
    if (iSubj < 0 || iSubj >= nSubjects)
        errLog("cSubj(%d) bad idx", iSubj);
    Subject *subj = subjects[iSubj];
    if (subj == NULL)
        errLog("cSubj(%d) bad subj", iSubj);
    return subj;
}

const char *Package::getPath(int pathIx) {         // return content directory path for pathList[i]
    if (pathIx < 0 || pathIxs == NULL || pathIx >= nPathIxs)
        errLog("getPath bad args");
    short dirIdx = pathIxs[pathIx];
    if (dirIdx < 0 || dirIdx >= Deployment::instance->numPaths)
        errLog("getPath bad dirIdx");
    return Deployment::instance->audioPaths[dirIdx];
}

const char *Package::findFileOnPath(char *pathBuf, const char *fn, const char * const paths[], int nPaths, const char * const exts[], int nExts) { // fill path[] with first dir/nm.wav & return it
    char fname[MAX_PATH];
    strcpy(fname, fn);
    // Drop any leading path.
    char *pF;
    if ((pF=strrchr(fname, '/')) != NULL || (pF=strrchr(fname, '\\')) != NULL) strcpy(fname, pF);
    // Truncate any extension.
    if ((pF=strrchr(fname, '.')) != NULL) *pF = '\0';
    // Search by paths...
    for (int iPath = 0; iPath < nPaths; ++iPath) {
        const char *aPath = paths[iPath];
        strcpy(pathBuf, aPath);
        strcat(pathBuf, fname);
        pF = pathBuf + strlen(pathBuf);
        // ...then by extension.
        for (int j=0; j<nExts; j++) {
            strcpy(pF, exts[j]);
            if (fexists(pathBuf))
                return pathBuf;
        }
    }
    return NULL;
}

// fill path[] with first dir/nm.wav & return it
const char *Package::findAudioPath(char *pathBuf, const char *fn) {
    if (pathBuf == NULL || pathIxs == NULL)
        errLog("findAudioPath bad srchpaths");
    const char *paths[nPathIxs];
    // Build paths array for search
    for (int ix = 0; ix < nPathIxs; ix++) {
        paths[ix] = getPath(ix);
    }
    return findFileOnPath(pathBuf, fn, paths, nPathIxs, preferredAudioExtensions, numAudioExtensions);
}

const char *Package::findScriptPath(char *pathBuf, const char *fn) {
    if (pathBuf == NULL || pathIxs == NULL)
        errLog("findScriptPath bad srchpaths");
    const char *paths[nPathIxs];
    static const char * const exts[] = {".csm"};
    // Build paths array for search
    for (int ix = 0; ix < nPathIxs; ix++) {
        paths[ix] = getPath(ix);
    }
    return findFileOnPath(pathBuf, fn, paths, nPathIxs, exts, 1);
}

void Package::addRecording(const char *recordingFileName) {
    if (ixUserFeedback >= 0) {
        DynamicSubject *dynamicSubject = static_cast<DynamicSubject*>(getSubject(ixUserFeedback));
        dynamicSubject->addRecording(recordingFileName);
    }
}


Package *Deployment::getPackage(int ixPackage) {                               // return iPkg'th package from current deployment
    if (ixPackage < 0 || ixPackage >= numPackages())
        errLog("getPackage(%d) bad idx", ixPackage);
    return packages[ixPackage];
}

/**
 * Build a full path from a filename and index into array-of-paths.
 * @param path The buffer to receive the full name.
 * @param aud The AudioFile for which to build the path.
 * @return the path. (Why?)
 */
const char *Deployment::getPathForAudioFile(char *pathBuffer, AudioFile *aud) { // fill path[] with dir/filename & return it
    const char **paths = audioPaths;
    if (pathBuffer == NULL || paths == NULL || aud->pathIdx < 0 || aud->pathIdx >= numPaths)
        errLog("buildAudioPath bad paths");
    strcpy(pathBuffer, paths[aud->pathIdx]);                // content directory path (ends in '/')
    strcat(pathBuffer, aud->filename);                      // filename (with extension)
    return pathBuffer;
}
//endregion

class PackageDataReader : LineReader {
public:
    PackageDataReader(const char *fname);

    bool readPackageData();

    void readAudioPaths(Deployment &deployment);

    AudioFile *readAudioFile(const char *tag);

    void resetSubjectOptions();
    void parseSubjectOptions();
    Subject *readSubject();

    void readPackageSearchPathList(Package &package);

    Package *readPackage(int ixPackage);

private:
    char subjectScript[MAX_PATH];  // Optional script file for a given subject.

    bool userFeedbackPublic;
    int ufPathIx;
    const char *ufListFile;
};

//
// private Deployment routines
/**
 * Read the count of "content paths" and the list of paths.
 * Adds an entry for UF.
 * @param deployment The Deployment into which to populate the path list.
 */
void PackageDataReader::readAudioPaths(Deployment &deployment) {
    if (errCount > 0) return;
    deployment.numPaths   = readInt("pkg_dat dirCnt");
    // If user feedback is public, also allocate space for the path to recordings.
    deployment.audioPaths = new("audioPaths") const char *[deployment.numPaths + (userFeedbackPublic ? 1 : 0)];
    for (int i = 0; i < deployment.numPaths; i++) {
        deployment.audioPaths[i] = readString("audPath");
    }
    if (userFeedbackPublic) {
        ufPathIx = deployment.numPaths++;
        deployment.audioPaths[ufPathIx] = TBP[pRECORDINGS_PATH];
    }
}

void PackageDataReader::readPackageSearchPathList(Package &package) {                    // parse next pkg_dat line as list of ContentPath indices
    if (errCount > 0) return;
    int d[10];
    readLine("prompts_paths");
    package.nPathIxs = sscanf(line, "%d;%d;%d;%d;%d;%d;%d;%d;%d;%d; \n",
                          &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9]);
    package.pathIxs = new("pathList") short[package.nPathIxs];
    for (int ix=0; ix<package.nPathIxs; ++ix) {
        package.pathIxs[ix] = d[ix];
    }
}

AudioFile *PackageDataReader::readAudioFile(const char *typ) {        // load dirIdx & filename from pkg_dat line
    if (errCount > 0) return NULL;
    AudioFile *audioFile = new("audioFile") AudioFile;
    readLine(typ);
    char fname[100];
    if (sscanf(line, "%d %s ", &audioFile->pathIdx, fname) == 2) {
        audioFile->filename = allocStr(fname, "audFilename");
    } else {
        error(typ);
    }
    return audioFile;
}

void PackageDataReader::resetSubjectOptions() {
    *subjectScript = '\0';
}

void PackageDataReader::parseSubjectOptions() {
    char name[50];
    char value[200];
    int nParts = sscanf(line, "%[^:]:%s", name, value);
    if (nParts == 2 && strcmp(name, "script") == 0) {
        strcpy(subjectScript, value);
    }
}

/**
 * Parse the description of one playlist.
 * @return a pointer to the Playlist object
 */
Subject *PackageDataReader::readSubject() {                       // parse content subject from pkg_dat
    if (errCount > 0) return NULL;

    resetSubjectOptions();
    const char * name = readString("subjName");

    AudioFile *prompt = readAudioFile("subj_pr");
    AudioFile *invitation = readAudioFile("subj_inv");

    // Optional values, like "script:foo.csm"
    const char *line = readLine("opts");
    while (line && !isdigit(*line)) {
        parseSubjectOptions();
        line = readLine("opts");
    }
    int nMessages = atoi(line);

    Subject *subj;
    if (nMessages == 0) {
        subj = new("subject") DynamicSubject(0, ufPathIx, "M0:/content/uf_list.txt");
    } else {
        subj = new("subject") Subject(nMessages);
    }
    subj->name = name;
    subj->shortPrompt = prompt;
    subj->invitation = invitation;
    if (*subjectScript) {
        subj->script = allocStr(subjectScript, "script");
    }

//    subj->messages = new("msgList") AudioFile *[subj->nMessages];
    subj->stats    = static_cast<MsgStats *>(tbAlloc(sizeof(MsgStats), "stats"));
    for (int i = 0; i < nMessages; i++) {
        subj->messages[i] = readAudioFile("msg");
    }
    return subj;
}

Package *PackageDataReader::readPackage(int pkgIdx) {
    /*
     *   package_name
     *   prompt_path prompt_filename
     *   playlist_prompt_path;playlist_prompt_path...
     *     num_playlists
     */
    if (errCount > 0) return NULL;
    Package *pkg = new("package") Package;

    pkg->pkgIdx     = pkgIdx;
    pkg->name       = readString("pkgName");
    pkg->pkg_prompt = readAudioFile("pkg_pr");      // audio prompt for package.
    readPackageSearchPathList(*pkg);         // search path for prompts

    pkg->nSubjects = readInt("nSubjs");            // number of playlists (subjects) in package
    pkg->subjects  = new("subj_list") Subject *[pkg->nSubjects+1];
    for (int i = 0; i < pkg->nSubjects; i++) {
        pkg->subjects[i] = readSubject();
        if (userFeedbackPublic && pkg->subjects[i]->numMessages() == 0) {
            pkg->ixUserFeedback = i;
        }
    }
    logEvtNSNI("LdPkg", "nm", pkg->getName(), "nSubj", pkg->numSubjects());
    return pkg;
}

//
const int PACKAGE_FORMAT_VERSION = 1;                 // format of Oct 2021
//
// public routine to load a full Deployment from  /content/packages_data.txt
bool PackageDataReader::readPackageData(void) {                        // load structured TBook package contents
    Deployment::instance = new("deployment") Deployment;

    if (inFile == NULL) {
        error("package_data.txt not found");
    } else {
        int fmtVer = readInt("fmt_ver");
        if (fmtVer != PACKAGE_FORMAT_VERSION) {
            error("bad format_version");
        } else {
            Deployment::instance->name = readString("deployName");
            logEvtNS("Deployment", "Name", Deployment::instance->name);

            readAudioPaths(*Deployment::instance);

            Deployment::instance->nPackages = readInt("nPkgs");
            Deployment::instance->packages  = new("deplPkgs") Package*[Deployment::instance->numPackages()];
            for (int i = 0; i < Deployment::instance->numPackages(); i++) {
                Deployment::instance->packages[i] = readPackage(i);
            }
        }
    }

    if (errCount > 0) {
        errLog("packages_data: %d parse errors", errCount);
        return false;
    } else {
        closeInFile();
        if (userFeedbackPublic) {
            for (int i=0; i<Deployment::instance->numPackages(); ++i) {
                Package *package = Deployment::instance->getPackage(i);
                int ixUf = package->ixUserFeedback;
                if (ixUf >= 0) {
                    DynamicSubject *dynamicSubject = static_cast<DynamicSubject*>(package->getSubject(ixUf));
                    dynamicSubject->readListFile();
                }
            }
        } else {
            // Search for and delete any lingering .wav recordings.
            char fname[MAX_PATH];
            strcpy(fname, TBP[pRECORDINGS_PATH]);
            strcat(fname, "*.wav");
            // Make a copy of the containing directory, for the fdelete() call.
            char fname2[MAX_PATH];
            strcpy(fname2, TBP[pRECORDINGS_PATH]);
            char * pFn = fname2 + strlen(fname2);

            fsFileInfo fInfo;
            fInfo.fileID = 0;

            FileSysPower(true);
            while (ffind(fname, &fInfo) == fsOK) {
                strcpy(pFn, fInfo.name);
                uint32_t status = fdelete(fname2, NULL);
                printf("Delete status %d: %s\n", status, fname2);
            }

        }
    }
    return true;
}

PackageDataReader::PackageDataReader(const char *fname) : LineReader(fname, "Deployment") {
    userFeedbackPublic = !encUfAudioEnabled;
}

bool readPackageData(void) {
    // load structured TBook package contents
    PackageDataReader pdr = PackageDataReader(TBP[pPACKAGES_DATA_TXT]);
    return pdr.readPackageData();
}



// packageData.c 
