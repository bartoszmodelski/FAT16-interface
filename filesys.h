/* filesys.h
 * 
 * describes FAT structures
 * http://www.c-jump.com/CIS24/Slides/FAT/lecture.html#F01_0020_fat
 * http://www.tavi.co.uk/phobos/fat.html
 */

#ifndef FILESYS_H
#define FILESYS_H

#include <time.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAXBLOCKS     1024
#define BLOCKSIZE     1024
#define FATENTRYCOUNT (BLOCKSIZE / sizeof(fatEntry_t))
#define DIRENTRYCOUNT ((BLOCKSIZE - (2*sizeof(int)) ) / sizeof(dirEntry_t))
#define MAXNAME       256
#define MAXPATHLENGTH 1024

#define UNUSED        -1
#define ENDOFCHAIN     0

#ifndef EOF
#define EOF           -1
#endif

//Constants for findEntryByName
#define FILE_NOT_FOUND                    -1

//Constants for valideInputForFOpen
#define VALIDATION_FAILED                 -1
#define FILE_NOT_FOUND_IN_W_OR_A_MODE     -2

//Constants for allocateNewEntry
#define ALLOCATION_FAILED                 -1

//Constants for findFreeBlock
#define NO_FREE_BLOCKS                    -1

//Constants for getBlockOfSubentry
#define ENTRY_NOT_FOUND                   -1

//Constants for folderAndEntry
#define INEXISTANT_ENTRY                  -1

//Constants for setFolderAndEntry
#define RELATIVE_PATH                     1
#define ABSOLUTE_PATH                     0


typedef unsigned char Byte ;

/* create a type fatEntry_t, we set this currently to short (16-bit)
 */
typedef short fatEntry_t ;


// a FAT block is a list of 16-bit entries that form a chain of disk addresses

//const int   fatEntrycount = (blocksize / sizeof(fatEntry_t)) ;

typedef fatEntry_t fatBlock_t [ FATENTRYCOUNT ] ;


/* create a type dirEntry_t
 */
 
typedef struct dirEntry {
   int         entryLength ;   // records length of this entry (can be used with names of variables length)
   Byte        isDir ;
   Byte        unUsed ;
   time_t      modTime ;
   int         fileLength ;
   fatEntry_t  firstBlock ;
   char        name [MAXNAME] ;
} dirEntry_t ;

// a directory block is an array of directory entries

//const int   dirEntrycount = (blocksize - (2*sizeof(int)) ) / sizeof(dirEntry_t) ;

typedef struct dirBlock {
   int isDir ;
   fatEntry_t parentBlockIndex;
   int nextEntry ;
   dirEntry_t entryList [ DIRENTRYCOUNT ] ; // the first two integer are marker and endpos
} dirBlock_t ;



// a data block holds the actual data of a fileLength, it is an array of 8-bit (byte) elements

typedef Byte dataBlock_t [ BLOCKSIZE ] ;


// a diskBlock can be either a directory block, a FAT block or actual data

typedef union block {
   dataBlock_t data ;
   dirBlock_t  dir  ;
   fatBlock_t  fat  ;
} diskBlock_t ;

// finally, this is the disk: a list of diskBlocks
// the disk is declared as extern, as it is shared in the program
// it has to be defined in the main program fileLength

extern diskBlock_t virtualDisk [ MAXBLOCKS ] ;

// when a file is opened on this disk, a file handle has to be
// created in the opening program

typedef struct filedescriptor {
   int         pos;           // byte within a block
   char        mode;
   fatEntry_t  currBlockIndex;
   diskBlock_t buffer;
   fatEntry_t  lastBlockIndex;
   int         fileLength;
   fatEntry_t  parentBlockIndex;
   int         parentEntrylistIndex;
} MyFILE;


typedef struct directoryAndEntry {
   char folderName[MAXNAME];
   fatEntry_t folderFirstBlock;
   int pathToFolderFound;
   char entryName[MAXNAME];
   fatEntry_t entryFirstBlock;
   int entryFound;
} folderAndEntry;


void format();
void writeDisk ( const char * filename );
MyFILE * myfopen(const char * filename, const char mode);
void myfclose(MyFILE * stream);
void myfputc(Byte b, MyFILE * stream);
int myfgetc(MyFILE * stream);
void mymkdir(char * path);
char ** mylistdir(const char * path);
void freeList(char ** entries);
void mychdir(char * path);
void myremove(char * path);
void myrmdir(char * path);

void copyRealFileToMyDisk(char * realPath, char * path);
void copyMyFileToRealDisk(char * realPath, char * path);

#endif
