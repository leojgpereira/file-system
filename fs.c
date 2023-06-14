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

/* Variáveis principais utilizadas no sistema de arquivo */
Superblock* superblock;
Inode* inodes;
char* buffer;

/* Guarda o nome das duas entradas iniciais de um diretório */
char dirEntry1[MAX_FILE_NAME] = ".";
char dirEntry2[MAX_FILE_NAME] = "..";

/* Inclui funções adicionais que criamos para uma melhor organização do código */
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

    /* Libera memória alocada dinâmicamente */
    free(buffer);

    /* Checa se o disco está formatado comparando o valor do número mágico */
    if(same_string(superblock->magicNumber, MAGIC_NUMBER)) {
        printf("Disco formatado!\n");
    } else {
        printf("Disco não formatado!\n");
        
        /* Libera memória alocada dinâmicamente */
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
    superblock->iMapStart = I_MAP_BLOCK;
    superblock->dMapStart = D_MAP_BLOCK;
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
    block_write(I_MAP_BLOCK, buffer);

    /* Zera os bytes da variável buffer, seta o bit correspondende ao primeiro bloco de dados para 1 (ocupado - diretório raiz) e escreve o buffer no bloco onde fica o mapa de bits dos blocos de dados */
    bzero(buffer, 512);
    set_bit(buffer, 0, 7);
    block_write(D_MAP_BLOCK, buffer);

    /* Cria vetor de i-nodes */
    inodes = (Inode*) malloc(NUMBER_OF_INODES * sizeof(Inode));

    /* Define o primeiro i-node como sendo o i-node correspondente ao diretório raiz */
    inodes[0].type = 1;
    inodes[0].size = 2 * sizeof(DirectoryItem);
    inodes[0].direct[0] = DATA_BLOCK_START;

    /* Copia o vetor de i-nodes para a variável buffer e escreve o conteúdo nos blocos alocados para armazenar i-nodes */
    buffer = realloc(buffer, 22528);
    bcopy((unsigned char*) inodes, (unsigned char*) buffer, NUMBER_OF_INODES * sizeof(Inode));

    for(int i = INODE_START; i < INODE_END + 1; i++) {
        block_write(i, &buffer[(i - INODE_START) * 512]);
    }

    /* Inicializa um vetor de diretórios e insere as entradas '.' e '..' inicialmente como entradas do diretório raiz */
    DirectoryItem* rootDirectory = (DirectoryItem*) malloc(2 * sizeof(DirectoryItem));
    bcopy((unsigned char*) dirEntry1, (unsigned char*) rootDirectory[0].name, 32);
    rootDirectory[0].inode = 0;
    bcopy((unsigned char*) dirEntry2, (unsigned char*) rootDirectory[1].name, 32);
    rootDirectory[1].inode = 0;

    /* Escreve no primeiro bloco de dados o diretório raiz */
    buffer = realloc(buffer, 512);
    bzero(buffer, 512);
    bcopy((unsigned char*) rootDirectory, (unsigned char*) buffer, 2 * sizeof(DirectoryItem));
    block_write(DATA_BLOCK_START, buffer);

    /* Libera memória alocada dinâmicamente */
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
    /* Verifica se já existe um diretório com o mesmo nome */
    if(dir_exists(fileName)) {
        return -1;
    }

    /* Aloca memória para variável buffer */
    buffer = (char*) malloc(512 * sizeof(char));

    /* Declara vetor de bits para guardar o vetor de bits dos inodes */
    char imap[64];

    /* Encontra inode livre */
    int inodeNumber = find_free_bit_number(imap, I_MAP_BLOCK);

    /* Checa se houve sucesso em encontrar um inode livre */
    if(inodeNumber == -1)
        return -1;

    printf("inode number = %d is free\n", inodeNumber);

    /* Salva mapa de bits atualizado no disco */
    save_bitmap(imap, I_MAP_BLOCK);

    /* Declara vetor de bits para guardar o vetor de bits dos blocos de dados */
    char dmap[251];

    /* Encontra bloco de dados livre */
    int blockNumber = find_free_bit_number(dmap, D_MAP_BLOCK);

    /* Checa se houve sucesso em encontrar um bloco de dados livre */
    if(blockNumber == -1)
        return -1;

    printf("block number = %d is free\n", blockNumber + DATA_BLOCK_START);

    /* Salva mapa de bits atualizado no disco */
    save_bitmap(dmap, D_MAP_BLOCK);

    /* Cria as entradas . e .. de um diretório vazio */
    DirectoryItem* newDirectory = (DirectoryItem*) malloc(2 * sizeof(DirectoryItem));
    bcopy((unsigned char*) dirEntry1, (unsigned char*) newDirectory[0].name, 32);
    newDirectory[0].inode = inodeNumber;
    bcopy((unsigned char*) dirEntry2, (unsigned char*) newDirectory[1].name, 32);
    newDirectory[1].inode = superblock->workingDirectory;

    /* Escreve no disco o diretório criado no bloco correspondente ao diretório */
    bzero(buffer, 512);
    bcopy((unsigned char*) newDirectory, (unsigned char*) buffer, 2 * sizeof(DirectoryItem));
    block_write(blockNumber + DATA_BLOCK_START, buffer);

    free(buffer);

    printf(". inode = %d, .. inode = %d\n", newDirectory[0].inode, newDirectory[1].inode);

    /* Aloca inode para o novo diretório criado */
    Inode* newInode = (Inode*) malloc(sizeof(Inode));
    newInode->type = 1;
    newInode->size = 2 * sizeof(DirectoryItem);
    newInode->direct[0] = blockNumber + DATA_BLOCK_START;

    /* Salva o inode correspondente ao diretório no disco */
    save_inode(newInode, inodeNumber);

    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(superblock->workingDirectory);

    /* Checa se encontrou inode */
    if(inode == NULL)
        return -1;

    /* Calcula o bloco de inicio e termino correspondente ao diretório atual */
    int blockStart = inode->size / 512;
    int blockEnd = (inode->size + sizeof(DirectoryItem)) / 512;
    int byteStart = inode->size % 512;

    buffer = (char*) malloc((blockEnd - blockStart + 1) * 512 * sizeof(char));

    printf("***%d %d %d\n", blockStart, blockEnd, byteStart);

    /* Lê os blocos onde o diretório atual está salvo */
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    /* Cria entrada do novo diretório */
    DirectoryItem* directory = (DirectoryItem*) malloc(sizeof(DirectoryItem));
    bcopy((unsigned char*) fileName, (unsigned char*) directory->name, strlen(fileName) + 1);
    directory->inode = inodeNumber;
    // printf("***%s is allocated to inode number %d\n", directory->name, directory->inode);

    /* Guarda o novo diretório criado dentro do bloco de dados do diretório atual */
    bcopy((unsigned char*) directory, (unsigned char*) &buffer[byteStart], sizeof(DirectoryItem));
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_write(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    inode->size += sizeof(DirectoryItem);
    save_inode(inode, superblock->workingDirectory);

    /* Libera memória alocada dinamicamente */
    printf("Freeing...\n");
    free(buffer);
    free(directory);
    free(newDirectory);
    free(newInode);
    free(inode);
    
    return 0;
}

int fs_rmdir( char *fileName) {
    /* Verifica se o diretório existe */
    if(!dir_exists(fileName)) {
        return -1;
    }
    
    /* Certifica-se de que não seja os diretórios . ou .. */
    if(same_string(fileName, ".") || same_string(fileName, "..")) {
        return -1;
    }

    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(superblock->workingDirectory);

    /* Recupera a lista de diretórios dentro do diretório atual */
    DirectoryItem* directories = get_directory_items(superblock->workingDirectory);

    /* Verifica se foi retornado uma lista de diretórios */
    if(directories == NULL)
        return -1;

    /* Percorre a lista de diretórios */
    for(int i = 0; i < (inode->size / sizeof(DirectoryItem)); i++) {
        /* Verifica se o nome do diretório é o mesmo que está sendo procurado */
        if(same_string(directories[i].name, fileName)) {
            printf("Deleted\n");

            /* Aloca memória para o buffer */
            buffer = (char*) malloc(512 * sizeof(char));

            /* Carrega o inode do diretório a ser removido */
            Inode* dirInode = find_inode(directories[i].inode);

            /* Verifica se o inode não é de um diretório ou se o diretório não está vazio */
            if(dirInode->type != 1 || dirInode->size != (2 * sizeof(DirectoryItem)))
                return -1;

            /* Lê o vetor de bits dos inodes do disco */
            block_read(I_MAP_BLOCK, buffer);

            /* Carrega o vetor de bits dos inodes do buffer para um vetor */
            char imap[64];
            bcopy((unsigned char*) buffer, (unsigned char*) imap, 64);
            
            /* Seta para 0 o bit correspondente ao inode do diretório a ser removido */
            unset_bit(imap, (directories[i].inode / 8), (directories[i].inode % 8));

            /* Copia o vetor de bits dos inodes modificado de volta para o buffer e escreve de volta no disco */
            bcopy((unsigned char*) imap, (unsigned char*) buffer, 64);
            block_write(I_MAP_BLOCK, buffer);

            /* Move os diretórios uma posição a menos a partir do diretório removido */
            for(int j = i; j < (inode->size / sizeof(DirectoryItem)) - 1; j++) {
                directories[j] = directories[j + 1];
            }

            /* Altera o tamanho do diretório no seu inode e salva no disco o inode */
            inode->size -= sizeof(DirectoryItem);
            save_inode(inode, superblock->workingDirectory);

            /* Calcula quantos blocos são necessários para guardar os diretórios restantes */
            int numBlocks = (inode->size / 512) + 1;

            /* Copia a lista de diretórios modificada de volta para a variável buffer */
            buffer = realloc(buffer, numBlocks * 512 * sizeof(char));
            bcopy((unsigned char*) directories, (unsigned char*) buffer, inode->size);

            /* Salva no disco o conteúdo da variável buffer nos seus respectivos blocos de dados */
            for(int i = 0; i < numBlocks; i++) {
                block_write(inode->direct[i], &buffer[i * 512]);
            }

            /* Lê o vetor de bits dos blocos de dados do disco */
            block_read(D_MAP_BLOCK, buffer);

            /* Carrega o vetor de bits dos blocos de dados do buffer para um vetor */
            char dmap[251];
            bcopy((unsigned char*) buffer, (unsigned char*) dmap, 251);

            /* Calcula quantos blocos de dados o diretório removido estava ocupando */
            numBlocks = (dirInode->size / 512) + 1;

            /* Seta para 0 os bits correspondentes aos blocos de dados do diretório removido */
            for(int i = 0; i < numBlocks; i++) {
                unset_bit(dmap, (dirInode->direct[i] - 47) / 8, (dirInode->direct[i] - 47) % 8);
            }
            
            /* Copia o vetor de bits dos blocos de dados modificado de volta para o buffer e escreve de volta no disco */
            bcopy((unsigned char*) dmap, (unsigned char*) buffer, 251);
            block_write(D_MAP_BLOCK, buffer);
            
            /* Libera memória alocada dinâmicamente */
            free(buffer);
            free(dirInode);

            /* Encerra o loop pela lista de diretórios */
            break;
        }
    }

    /* Libera memória alocada dinâmicamente */
    free(directories);
    free(inode);

    return 0;
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

int fs_ls() {
    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(superblock->workingDirectory);
    /* Recupera a lista de diretórios dentro do diretório atual */
    DirectoryItem* directories = get_directory_items(superblock->workingDirectory);

    /* Verifica se foi retornado uma lista de diretórios */
    if(directories == NULL)
        return -1;

    /* Percorre a lista de diretórios */
    for(int i = 0; i < (inode->size / sizeof(DirectoryItem)); i++) {
        printf("\033[1;34m");
        printf("%s -> %d\n", directories[i].name, directories[i].inode);
        printf("\033[0m");
    }

    /* Libera memória alocada dinâmicamente */
    free(directories);
    free(inode);

    return 0;
}