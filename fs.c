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
File** fdTable;
char* buffer;
int numFileDescriptors;

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
    fdTable = init_fd_table();
    numFileDescriptors = 0;
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
    /* Recupera posição do descritor se ele já existe ou -1 caso contrário */
    int openedFileDescriptor = fd_exists(fileName);

    /* Verifica se não há descritores disponíveis e se o arquio já não tem um descritor associado (aberto anteriormente) */
    if(numFileDescriptors >= FD_TABLE_SIZE && openedFileDescriptor < 0)
        return -1;
    
    /* Recupera o item que representa o arquivo dentro do diretório atual ou NULL caso não encontre */
    DirectoryItem* directoryItem = NULL;
    directoryItem = get_directory_item(fileName);

    /* FS O RDONLY */
    if(flags == 1) {
        /* Verifica se encontrou o arquivo com nome fileName no diretório atual */
        if(directoryItem == NULL) {
            return -1;
        }
    /* FS O WRONLY ou FS O RDWR */
    } else if(flags == 2 || flags == 3) {
        /* Verifica se encontrou o arquivo com nome fileName no diretório atual */
        if(directoryItem == NULL) {
            /* Cria um arquivo no diretório atual caso ele não exista */
            directoryItem = create_new_file(fileName);

            /* Retorna -1 caso não seja possível criar um novo arquivo */
            if(directoryItem == NULL)
                return -1;
        } else {
            /* Guarda o tipo de item (1: diretório, 0: arquivo) */
            int type;

            /* Carrega o inode correspondente ao item buscado e recupera seu tipo */
            Inode* inode = find_inode(directoryItem->inode);
            type = inode->type;
            free(inode);

            /* Verifica se é um diretório e retorna um erro em caso afirmativo */
            if(type == 1) {
                /* Libera memória alocada dinâmicamente */
                free(directoryItem);
                return -1;
            }
        }
    /* Retorna erro se não foi passado um dos três modos de acesso */
    } else {
        return -1;
    }

    /* Declara um ponteiro para um descritor de arquivos */
    File* fd;

    /* Verifica se já existe um descritor de arquivos para o arquivo solicitado */
    if(openedFileDescriptor >= 0) {
        /* Atualiza as informações do descritor já existente */
        fd = fdTable[openedFileDescriptor];
        fd->mode = flags;
        fd->offset = 0;
    } else {
        /* Cria um novo descritor de arquivos */
        fd = (File*) malloc(sizeof(File));
        bcopy((unsigned char*) fileName, (unsigned char*) fd->name, strlen(fileName) + 1);
        fd->mode = flags;
        fd->offset = 0;
        fd->inode = directoryItem->inode;

        /* Encontra uma posição NULL dentro do vetor de descritores de arquivo */
        int pos;
        for(pos = 0; pos < FD_TABLE_SIZE; pos++) {
            if(fdTable[pos] == NULL)
                break;
        }

        /* Guarda o novo descritor de arquivo na posição pos do vetor de descritores de arquivos */
        fdTable[pos] = fd;
        /* Incrementa a variável que contabiliza a quantidade de descritores sendo utilizados no momento */
        numFileDescriptors++;

        /* Guarda a posição do descritor dentro do vetor */
        openedFileDescriptor = pos;
    }

    /* Libera memória alocada dinâmicamente */
    free(directoryItem);

    /* Retorna o descritor de arquivo correspondente para o arquivo solicitado */
    return openedFileDescriptor;
}

int fs_close(int fd) {
    /* Verifica se o número do descritor é válido (entre 0 e FD_TABLE_SIZE-1 -- incluso) e se há um descritor alocado na posição fd fo vetor de descritores de arquivos */
    if(fd < 0 || fd >= FD_TABLE_SIZE || fdTable[fd] == NULL)
        return -1;

    /* Libera a memória alocada para o descritor */
    free(fdTable[fd]);
    /* Atualiza o ponteiro no vetor de descritores de arquivos para NULL */
    fdTable[fd] = NULL;
    /* Decrementa o número de descritores de arquivos */
    numFileDescriptors--;

    return 0;
}

int fs_read( int fd, char *buf, int count) {
    return -1;
}
    
int fs_write( int fd, char *buf, int count) {
    Inode* inode = find_inode(fdTable[fd]->inode);

    if(inode->type != 0)
        return -1;

    int blockStart = inode->size / 512;
    int blockEnd = (inode->size + count) / 512;
    int byteStart = fdTable[fd]->offset;

    printf("%d %d %d %d\n", count, blockStart, blockEnd, byteStart);

    char dmap[251];
    load_bitmap(dmap, D_MAP_BLOCK);

    int blockNumber;

    for(int i = blockStart; i < blockEnd + 1; i++) {
        printf(">>>%d\n", inode->direct[i]);
        if(inode->direct[i] == -1) {
            blockNumber = find_free_bit_number(dmap);

            if(blockNumber == -1)
                return -1;

            inode->direct[i] = blockNumber + DATA_BLOCK_START;

            printf("block number = %d is free\n", blockNumber + DATA_BLOCK_START);
        }
    }

    char* buffer = (char*) malloc((blockEnd - blockStart + 1) * 512 * sizeof(char));

    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    printf("+++%s\n", buf);
    bcopy((unsigned char*) buf, (unsigned char*) &buffer[byteStart], count);

    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_write(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    inode->size += count;
    fdTable[fd]->offset += count;

    save_bitmap(dmap, D_MAP_BLOCK);
    save_inode(inode, fdTable[fd]->inode);

    free(inode);
    free(buffer);
    
    return count;
}

int fs_lseek( int fd, int offset) {
    return -1;
}

int fs_mkdir(char *fileName) {
    /* Verifica se já existe um diretório com o mesmo nome */
    if(item_exists(fileName)) {
        return -1;
    }

    /* Aloca memória para variável buffer */
    buffer = (char*) malloc(512 * sizeof(char));

    /* Declara vetor de bits para guardar o vetor de bits dos inodes */
    char imap[64];
    load_bitmap(imap, I_MAP_BLOCK);

    /* Encontra inode livre */
    int inodeNumber = find_free_bit_number(imap);

    /* Checa se houve sucesso em encontrar um inode livre */
    if(inodeNumber == -1)
        return -1;

    printf("inode number = %d is free\n", inodeNumber);

    /* Salva mapa de bits atualizado no disco */
    save_bitmap(imap, I_MAP_BLOCK);

    /* Declara vetor de bits para guardar o vetor de bits dos blocos de dados */
    char dmap[251];
    load_bitmap(dmap, D_MAP_BLOCK);

    /* Encontra bloco de dados livre */
    int blockNumber = find_free_bit_number(dmap);

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
    if(!item_exists(fileName)) {
        return -1;
    }
    
    /* Certifica-se de que não seja os diretórios . ou .. */
    if(same_string(fileName, ".") || same_string(fileName, "..")) {
        return -1;
    }

    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(superblock->workingDirectory);

    /* Recupera a lista de diretórios dentro do diretório atual */
    DirectoryItem* directoryItems = get_directory_items(superblock->workingDirectory);

    /* Verifica se foi retornado uma lista de diretórios */
    if(directoryItems == NULL)
        return -1;

    /* Percorre a lista de diretórios */
    for(int i = 0; i < (inode->size / sizeof(DirectoryItem)); i++) {
        /* Verifica se o nome do diretório é o mesmo que está sendo procurado */
        if(same_string(directoryItems[i].name, fileName)) {
            printf("Deleted\n");

            /* Aloca memória para o buffer */
            buffer = (char*) malloc(512 * sizeof(char));

            /* Carrega o inode do diretório a ser removido */
            Inode* dirInode = find_inode(directoryItems[i].inode);

            /* Verifica se o inode não é de um diretório ou se o diretório não está vazio */
            if(dirInode->type != 1 || dirInode->size != (2 * sizeof(DirectoryItem)))
                return -1;

            /* Lê o vetor de bits dos inodes do disco */
            block_read(I_MAP_BLOCK, buffer);

            /* Carrega o vetor de bits dos inodes do buffer para um vetor */
            char imap[64];
            bcopy((unsigned char*) buffer, (unsigned char*) imap, 64);
            
            /* Seta para 0 o bit correspondente ao inode do diretório a ser removido */
            unset_bit(imap, (directoryItems[i].inode / 8), (directoryItems[i].inode % 8));

            /* Copia o vetor de bits dos inodes modificado de volta para o buffer e escreve de volta no disco */
            bcopy((unsigned char*) imap, (unsigned char*) buffer, 64);
            block_write(I_MAP_BLOCK, buffer);

            /* Move os diretórios uma posição a menos a partir do diretório removido */
            for(int j = i; j < (inode->size / sizeof(DirectoryItem)) - 1; j++) {
                directoryItems[j] = directoryItems[j + 1];
            }

            /* Altera o tamanho do diretório no seu inode e salva no disco o inode */
            inode->size -= sizeof(DirectoryItem);
            save_inode(inode, superblock->workingDirectory);

            /* Calcula quantos blocos são necessários para guardar os diretórios restantes */
            int numBlocks = (inode->size / 512) + 1;

            /* Copia a lista de diretórios modificada de volta para a variável buffer */
            buffer = realloc(buffer, numBlocks * 512 * sizeof(char));
            bcopy((unsigned char*) directoryItems, (unsigned char*) buffer, inode->size);

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
    free(directoryItems);
    free(inode);

    return 0;
}

int fs_cd(char *dirName) {
    /* Recupera o item de diretório correspondente a dirName */
    DirectoryItem* directoryItem = get_directory_item(dirName);

    /* Verifica se não existe o diretório */
    if(directoryItem == NULL) {
        return -1;
    }

    /* Recupera o inode do diretório */
    Inode* inode = find_inode(directoryItem->inode);

    /* Verifica se realmente é um diretório */
    if(inode->type != 1)
        return -1;

    /* Altera o inode que corresponde ao diretório de trabalho atual no superbloco */
    superblock->workingDirectory = directoryItem->inode;

    /* Libera memória alocada */
    free(directoryItem);
    free(inode);

    return 0;
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
    DirectoryItem* directoryItems = get_directory_items(superblock->workingDirectory);

    /* Verifica se foi retornado uma lista de diretórios */
    if(directoryItems == NULL)
        return -1;

    /* Percorre a lista de diretórios */
    for(int i = 0; i < (inode->size / sizeof(DirectoryItem)); i++) {
        printf("\033[1;34m");
        printf("%s -> %d\n", directoryItems[i].name, directoryItems[i].inode);
        printf("\033[0m");
    }

    /* Libera memória alocada dinâmicamente */
    free(directoryItems);
    free(inode);

    return 0;
}