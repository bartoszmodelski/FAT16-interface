#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "filesys.h"


diskBlock_t  virtualDisk [MAXBLOCKS];           // define our in-memory virtual, with MAXBLOCKS blocks
fatEntry_t   FAT         [MAXBLOCKS];           // define a file allocation table with MAXBLOCKS 16-bit entries
fatEntry_t   rootDirIndex            = 0 ;       // rootDir will be set by format
dirEntry_t   staticBufferForCurrentDir;
dirEntry_t * currentDir              = &staticBufferForCurrentDir;
fatEntry_t   currentDirIndex         = 0 ;


void readFAT();
void setCurrentDirToRoot();
folderAndEntry getDetailsFromPath(const char * path);
int getParentBlock(int indexOfDirectory);

/* writeDisk : writes virtual disk out to physical disk
 * 
 * in: file name of stored virtual disk
 */

void writeDisk ( const char * filename )
{
   FILE * dest = fopen( filename, "w" ) ;
   if ( fwrite ( virtualDisk, sizeof(virtualDisk), 1, dest ) < 0 )
      fprintf ( stderr, "write virtual disk to disk failed\n" ) ;
   //write( dest, virtualDisk, sizeof(virtualDisk) ) ;
   fclose(dest);
   printf("\nDisk has been saved.");
   
}

void readDisk ( const char * filename )
{
   FILE * dest = fopen( filename, "r" ) ;
   if ( fread ( virtualDisk, sizeof(virtualDisk), 1, dest ) < 0 )
      fprintf ( stderr, "write virtual disk to disk failed\n" ) ;
   //write( dest, virtualDisk, sizeof(virtualDisk) ) ;
      fclose(dest) ;
   
   readFAT();
   
   rootDirIndex = 3;
   currentDirIndex = 3;
   currentDir = NULL;
}


/* the basic interface to the virtual disk
 * this moves memory around
 */

void writeBlock ( diskBlock_t * block, int block_address )
{
   memmove(virtualDisk[block_address].data, block->data, BLOCKSIZE);
}


/* read and write FAT
 * 
 * please note: a FAT entry is a short, this is a 16-bit word, or 2 bytes
 *              our blocksize for the virtual disk is 1024, therefore
 *              we can store 512 FAT entries in one block
 * 
 *              how many disk blocks do we need to store the complete FAT:
 *              - our virtual disk has MAXBLOCKS blocks, which is currently 1024
 *                each block is 1024 bytes long
 *              - our FAT has MAXBLOCKS entries, which is currently 1024
 *                each FAT entry is a fatEntry_t, which is currently 2 bytes
 *              - we need (MAXBLOCKS /(BLOCKSIZE / sizeof(fatEntry_t))) blocks to store the
 *                FAT
 *              - each block can hold (BLOCKSIZE / sizeof(fatEntry_t)) fat entries
 */

/*****
   FUNCTIONS FOR GCS D3-D1 BELOW
*****/


void copyFAT()
{
   writeBlock((diskBlock_t*)(fatBlock_t*)&FAT, 1);
   writeBlock((diskBlock_t*)((uintptr_t)(diskBlock_t*)(fatBlock_t*)&FAT + sizeof(diskBlock_t)), 2);
}


/* implement format()
 */
void format()
{
   diskBlock_t block;
   memset(&block, 0x0, BLOCKSIZE);
      
   for (int i = 0; i < MAXBLOCKS; i++)
      writeBlock(&block, i);
      
	strcpy(block.data, "CS3026 Operating Systems Assignment");
	writeBlock(&block, 0);
	
	/* prepare FAT table
	 * write FAT blocks to virtual disk
	 */
	 
	 for (int i = 0; i < MAXBLOCKS; i++)
	   FAT[i] = UNUSED;
	 
	 FAT[0] = ENDOFCHAIN;
	 FAT[1] = 2;
	 FAT[2] = ENDOFCHAIN;
	 FAT[3] = ENDOFCHAIN;  
	
	 copyFAT();
	 
	 
	//Create, zero-out and prepare root directory
   diskBlock_t newRootDir;
   memset(&newRootDir, 0x0, BLOCKSIZE);
   newRootDir.dir.isDir = 1;
   newRootDir.dir.parentBlockIndex = 0;
   newRootDir.dir.nextEntry = 0;
   //Write it to the disk
   writeBlock(&newRootDir, 3);
   //Set the var having index of root dir block
   rootDirIndex = 3;
   
   setCurrentDirToRoot();
}


/*****
   FUNCTIONS FOR GCS C3-C1 BELOW
*****/

void loadBlock(diskBlock_t * block, int block_address)
{
   memmove(block->data, virtualDisk[block_address].data, BLOCKSIZE);
}

void readFAT()
{
   loadBlock((diskBlock_t*)(fatBlock_t*)&FAT, 1);
   loadBlock((diskBlock_t*)((uintptr_t)(diskBlock_t*)(fatBlock_t*)&FAT + sizeof(diskBlock_t)), 2);
}


void zeroOutBlock(int index)
{
   diskBlock_t block;
   memset(&block, 0x0, BLOCKSIZE);
	writeBlock(&block, index);
}

void clearChain(int index)
{
   int next = FAT[index];
   
   FAT[index] = UNUSED;
   zeroOutBlock(index);
   
   if (next != ENDOFCHAIN) {
      clearChain(next);
   } else {
      copyFAT();
   }
}


//Provided with any block, finds last block of the chain.
int getEndOfChainIndex(int index)
{
   while (FAT[index] != ENDOFCHAIN)
      index = FAT[index];
   return index;  
}

void setCurrentDirToRoot()
{
   currentDirIndex = rootDirIndex;
   currentDir = NULL;
   printf("\nCurrent dir: root");
}


int findEntryByName(int dirBlockIndex, const char * filename)
{
   //load needed dirblock in memory
   diskBlock_t dirBlock;
   loadBlock(&dirBlock, dirBlockIndex);
   
   //iterate over entryList
   for (int i = 0; i < dirBlock.dir.nextEntry; i++)
   {
      //compare filename from entry number i to the one provided
      if ((strcmp(dirBlock.dir.entryList[i].name, filename) == 0) && (dirBlock.dir.entryList[i].unUsed == 0))
      {
         return i;
      }
      
   }
   return FILE_NOT_FOUND;
}

int countBlocksInChain(int index)
{
   int numberOfBlocks = 1;
   
   while (FAT[index] != ENDOFCHAIN)
   {
      index = FAT[index];
      numberOfBlocks++;
   }
   return numberOfBlocks;
}

//Just iterate over FAT table until find something unused.
//If nothing found return NO_FREE_BLOCKS.
int findFreeBlock()
{
   for (int i = 3; i < MAXBLOCKS; i++)
   {
      if (FAT[i] == UNUSED)
      {
         return i;
      }
   }
   return NO_FREE_BLOCKS;
}

//Check if input for fopen is correct
int validateInputForFOpen(int directoryBlockIndex, const char * filename, const char mode)
//return index of entry of file, if found
//return FILE_NOT_FOUND_IN_W_OR_A_MODE, if file not found but mode is write or append
//return VALIDATION_FAILED otherwise
{
   //Check input mode
   if ((mode != 'r') && (mode != 'w') && (mode != 'a'))
      return VALIDATION_FAILED;
      
   //Load current dir for validation below
   diskBlock_t currDirBlock;
   loadBlock(&currDirBlock, directoryBlockIndex);
    
   //Find file by name in current dir
   int entryIndex = findEntryByName(directoryBlockIndex, filename);
   
   //If file not found and in reading mode
   if ((entryIndex == FILE_NOT_FOUND) && (mode == 'r'))
      return VALIDATION_FAILED;
      
   //If file found, but is a directory
   if ((entryIndex != FILE_NOT_FOUND) && (currDirBlock.dir.entryList[entryIndex].isDir))
      return VALIDATION_FAILED;
   
   //If file not found but mode is append or write
   if ((entryIndex == FILE_NOT_FOUND) && ((mode == 'a') || (mode == 'w')))
      return FILE_NOT_FOUND_IN_W_OR_A_MODE;
      
   return entryIndex;
}

void fillDirEntry(dirEntry_t * dirEntry_ptr, const char * filename, int isDir, int firstBlock)
{
   dirEntry_ptr->entryLength = 0xFFFFFFFF;
   dirEntry_ptr->isDir = isDir;
   dirEntry_ptr->unUsed = 0;
   dirEntry_ptr->modTime = time(NULL);
   dirEntry_ptr->fileLength = 0;
   dirEntry_ptr->firstBlock = firstBlock;
   strcpy(dirEntry_ptr -> name, filename);
}

int allocateNewEntry(int directoryBlockIndex, const char * filename, int isDir)
{
   //Filename length checker
   if (!(strlen(filename) < MAXNAME))
      return ALLOCATION_FAILED;
   
   //load block of directory in which entry is created
   diskBlock_t temp;
   loadBlock(&temp, directoryBlockIndex);
   
   //index pointing to free entry in directory's entryList
   int freeEntry = -1;
   
   //Check if there's space for new entry
   if (temp.dir.nextEntry >= DIRENTRYCOUNT)
   {
      //If there's no space, maybe a unused entry?
      for (int i = 0; i < DIRENTRYCOUNT; i++)
      {
         if (temp.dir.entryList[i].unUsed == 1)
         {
            freeEntry = i;
            break;
         }
      }
      
      //if didn't find anything unused return fail
      if (freeEntry == -1)
      {
         return ALLOCATION_FAILED;
      }
   } else {
      //if entryList not full we can allocate the entryList[ nextEntry ]
      freeEntry = temp.dir.nextEntry;
      
      //if new entry is allocated then nextEntry must be shifted
      temp.dir.nextEntry++;
   }
      
   //Find free block to allocate file
   int index = findFreeBlock();
   
   // If any free block was found, its index was returned
   // If not, NO_FREE_BLOCKS was returned and func cannot proceed
   if (index == NO_FREE_BLOCKS)
      return ALLOCATION_FAILED;
      
   //Updating FAT table
   FAT[index] = ENDOFCHAIN;
   copyFAT();
   
   //UPDATING dirEntry below
      //filling dirEntry structure
   fillDirEntry(&(temp.dir.entryList[ freeEntry ]), filename, isDir, index);
   
   //if dir then initial structure has to be set 
   if (isDir == 1) {
      diskBlock_t newRootDir;
      memset(&newRootDir, 0x0, BLOCKSIZE);
      newRootDir.dir.isDir = 1;
      newRootDir.dir.parentBlockIndex = directoryBlockIndex;
      newRootDir.dir.nextEntry = 0;
      //Write it to the disk
      writeBlock(&newRootDir, index);
   }
   
   //writing the block black into virutal disk
   writeBlock(&temp, directoryBlockIndex);
   
   //returning index of allocated file in its parent's entrylist
   return freeEntry;
}




MyFILE * myfopen(const char * path, const char mode)
{
   folderAndEntry details = getDetailsFromPath(path);
   
   if (details.pathToFolderFound == 0) {
      printf("\nError: path to folder not found.");
      return NULL;
   }
   
   char * filename = malloc(sizeof(details.entryName) + 1);
   strcpy(filename, details.entryName);
   
   int blockIndex = details.folderFirstBlock;
   
   int result = validateInputForFOpen(blockIndex, filename, mode);
   
   if (result == VALIDATION_FAILED)
      return NULL;
      
   int entryIndex = -1;
   
   //if validation returned entryIndex, store it
   if (result > -1)
      entryIndex = result;
   
   diskBlock_t currDirBlock;
   loadBlock(&currDirBlock, blockIndex);
      
   //if validation returned file not found in write or append mode
   //create file and store its index in entryIndex
   if (result == FILE_NOT_FOUND_IN_W_OR_A_MODE)
   {
      entryIndex = allocateNewEntry(blockIndex, filename, 0);
      
      if (entryIndex == ALLOCATION_FAILED)
      {
         printf("\nAllocation failed: no room for new file?");
         return NULL;
      }
      
      //reload dirblock after entry allocation
      loadBlock(&currDirBlock, blockIndex);
   } else if (mode == 'w') {
      //if file found but in write mode it has to be gotten rid of
      
      //clear FAT chain
      clearChain(currDirBlock.dir.entryList[entryIndex].firstBlock);
      //set entry to unused
      currDirBlock.dir.entryList[entryIndex].unUsed = 1;
      //save dir block
      writeBlock(&currDirBlock, blockIndex);
      
      
      //allocate new entry
      entryIndex = allocateNewEntry(blockIndex, filename, 0);
      
      //reload dirBlock after entry allocation
      loadBlock(&currDirBlock, blockIndex);
      
      if (entryIndex == ALLOCATION_FAILED)
         return NULL;
   }
   
   
   
   //Creating filedescriptor structure
   MyFILE * newFile = malloc(sizeof(MyFILE)); //dynamically cause scope independence
   newFile->mode = mode;
   newFile->fileLength = currDirBlock.dir.entryList[entryIndex].fileLength;
   newFile->lastBlockIndex = getEndOfChainIndex(currDirBlock.dir.entryList[entryIndex].firstBlock);
   newFile->parentBlockIndex = blockIndex;
   newFile->parentEntrylistIndex = entryIndex;
   
   //If append mode, pos, currBlock
   if (mode == 'a') {
      newFile->pos = newFile->fileLength - (countBlocksInChain(currDirBlock.dir.entryList[entryIndex].firstBlock) - 1) * BLOCKSIZE; 
         //get position in last file
      newFile->currBlockIndex = newFile->lastBlockIndex;
   } else {
      newFile->pos = 0;
      newFile->currBlockIndex = currDirBlock.dir.entryList[entryIndex].firstBlock;
   }
   //allocation based on currBlockIndex set above
   
   //loading buffer
   loadBlock(&(newFile->buffer), (newFile->currBlockIndex));
   
   //cleanup
   free(filename);
   
   //return handle to the structure
   return newFile;
}

void myfclose(MyFILE	* stream)
{
   if ((stream->mode == 'w') || (stream->mode == 'a'))
   {
      //saving buffer
      writeBlock(&stream->buffer, stream->currBlockIndex);
      
      //updating file length in dirEntry
      diskBlock_t buffer;
      loadBlock(&buffer, stream->parentBlockIndex);
      buffer.dir.entryList[stream->parentEntrylistIndex].fileLength = stream->fileLength;
      writeBlock(&buffer, stream->parentBlockIndex);
   }
   
   //free the dynamically allocated memory 
   free(stream);
}


void myfputc(Byte b, MyFILE * stream)
{
   //If in read mode, nothing to do in here.
   if (stream->mode == 'r')
      return;
   
   
   //If (position after last available position) AND (ENDOFCHAIN block in buffer)
   //function should save buffer and allocate new one.
   if ((stream->pos == BLOCKSIZE)  
         && (stream->currBlockIndex == stream->lastBlockIndex))     
   {
      //Find free block
      int freeBlockIndex = findFreeBlock();
      if (freeBlockIndex == NO_FREE_BLOCKS)
         return;
      
      //Update FAT table
      FAT[stream->currBlockIndex] = freeBlockIndex;
      FAT[freeBlockIndex] = ENDOFCHAIN;
      copyFAT();
      
      //Save buffer before loading new block
      writeBlock(&(stream->buffer), stream->currBlockIndex);
      
      //Updating filedescriptor
      stream->pos = 0;
      stream->lastBlockIndex = freeBlockIndex;
      stream->currBlockIndex = freeBlockIndex;
      
      //Loading recently allocated block
      loadBlock(&(stream->buffer), stream->currBlockIndex);
   } 


   //If (position after last available position) AND (not ENDOFCHAIN block in buffer)
   //then save previous block and load next one.
   //
   //Note: altought second condition is not explicty stated, it does not need to be:
   //    if (position after last available position) AND (endofchain block in buffer)
   //    position would have been changed by preceding IF
   if (stream->pos == BLOCKSIZE) 
   {
      //save buffer
      writeBlock(&(stream->buffer), stream->currBlockIndex);
      //update filedescriptor
      stream->currBlockIndex = FAT[stream->currBlockIndex];
      stream->pos = 0;
      //load new block into buffer
      loadBlock(&(stream->buffer), stream->currBlockIndex);
   }

   //finally write byte B into buffer
   stream->buffer.data[stream->pos] = b;
   
   //fileLength is always equal to (last available position + 1).
   //If current position is equal to fileLength, it means file
   //has just been extended, so fileLength should be updated.
   
   //if position is equal to fileLength, increase file size
   if ((stream->pos == (stream->fileLength % BLOCKSIZE))          //If position is equal to length of the part of the file,
                                                                  // which resides in last block
         && (stream->currBlockIndex == stream->lastBlockIndex))   //AND last block is currently in buffer
      stream->fileLength++;                                       
   stream->pos++;
}

int myfgetc(MyFILE * stream)
{
   if ((stream->pos % BLOCKSIZE == (stream->fileLength % BLOCKSIZE))   //if position is after last position...
         && (stream->currBlockIndex == stream->lastBlockIndex))        //...in last block,
   {
      return EOF;                                                      //position ran out of file.
   }
   
   //If (position is after last available position) 
   // and (NOT in last block)***
   //then next block must be loaded to read rest of the file
   //
   //** second condition works implicitly
   if (stream->pos == BLOCKSIZE)                                
   {
      if (stream->mode != 'r')
         writeBlock(&(stream->buffer), (stream->currBlockIndex));
         
      stream->pos = 0;
      stream->currBlockIndex = FAT[stream->currBlockIndex];
      loadBlock(&(stream->buffer), (stream->currBlockIndex));
   }
   
   //increasing position
   stream->pos++;
   return stream->buffer.data[stream->pos - 1];
}


/*****
   FUNCTIONS FOR GCS B3-B1 BELOW
*****/



void makeDir(fatEntry_t index, char * nameOfDir)
{
   if (allocateNewEntry(index, nameOfDir, 1) == ALLOCATION_FAILED)
      printf("\nAllocation failed (no room for new entry or overlapping name?)");
}

// This function provided with index of a dir block responsible for first directory
// and a name of subdirectory (or file) will return in which block this subdirectory
// or file is stored.
// Exemplary path: aaa/bbb/ccc.
// getFirstBlockOEntry(index_of_block_aaa, "bbb") return index_of_block_bbb
// getFirstBlockOfEntry(index_of_block_bbb, "ccc") return index_of_block_ccc
int getFirstBlockOfEntry(int indexOfDirectory, char * nameOfEntry)
{
   int entryIndex = findEntryByName(indexOfDirectory, nameOfEntry);
   
   if (entryIndex == FILE_NOT_FOUND)
      return ENTRY_NOT_FOUND;
   
   diskBlock_t diskBlock;
   loadBlock(&diskBlock, indexOfDirectory);
   
   return diskBlock.dir.entryList[entryIndex].firstBlock;
}

int getNumberOfSlashes(const char * sequence)
{
   int count = 0;
      for (int i = 0; sequence[i] != '\0'; sequence[i]=='/' ? (i++, count++) : i++);
   return count;
}


//input: path, * array of strings, size of array of strings
//writes: array of strings
//returns: number of strings that were saved
int processPath(const char * inputPath, char ** listOfEntries, int lengthOfList)
{
   //inputPath could be in immutable memory
   char path[MAXNAME];
   strcpy(path, inputPath);
   
   //setup strtok and extract first name in path
   char *saveptr, *buffer;
   buffer = __strtok_r(path, "/", &saveptr);
   if (buffer == NULL) {
      return 0;
   }
   strncpy(listOfEntries[0], buffer, MAXNAME);
   
   //extract rest of path
   int usedElemenets;
   for (usedElemenets = 1; usedElemenets < lengthOfList; usedElemenets++)
   {
      buffer =  __strtok_r(NULL, "/", &saveptr);
      if (buffer == NULL)
         break;
      strncpy(listOfEntries[usedElemenets], buffer, MAXNAME);
   }
   
   return usedElemenets;
}



void getDirNameFromItsIndex(char * name, int index) {
   if (index == 3) {
      strcpy(name, "root");
      return;
   }
   diskBlock_t diskBlock;
   //load block
   loadBlock(&diskBlock, index);
   //load its parents
   loadBlock(&diskBlock, diskBlock.dir.parentBlockIndex);
   
   //iterate over parent's entries
   for (int i = 0; i < diskBlock.dir.nextEntry; i++) {
      if ((diskBlock.dir.entryList[i].firstBlock == index) && (diskBlock.dir.entryList[i].unUsed == 0)) {
         strcpy(name, diskBlock.dir.entryList[i].name);
         return;
      }
   }
}

folderAndEntry setFolderAndEntry(char ** listOfEntries, int lengthOfList, int isRelative)
{
   int firstBlock;
   folderAndEntry details;
   
   if (isRelative == RELATIVE_PATH) {     //relative path
      firstBlock = currentDirIndex;
   } else {                   //absolute path
      firstBlock = rootDirIndex;
   }
   
   int blockOfEntry;
   
   int i;
   for (i = 0; i < lengthOfList - 1; i++)
   {
      //special case
      if (strcmp(listOfEntries[i], "..") == 0) {
         blockOfEntry = getParentBlock(firstBlock);
         if (firstBlock == 0) {
            printf("\nError: path points to parent of root.");
            details.pathToFolderFound = 0;
            details.entryFound = 0;
            return details;
         }
         
         firstBlock = blockOfEntry;
         continue;
      }
      //find next folder
      blockOfEntry = getFirstBlockOfEntry(firstBlock, listOfEntries[i]);
      
      //if folder with this name does not exist return error
      if (blockOfEntry == ENTRY_NOT_FOUND)
      {
         details.pathToFolderFound = 0;
         return details;
      }
      //if exists, set firstBlock 
      //so it will be explored by next iteration
      firstBlock = blockOfEntry;
   }
   
   
   details.folderFirstBlock = firstBlock;
   if (i == 0) {
      details.pathToFolderFound = 1;
      strcpy(details.folderName, "root");
   } else {
      details.pathToFolderFound = 1;
      strcpy(details.folderName, listOfEntries[i-1]);
   }
   
   if (strcmp(details.folderName, "..") == 0) {
      getDirNameFromItsIndex(details.folderName, details.folderFirstBlock);
   }
   
   strcpy(details.entryName, listOfEntries[i]);
   //details.entryFirstBlock = ?

   
   int buffer = getFirstBlockOfEntry(firstBlock, listOfEntries[i]);
   
   if (buffer == ENTRY_NOT_FOUND) {
      details.entryFound = 0;
   } else {
      details.entryFound = 1;
      details.entryFirstBlock = buffer;
   }

   
   return details;
}


folderAndEntry getDetailsFromPath(const char * inputPath)
{
   char path[strlen(inputPath)];
   if ((inputPath[0] == '.') && (inputPath[1] == '/')) {
      memmove(path, inputPath+2, strlen(inputPath));
   } else {
      strcpy(path, inputPath);
   }
   
   folderAndEntry details;
   details.pathToFolderFound = 0;
   details.entryFound = 0;
   
   if (strlen(path) == 0) {
      printf("\nPath of length 0");
      return details;
   }
   
   //count number of slashes in path
   int lengthOfList = getNumberOfSlashes(path) + 1;  // + 1 because relative path has
                                                   // more elements than slashes
   
   //create an empty array to store entries' names
   char ** listOfEntries = malloc(lengthOfList * sizeof(char*));
   for (int i = 0; i < lengthOfList; i++)
   {
      listOfEntries[i] = malloc((MAXNAME + 1) * sizeof(char));
      memset(listOfEntries[i], '\0', (MAXNAME));
   }
   
   //get entries' names
   int usedElements = processPath(path, listOfEntries, lengthOfList);
   //if no elements were written return error
   if (usedElements == 0) {
      printf("\nIncorrect path");
      return details;
   }

   //prints all elements of path, useful for testing
   //for (int i = 0; i < usedElements; i++)
      //printf("\n :: %s", listOfEntries[i]);
      
   //check if path exists
   if ((path[0] == '/') ||          //if path is absolute, 
       (currentDir == NULL))   {    //or currentDir is root and relative can be treated as absolute
      details = setFolderAndEntry(listOfEntries, usedElements, ABSOLUTE_PATH);
   } else {
      details = setFolderAndEntry(listOfEntries, usedElements, RELATIVE_PATH);
   }
      
   //cleanup
   for (int i = 0; i < lengthOfList; i++)
      free(listOfEntries[i]);
   free(listOfEntries);


   return details;
}


void mymkdir(char * path)
{

   folderAndEntry details = getDetailsFromPath(path);
   
   if (details.pathToFolderFound == 0) {
      printf("\nError: path to folder not found.");
      return;
   }
   if (details.entryFound == 1) {
      printf("\nError: existing folder or file collides with given filename.");
      return;
   }
   
   makeDir(details.folderFirstBlock, details.entryName);
}

char ** listDir(fatEntry_t index) {
   diskBlock_t diskBlock;
   loadBlock(&diskBlock, index);
   
   //create an empty array to store names
   char ** listOfEntries = malloc((DIRENTRYCOUNT + 1) * sizeof(char*));
   for (int i = 0; i < DIRENTRYCOUNT + 1; i++)
      listOfEntries[i] = NULL;
      
   int shift = 0;
   for (int i = 0; i < DIRENTRYCOUNT; i++)   {
      if ((diskBlock.dir.entryList[i].unUsed == 1) || (diskBlock.dir.entryList[i].name[0] == '\0'))
      {
         shift++;
         continue;
      }
      listOfEntries[i - shift] = malloc((MAXNAME + 1) * sizeof(char));
      memset(listOfEntries[i - shift], '\0', (MAXNAME));
      strcpy(listOfEntries[i - shift], diskBlock.dir.entryList[i].name);
      printf("\n%s", listOfEntries[i - shift]);
   }
   
   return listOfEntries;
   /*   //cleanup
   for (int i = 0; i < DIRENTRYCOUNT; i++)
      free(listOfEntries[i]);
   free(listOfEntries);*/
}

void freeList(char ** listOfEntries)
{
   for (int i = 0; i < DIRENTRYCOUNT + 1; i++)
   {
      if (listOfEntries[i] != NULL)
         free(listOfEntries[i]);
   }
   free(listOfEntries);
}

char ** mylistdir(const char * path)
{
   if (strcmp(path, ".") == 0)
   {
      printf("\nContent of %s: ", path);
      return listDir(currentDirIndex);
   }
   
   folderAndEntry details = getDetailsFromPath(path);
   
   if ((details.pathToFolderFound == 0) || (details.entryFound == 0)) {
      printf("\nError: path is incorrect");
      return NULL;
   }
   printf("\nContent of %s: ", path);
   return listDir(details.entryFirstBlock);
}


/*****
   FUNCTIONS FOR GCS A5-A1 BELOW
*****/

int getParentBlock(int indexOfDirectory)
{
   diskBlock_t diskBlock;
   loadBlock(&diskBlock, indexOfDirectory);
   return diskBlock.dir.parentBlockIndex;
}


void setCurrentDir(dirEntry_t buffer)
{
   staticBufferForCurrentDir = buffer;
   currentDir = &staticBufferForCurrentDir;
   currentDirIndex = currentDir->firstBlock;
   printf("\nCurrent dir: %s", currentDir->name);
}

void mychdir(char * path)
{
   if (strcmp(path, "/") == 0) {
      setCurrentDirToRoot();
      return;
   } 
   if (strcmp(path, "..") == 0) {
      if (currentDirIndex == rootDirIndex) {
         printf("Error: you can't go below root dir.");
      }
      int parent = getParentBlock(currentDirIndex);
      int grandParent = getParentBlock(parent);
      if (grandParent == 0) {
         setCurrentDirToRoot();
         return;
      }
      diskBlock_t diskBlock;
      loadBlock(&diskBlock, grandParent);
      //find parent's entry in grandparent
      int i;
      for (i = 0; i < diskBlock.dir.nextEntry; i++) {
         if (diskBlock.dir.entryList[i].firstBlock == parent) 
            break;
      }
      setCurrentDir(diskBlock.dir.entryList[i]);
      return;
   }
   
   folderAndEntry details = getDetailsFromPath(path);
   if ((details.pathToFolderFound == 0) || (details.entryFound == 0)) {
      printf("\nError: path is incorrect");
      return;
   }
   currentDirIndex = details.entryFirstBlock;
   
   diskBlock_t buffer;
   loadBlock(&buffer, details.folderFirstBlock);
   
   //if it could return error, then test above would be failed
   int entryIndex = findEntryByName(details.folderFirstBlock, details.entryName);
   
   setCurrentDir(buffer.dir.entryList[entryIndex]);
   
}

void myremove(char * path) 
{
   folderAndEntry details = getDetailsFromPath(path);
   if ((details.pathToFolderFound == 0) || (details.entryFound == 0)) {
      printf("\nError: file not found");
      return;
   }
   
   
   //update entrylist in parent folder
   diskBlock_t diskBlock;
   loadBlock(&diskBlock, details.folderFirstBlock);
   
   //check if not a folder
   if (diskBlock.dir.entryList[ findEntryByName(details.folderFirstBlock, details.entryName) ].isDir == 1) {
      printf("\nError: path leads to a folder.");
      return;  
   }
   diskBlock.dir.entryList[ findEntryByName(details.folderFirstBlock, details.entryName) ].unUsed = 1;
   writeBlock(&diskBlock, details.folderFirstBlock);
   
   // clean fat table and overwrite blocks with zeros
   // *** Please note I am aware it is not neccessary to overwrite 
   // blocks with zeros for disk to work correctly. I am doing this
   // to ensure disk does not have any clutter and facilitate marking.
   clearChain(details.entryFirstBlock);
}

int anyUsedEntryInside(fatEntry_t blockIndex) 
{
   diskBlock_t diskBlock;
   loadBlock(&diskBlock, blockIndex);
   
   for (int i = 0; i < diskBlock.dir.nextEntry; i++)
   {
      if (diskBlock.dir.entryList[i].unUsed == 0)
      {
         return 1;
      }
   }
   return 0;
}

void myrmdir(char * path) 
{
   folderAndEntry details = getDetailsFromPath(path);
   
   if ((details.entryFirstBlock == currentDirIndex) || (strcmp(path, ".") == 0)) {
      printf("\nError: you cannot remove the folder in which you currently are.");
      return;
   }
   
   if ((details.pathToFolderFound == 0) || (details.entryFound == 0)) {
      printf("\nError: folder not found");
      return;
   }
   
   if (anyUsedEntryInside(details.entryFirstBlock) == 1) {
      printf("\nError: you cannot remove a non empty folder.");
      return;
   }
   
   diskBlock_t diskBlock;
   loadBlock(&diskBlock, details.folderFirstBlock);
   
   int entryIndex = findEntryByName(details.folderFirstBlock, details.entryName);
   diskBlock.dir.entryList[entryIndex].unUsed = 1;
   
   writeBlock(&diskBlock, details.folderFirstBlock);
   //clear block and fat
   clearChain(details.entryFirstBlock);
}

void copyRealFileToMyDisk(char * realPath, char * path)
{
   MyFILE * file = myfopen(path, 'w');
   FILE * realFile = fopen(realPath, "r");
   
   if ((file == NULL) || (realFile == NULL)) {
      if (file != NULL)
         myfclose(file);
      if (realFile != NULL)
         fclose(realFile);
      printf("\nError: at least one of the paths is incorrect.")
      return;
   }
   
   int buffer = fgetc(realFile);
   while (buffer != EOF) {
      myfputc(buffer, file);
      buffer = fgetc(realFile);
   }
   
   myfclose(file);
   fclose(realFile);
}

void copyMyFileToRealDisk(char * realPath, char * path)
{
   MyFILE * file = myfopen(path, 'r');
   FILE * realFile = fopen(realPath, "w");
   
   if ((file == NULL) || (realFile == NULL)) {
      if (file != NULL)
         myfclose(file);
      if (realFile != NULL)
         fclose(realFile);
      printf("\nError: at least one of the paths is incorrect.")
      return;
   }
   
   int buffer = myfgetc(file);
   
   while (buffer != EOF) {
      fputc(buffer, realFile);
      buffer = myfgetc(file);
   }
   
   myfclose(file);
   fclose(realFile);
}