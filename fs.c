#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"
#include <stdlib.h>

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

typedef struct {
    int magicNumber;
} Superblock;

void fs_init( void) {
    block_init();

    /* Cria variável buffer */
    char buffer[512] = {0};
    /* Lẽ o primeiro bloco do disco (superbloco) e guarda na variável buffer */
    block_read(0, buffer);

    /* Copia os 4 primeiros bytes do superbloco para a variável magicNumber */
    unsigned char magicNumber[5];
    bcopy((unsigned char*) buffer, magicNumber, 4);
    magicNumber[4] = '\0';

    /* Checa se o número mágico corresponde ao número mágico do nosso sistema de arquivos */
    if(same_string((char*) magicNumber, MAGIC_NUMBER)) {
        printf("Disco formatado!\n");
    } else {
        printf("Disco não formatado!\n");
    }
}

int fs_mkfs( void) {
    return -1;
}

int fs_open( char *fileName, int flags) {
    return -1;
}

int fs_close( int fd) {
    return -1;
}

int fs_read( int fd, char *buf, int count) {
    return -1;
}
    
int fs_write( int fd, char *buf, int count) {
    return -1;
}

int fs_lseek( int fd, int offset) {
    return -1;
}

int fs_mkdir( char *fileName) {
    return -1;
}

int fs_rmdir( char *fileName) {
    return -1;
}

int fs_cd( char *dirName) {
    return -1;
}

int 
fs_link( char *old_fileName, char *new_fileName) {
    return -1;
}

int fs_unlink( char *fileName) {
    return -1;
}

int fs_stat( char *fileName, fileStat *buf) {
    return -1;
}

