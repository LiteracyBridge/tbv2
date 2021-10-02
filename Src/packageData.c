// TBookV2  packageData.c
//   Gene Ball  Sept2021

#include "tbook.h"
#include "packageData.h"

// --------   read package_data.txt to get structure & audio files for loaded content
char *          PackageVersion = NULL;      // version string from package_data.txt
//int 			nPackages;				    // num packages found on device
Package_t **	Packages;			        // package info

static void		pathCat( char *path, const char * src, const char * src2 ){  // strcat & convert '\' to '/'
	short plen = strlen( path );
	if ( path[plen-1]=='\\' ) path[plen-1] = '/';
	if ( path[plen-1]!='/' ) path[plen++] = '/'; 
	
	short slen = strlen( src );
	for( short i=0; i<slen; i++ )
		path[ plen+i ] = src[i]=='\\'? '/' : src[i];
	path[ plen+slen ] = 0;
	
	if ( src2 != NULL ){
		plen += slen;
		slen = strlen( src2 );
		for( short i=0; i<slen; i++ )
			path[ plen+i ] = src2[i]=='\\'? '/' : src2[i];
		path[ plen+slen ] = 0;
	}
}
static void		appendIf( char * path, const char *suffix ){	// append 'suffix' if not there
	short len = strlen( path );
	short slen = strlen( suffix );
	if ( len<slen || strcasecmp( &path[len-slen], suffix )!=0 )
		strcat( path, suffix );
}
void			XbuildPath( char *dstpath, const char *dir, const char *nm, const char *ext ){ 		// dstpath[] <= "dir/nm.ext"
	short dirlen = strlen( dir );
	short nmlen = strlen( nm );
	short extlen = strlen( ext );
	
	strcpy( dstpath, "M0:/" );
	bool absNm = ( nm[0]=='/' || strcmp(nm, "M0:")==0 );		// is nm an absolute path?
	
	const char *p = absNm? nm : dir;
	if ( p[2]==':' ) p += 3;
	if ( *p=='/' ) p++;
	pathCat( dstpath, p, NULL );
	if ( !absNm ){
		appendIf( dstpath, "/" );
		pathCat( dstpath, nm, NULL );
	}
	appendIf( dstpath, ext );
	if ( strlen( dstpath ) >= MAX_PATH ) 
		tbErr( "path too long" ); 
}
static char *	allocStr( const char * s ){
	char * pStr = tbAlloc( strlen( s )+1, "package paths" );
	strcpy( pStr, s );	
	return pStr;
}

ContentPaths_t * loadContentPaths( FILE *inF ){                // parse pkg_dat list of Content directories
    ContentPaths_t * dirs = NULL;
    char line[200];
    int dirCnt = 0;
    if ( fscanf( inF, "%d\n", &dirCnt )!= 1 ) { errLog( "pkg_dat dirCnt missing" ); return NULL; }
    dirs = (ContentPaths_t *) tbAlloc( sizeof(int) + dirCnt * sizeof(char *), "contentPaths" );
 
    dirs->nPaths = dirCnt;
    for (int i=0; i<dirCnt; i++){
        if ( fscanf( inF, "%s\n", line )==1  ){
            dirs->audPath[i] = allocStr( line );
        } else errLog( "pkg_dat audPath[%d] fail", i );        
    }
    return dirs;
}
PathList_t *     loadSearchPath( FILE *inF ){                  // parse next pkg_dat line as list of ContentPath indices
    PathList_t * plist = NULL;
    return plist;
}
AudioFile_t *    findAudio( FILE *inF, PathList_t *paths ){     // find next pkg_dat line as file on paths
    AudioFile_t * audfile = NULL;
    return audfile;
}
Subject_t *      loadSubject( FILE *inF, PathList_t *paths ){   // parse content subject from pkg_dat 
    Subject_t * subj = NULL;
    return subj;
}

bool            loadPackageData( void ){            // load structured TBook package contents 
	char line[200];
    
	FILE *inFile = tbOpenRead( TBP[ pPKG_DAT ] ); 
	if ( inFile == NULL ){ errLog( "package_data.txt not found\n" );  return false;  }
	if ( fscanf( inFile, "%[^\n]\n", line )==1  ){
        PackageVersion = allocStr( line );
        dbgLog("! Package_data: %s",  PackageVersion );
    } else errLog( "pkg_dat Version fail" );        
         
    ContentPaths_t * DirList = loadContentPaths( inFile );
    
	tbCloseFile( inFile );	
    return true;
}

/*
void					findPackages( void ){						// scan for M0:/package*  directories
	const char *packageDirPatt = "M0:/package*";		//TODO:  "M0:/packages/packages.list"

	fsFileInfo fInfo;
	fInfo.fileID = 0;
	nPackages = 0;
	int pkgNmLen = 100;
																									//TODO: read directories from packages.list
	while ( ffind( packageDirPatt, &fInfo )==fsOK ){		// find all package directories on device
		char pkgPath[ MAX_PATH ];
		buildPath( pkgPath, "M0:/", fInfo.name, "/" );
		TBPackage[ nPackages ] = readContent( pkgPath, nPackages );
		nPackages++;
		if ( strlen( fInfo.name ) < pkgNmLen ){
			pkgNmLen = strlen( fInfo.name );
			iPkg = nPackages-1;			// shortest is the initial package
		}
	}
	TBPkg = TBPackage[ iPkg ];
}
TBPackage_t * readContent( const char * pkgPath, int pkgIdx ){		// parse list_of_subjects.txt & messages.txt for each Subj => Content	
	char pth[MAX_PATH];
	buildPath( pth, pkgPath, "list_of_subjects", ".txt" );		//TODO: playlists.list
																									//TODO: read package.properties  => audio_path, version, playlists?
	
	FILE *inFile = tbOpenReadBinary( pth ); //fopen( pth, "rb" );
	if ( inFile==NULL )
		tbErr( "list_of_subjects file not found" );
	
	TBPackage_t * Pkg = tbAlloc( sizeof(TBPackage_t), "pkg" );
	Pkg->idx = pkgIdx;
	Pkg->nSubjs = 0;
	Pkg->path = allocStr( pkgPath );								
	//TODO: audio_path=path1;path2;...;pathn
	buildPath( pth, pkgPath, "package", ".wav" );
	Pkg->packageName = allocStr( pth );
	logEvtNINS( "loadPkg", "idx", pkgIdx, "path", pkgPath );
	
	char 		line[200], dt[30];			// up to 200 characters per line
	fsTime tm;
	buildPath( pth, pkgPath, "version", ".txt" );				//TODO: from package.properties
	char *	contentVersion = loadLine( line, pth, &tm );
	sprintf( dt, "%d-%d-%d %d:%d", tm.year, tm.mon, tm.day, tm.hr, tm.min );
	logEvtNSNS( "Package", "dt", dt, "ver", contentVersion );
	
	TknID 	lineTkns[20];
	short 	nLnTkns = 0;		// num tokens in current line
	char	msg_txt_fname[MAX_PATH+15];
	while (true){
		char *s = fgets( line, 200, inFile );
		if (s==NULL) break;
		nLnTkns = tokenize( lineTkns, 20, line );		// get tokens from line
		if ( nLnTkns>0 && asPunct( lineTkns[0] )!=Semi ){	// if not comment, tkn is file path for subj/messages.txt
			tbSubject * sb = tbAlloc( sizeof( tbSubject ), "tbSubjects" ); 
			Pkg->TBookSubj[ Pkg->nSubjs ] = sb;
			Pkg->nSubjs++;

			buildPath( msg_txt_fname, pkgPath, tknStr( lineTkns[0] ), "");
			sb->path = allocStr( msg_txt_fname );
			
			buildPath( msg_txt_fname, sb->path, "messages", ".txt" );
			TknID msgs = parseFile( msg_txt_fname );	// parse JSONish messages.txt
			sb->name = tknStr( getField( msgs, "Subject" ));
			sb->audioName = tknStr( getField( msgs, "audioName" ));
			sb->audioPrompt = tknStr( getField( msgs, "audioPrompt" ));
			TknID msgLst = getField( msgs, "Messages" );
			sb->NMsgs = lstCnt( msgLst );
			if ( sb->NMsgs == 0 || sb->NMsgs > MAX_SUBJ_MSGS ) 
				tbErr( "Invalid msg list" );
			for ( int i=0; i<sb->NMsgs; i++ )
				sb->msgAudio[ i ] = tknStr( getField( msgLst, (char *)i ));			// file paths for each message
			logEvtNSNI( "Subj", "nm", sb->name, "nMsgs", sb->NMsgs );
		}
	}
	tbCloseFile( inFile );  //fclose( inFile );	// done with list_of_subjects.txt
	return Pkg;
}
*/
// packageData.c 
