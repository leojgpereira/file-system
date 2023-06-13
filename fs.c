#include <stdlib.h>
#include <math.h>
#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

Superblock* superblock;
Inode* inodes;
char* buffer;

#include "fs_functions.c"

void fs_init( void) {
    block_init();

    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc(512 * sizeof(char));

    /* Zera todos os bytes da variável buffer */
    bzero(buffer, 512);
    /* Lê o primeiro bloco do disco (superbloco) e armazena na variável buffer */
    block_read(0, buffer);

    /* Inicializa a estrutura superblock */
    superblock = (Superblock*) malloc(sizeof(Superblock));
    /* Copia o conteúdo do primeiro bloco para a estrutura superblock */
    bcopy((unsigned char*) buffer, (unsigned char*) superblock, sizeof(Superblock));

    free(buffer);

    /* Checa se o disco está formatado comparando o valor do número mágico */
    if(same_string(superblock->magicNumber, MAGIC_NUMBER)) {
        printf("Disco formatado!\n");
    } else {
        printf("Disco não formatado!\n");

        free(superblock);

        /* Invoca a função responsável por formatar o disco */
        fs_mkfs();
    }

    /* Cria tabela de descritores de arquivo em memória */
    File* fd_table = init_fd_table();
}

int fs_mkfs( void) {
    /* Inicializa a estrutura superblock */
    superblock = (Superblock*) malloc(sizeof(Superblock));

    /* Inicializa as informações do superbloco */
    bcopy((unsigned char*) MAGIC_NUMBER, (unsigned char*) superblock->magicNumber, 5);
    superblock->diskSize = FS_SIZE;
    superblock->workingDirectory = ROOT_DIRECTORY_INODE;
    superblock->numberOfInodes = NUMBER_OF_INODES;
    superblock->numberOfDataBlocks = NUMBER_OF_DATA_BLOCKS;
    superblock->iMapStart = I_MAP_START;
    superblock->dMapStart = D_MAP_START;
    superblock->inodeStart = INODE_START;
    superblock->dataBlockStart = DATA_BLOCK_START;

    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc(512 * sizeof(char));

    /* Zera os bytes do buffer, copia os bytes da estrutura superblock para o buffer e escreve o conteúdo no primeiro bloco (superblock) do disco */
    bzero(buffer, 512);
    bcopy((unsigned char*) superblock, (unsigned char*) buffer, sizeof(Superblock));
    block_write(0, buffer);

    /* Zera os bytes da variável buffer, seta o bit correspondende ao primeiro i-node para 1 (ocupado - diretório raiz) e escreve o buffer no bloco onde fica o mapa de bits dos i-nodes */
    bzero(buffer, 512);
    set_bit(buffer, 0, 7);
    block_write(I_MAP_START, buffer);

    /* Zera os bytes da variável buffer, seta o bit correspondende ao primeiro bloco de dados para 1 (ocupado - diretório raiz) e escreve o buffer no bloco onde fica o mapa de bits dos blocos de dados */
    bzero(buffer, 512);
    set_bit(buffer, 0, 7);
    block_write(D_MAP_START, buffer);

    /* Cria vetor de i-nodes */
    inodes = (Inode*) malloc(NUMBER_OF_INODES * sizeof(Inode));

    /* Define o primeiro i-node como sendo o i-node correspondente ao diretório raiz */
    inodes[0].type = 1;
    inodes[0].size = 2 * sizeof(Directory);
    inodes[0].direct[0] = DATA_BLOCK_START;

    /* Copia o vetor de i-nodes para a variável buffer e escreve o conteúdo nos blocos alocados para armazenar i-nodes */
    buffer = realloc(buffer, 22528);
    bcopy((unsigned char*) inodes, (unsigned char*) buffer, NUMBER_OF_INODES * sizeof(Inode));

    for(int i = INODE_START; i < INODE_END + 1; i++) {
        block_write(i, &buffer[(i - INODE_START) * 512]);
    }

    /* Guarda o nome das duas entradas iniciais de um diretório */
    char dirEntry1[MAX_FILE_NAME] = ".";
    char dirEntry2[MAX_FILE_NAME] = "..";

    /* Inicializa um vetor de diretórios e insere as entradas '.' e '..' inicialmente como entradas do diretório raiz */
    Directory* rootDirectory = (Directory*) malloc(2 * sizeof(Directory));
    bcopy((unsigned char*) dirEntry1, (unsigned char*) rootDirectory[0].name, 32);
    rootDirectory[0].inode = 0;
    bcopy((unsigned char*) dirEntry2, (unsigned char*) rootDirectory[1].name, 32);
    rootDirectory[1].inode = 0;

    /* Escreve no primeiro bloco de dados o diretório raiz */
    buffer = realloc(buffer, 512);
    bzero(buffer, 512);
    bcopy((unsigned char*) rootDirectory, (unsigned char*) buffer, 2 * sizeof(Directory));
    block_write(DATA_BLOCK_START, buffer);

    free(buffer);
    free(inodes);
    free(rootDirectory);

    return 0;
}

int fs_open(char *fileName, int flags) {
    Inode* inode = find_inode(superblock->workingDirectory);

    printf("***%d\n", inode->type);


    return 5;
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

int fs_mkdir(char *fileName) {
    printf("-----%d %lu - %s\n", strlen(fileName), sizeof(Directory), fileName);

    buffer = (char*) malloc(1024 * sizeof(char));

    int inodeNumber = -1;
    char imap[64];
    block_read(1, buffer);
    bcopy((unsigned char*) buffer, (unsigned char*) imap, 64);

    for(int i = 0; i < (int) ceil(NUMBER_OF_INODES / 8); i++) {
        for(int j = 7; j > -1; j--) {
            int bit = get_bit(imap, i, j);
            printf("bit = %d\n", bit);

            if(bit == 0) {
                inodeNumber = 7 - j + (i * 8);
                set_bit(imap, i, j);
                break;
            }
        }

        if(inodeNumber != -1)
            break;
    }

    printf("inode number = %d is free\n", inodeNumber);

    bcopy((unsigned char*) imap, (unsigned char*) buffer, 64);
    block_write(1, buffer);

    if(inodeNumber == -1)
        return -1;

    //////////////////////////////////////

    int blockNumber = -1;
    char dmap[251];
    block_read(2, buffer);
    bcopy((unsigned char*) buffer, (unsigned char*) dmap, 251);

    for(int i = 0; i < (int) ceil(NUMBER_OF_DATA_BLOCKS / 8); i++) {
        for(int j = 7; j > -1; j--) {
            int bit = get_bit(dmap, i, j);
            printf("bit = %d\n", bit);

            if(bit == 0) {
                blockNumber = 7 - j + (i * 8);
                set_bit(dmap, i, j);
                break;
            }
        }

        if(blockNumber != -1)
            break;
    }

    printf("block number = %d is free\n", blockNumber + DATA_BLOCK_START);

    bcopy((unsigned char*) dmap, (unsigned char*) buffer, 251);
    block_write(2, buffer);

    if(blockNumber == -1)
        return -1;

    //////////////////////////////////////

    char dirEntry1[MAX_FILE_NAME] = ".";
    char dirEntry2[MAX_FILE_NAME] = "..";

    Directory* newDirectory = (Directory*) malloc(2 * sizeof(Directory));
    bcopy((unsigned char*) dirEntry1, (unsigned char*) newDirectory[0].name, 32);
    newDirectory[0].inode = inodeNumber;
    bcopy((unsigned char*) dirEntry2, (unsigned char*) newDirectory[1].name, 32);
    newDirectory[1].inode = superblock->workingDirectory;

    printf(". inode = %d, .. inode = %d\n", newDirectory[0].inode, newDirectory[1].inode);

    bzero(buffer, 512);
    bcopy((unsigned char*) newDirectory, (unsigned char*) buffer, 2 * sizeof(Directory));
    block_write(blockNumber + DATA_BLOCK_START, buffer);

    Inode* newInode = (Inode*) malloc(sizeof(Inode));
    newInode->type = 1;
    newInode->size = 2 * sizeof(Directory);
    newInode->direct[0] = blockNumber + DATA_BLOCK_START;

    save_inode(newInode, inodeNumber);

    //////////////////////////////////////

    Inode* inode = find_inode(superblock->workingDirectory);
    printf("***%d\n", inode->direct[0]);

    int blockStart = inode->size / 512;
    int blockEnd = (inode->size + sizeof(Directory)) / 512;
    int byteStart = inode->size % 512;

    printf("***%d %d %d\n", blockStart, blockEnd, byteStart);

    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    Directory* directory = (Directory*) malloc(sizeof(Directory));
    bcopy((unsigned char*) fileName, (unsigned char*) directory->name, strlen(fileName) + 1);
    directory->inode = inodeNumber;
    printf("***%s is allocated to inode number %d\n", directory->name, directory->inode);

    bcopy((unsigned char*) directory, (unsigned char*) &buffer[byteStart], sizeof(Directory));
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_write(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    free(buffer);
    free(inode);
    free(directory);

    return 0;

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

