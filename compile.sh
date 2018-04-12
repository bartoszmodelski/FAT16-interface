rm program.exe
gcc -std=c99 shell.c filesys.c -o program.exe
./program.exe
xxd virtualdiskA5_A1 dump.txt