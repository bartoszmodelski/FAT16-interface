#include <stdio.h>
#include <string.h>
#include "filesys.h"

int main()
{   
    printf("\n<>  FORMAT");
    format();

    printf("\n<>  TESTING");
    
    MyFILE * file = myfopen("file1.txt", 'w');
    for (int i = 0; i < 500; i++)
        myfputc(0x23, file);
    myfclose(file);
    
    copyMyFileToRealDisk("file1.txt", "file1.txt");
    copyRealFileToMyDisk("file1.txt", "file2.txt");
    
    
    printf("\n<>  SAVE TO FILE");
    writeDisk("virtualdiskA5_A1");
    
    printf("\n<>  DONE \n");
    return 0;
}