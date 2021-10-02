// TBookV2		packageData.h

#ifndef PACKAGE_DATA_H
#define PACKAGE_DATA_H
#include "tbook.h"

//  ------------  TBook Content

// package_data.txt  file structure:
//? Version string
//  NPaths     //  # of content dirs
//   path[i]    //  directory containing audio
//  NPkgs
//   Pkg[i] search_path_indices[]
//   Pkg[i] text_name
//   Pkg[i] prompts_dir_list
//   Pkg[i] NSubjs
//     Pkg[i].Subj[i]  text_name
//     Pkg[i].Subj[i]  dir_list
//     Pkg[i].Subj[i]  short_prompt
//     Pkg[i].Subj[i]  invitation
//     Pkg[i].Subj[i]  NMessages
//       Pkg[i].Subj[i].Message[i] audio_file
// 

typedef	struct ContentPaths {	// ContentPaths_t--  holds paths for TBook audio contents
	int nPaths;             // number of different content paths
	char * audPath[10];     // audio paths[ 0 .. nPaths-1 ]    
} ContentPaths_t;

typedef struct AudioFile {  // AudioFile_t -- location of a content audio file
    int pathIdx;            // index of audio directory in ContentPaths
    char * filename;        // filename within directory 
} AudioFile_t;

typedef struct PathList {   // PathList_t--  list of ContentPath indices in search order
    short PathLen;          // # of dirs to search
    short DirIdx[10];       // indices in ContentPaths[] 
} PathList_t;

typedef struct Subject { 	// Subject_t-- info for one subject
	char *		    subjName;		// text identifier for log messages
    PathList_t *    audio_dirs;     // directories to search for audio files
    AudioFile_t*    shortPrompt;    // dir+name of subject's short audio prompt
    AudioFile_t*    invitation;     // dir+name of subject's audio invitation
	MsgStats *		stats;
    
	int             nMsgs;          // number of messages
	AudioFile_t*    message[];      // path & filename of resolved audio files
} Subject_t;

typedef struct Package {	// Package_t-- path & subject list for a package
	int				pkgIdx;         // index within TBPackages[]
	char *			packageName;    // text identifier for log messages
    PathList_t *    prompts_dirs;
    AudioFile_t*    pkg_prompt;     // audio prompt for package

	int 			nSubjs;         // number of subjects in package
	Subject_t *		subject[];      // description of each subject
} Package_t;


extern int 				nPackages;				// num packages found on device
extern Package_t **	    Packages;			// package info
extern char *           PackagesVersion;

extern int				iPkg;       // index of currPkg in Packages[]
extern Package_t 	    * currPkg;	// TBook content package in use

//extern void				buildPath( char *dstpath, const char *dir, const char *nm, const char *ext ); // dstpath[] <= "dir/nm.ext"
extern bool             loadPackageData( void );                    // load structured TBook package contents 

//extern void				findPackages( void );									// scan for M0:/package*/  directories
//TBPackage_t * 			readContent( const char * pkgPath, int pkgIdx );		// parse list_of_subjects.txt & messages.txt for each Subj => Content

#endif           // PACKAGE_DATA_H

