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

void fs_init( void) {
    block_init();

    /* Cria variável buffer e lẽ o primeiro bloco do disco (superbloco) e guarda na variável buffer */
    char buffer[512] = {0};
    block_read(0, buffer);

    Superblock* superblock = (Superblock*) malloc(sizeof(Superblock));
    bcopy((unsigned char*) buffer, (unsigned char*) superblock, sizeof(Superblock));

    /* Checa se o número mágico corresponde ao número mágico do nosso sistema de arquivos */
    if(same_string(superblock->magicNumber, MAGIC_NUMBER)) {
        printf("Disco formatado!\n");
    } else {
        printf("Disco não formatado!\n");
        fs_mkfs();
    }
}

int fs_mkfs( void) {
    Superblock* superblock = (Superblock*) malloc(sizeof(Superblock));
    bcopy((unsigned char*) MAGIC_NUMBER, (unsigned char*) superblock->magicNumber, 5);
    superblock->diskSize = FS_SIZE;
    superblock->numberOfInodes = NUMBER_OF_INODES;
    superblock->numberOfDataBlocks = NUMBER_OF_DATA_BLOCKS;
    superblock->inodeStart = INODE_START;
    superblock->dataBlockStart = DATA_BLOCK_START;

    char buffer[512] = {0};
    bcopy((unsigned char*) superblock, (unsigned char*) buffer, sizeof(Superblock));
    block_write(0, buffer);

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

