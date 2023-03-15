// TBookV2    packageData.h

#ifndef PACKAGE_DATA_H
#define PACKAGE_DATA_H

#include <vector>

#include "tbook.h"

//  ------------  TBook Content

// package_data.txt  file structure:
// Version string
// NPaths     //  # of content dirs
//   path[0]    //  directory containing audio   0
//   ...
//   path[NPaths-1]
// NPkgs                 // P0
//   text_name           // P0
//   short_name          // P0
//   prompts_path_list   // P0
// NSubjs                // P0
//   text_name           // P0.S0
//   short_prompt        // P0.S0
//   invitation          // P0.S0
// NMessages             // P0.S0
//   dir + audio_file    // P0.S0.M0 
//   ...
//   dir + audio_file    // P0.S0.Mm 
//   text_name           // P0.S1
//   short_prompt        // P0.S1
//   invitation          // P0.S1
// NMessages             // P0.S1
//   dir + audio_file    // P0.S1.M0 
//   ...
//   dir + audio_file    // P0.S0.Mm 

class AudioFile {
public:
    AudioFile(){}
    AudioFile(int pathIx, const char *filename) : pathIdx(pathIx), filename(filename) {}
    int  pathIdx;                   // index of audio directory in Deployment.audioPaths
    const char *filename;                 // filename within directory
};

class Subject {
public:
    friend class PackageDataReader;
    friend class Package;

    Subject(int n) : messages(n) {};

    short numMessages() { return messages.size(); };

    AudioFile *getMessage(int ixMsg);

    const char *getName() { return name; };

public:
    const char *name;               // text identifier for log messages
    AudioFile  *shortPrompt;        // dir+name of subject's short audio prompt
    AudioFile  *invitation;         // dir+name of subject's audio invitation
    MsgStats   *stats;
    std::vector<AudioFile*> messages;
};

/**
 * Class to hold playlists that can grow dynamically. Although this might be generally useful,
 * the only user (at creation) is User Feedback, and this class only implements what User
 * Feedback needs.
 */
class DynamicSubject : Subject {
public:
    friend class PackageDataReader;
    friend class Package;

    DynamicSubject(int n, int pathIx, const char *listFileName);
    void addRecording(const char *recordingFileName);
    void readListFile();

private:
    int pathIx;
    const char *listFileName;
};

class Package {
public:
    friend class PackageDataReader;

    Package() : ixUserFeedback(-1) {};

    const char *findAudioPath(char *pathBuf, const char *fn);

    const char *getName() { return name; };

    int numSubjects() { return nSubjects; };

    Subject *getSubject(int ixSubj);

    void addRecording(const char *recordingFileName);

private:
    const char *getPath(int pathIx);

public:
    int        pkgIdx;               // index within Deployment.packages[], for debugging.
    const char *name;                // text identifier for log messages
    AudioFile  *pkg_prompt;          // audio prompt for package
    int        ixUserFeedback;
    int        nPathIxs;
    short      *pathIxs;             // search path for prompts
    int        nSubjects;
    Subject    **subjects;           // description of each subject
};

class Deployment {
public:
    friend class PackageDataReader;

    int numPackages() { return nPackages; };

    Package *getPackage(int ixPackage);

    const char *getPathForAudioFile(char *pathBuffer, AudioFile *audioFile);

public:
    const char *name;               // version string for loaded deployment
    int        numPaths;
    const char **audioPaths;        // list of all directories containing audio files for this Deployment
    int        nPackages;
    Package    **packages;          // cnt & ptrs to descriptions of packages
};



// Deployment data from  packages_data.txt
extern Deployment *theDeployment;     // cnt & ptrs to info for each loaded content Package
extern int iPkg;           // index of currPkg in Deployment
extern Package *currPkg;        // TBook content package in use

// Deployment loading & access interface
extern bool readPackageData(void);                        // load structured TBook package contents

extern char *getPathForAudioFile(char *path, AudioFile *aud); // fill path[] with dir/filename & return it


#endif           // PACKAGE_DATA_H`
