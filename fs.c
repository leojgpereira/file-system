#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
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
int unlink_addarg_count = 0;

/* Guarda o nome das duas entradas iniciais de um diretório */
char dirEntry1[MAX_FILE_NAME] = ".";
char dirEntry2[MAX_FILE_NAME] = "..";

/* Inclui funções adicionais que criamos para uma melhor organização do código */
#include "fs_functions.c"

void fs_init(void) {
    block_init();

    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc(BLOCK_SIZE * sizeof(char));

    /* Zera todos os bytes da variável buffer */
    bzero(buffer, BLOCK_SIZE);
    /* Lê o primeiro bloco do disco (superbloco) e armazena na variável buffer */
    block_read(SUPERBLOCK_BLOCK_NUMBER, buffer);

    /* Inicializa a estrutura superblock */
    superblock = (Superblock*) malloc(sizeof(Superblock));
    /* Copia o conteúdo do primeiro bloco para a estrutura superblock */
    bcopy((unsigned char*) buffer, (unsigned char*) superblock, sizeof(Superblock));

    /* Libera memória alocada dinâmicamente */
    free(buffer);

    /* Checa se o disco está formatado comparando o valor do número mágico */
    if(!same_string(superblock->magicNumber, MAGIC_NUMBER)) {
        /* Invoca a função responsável por formatar o disco */
        fs_mkfs();
    }

    /* Cria tabela de descritores de arquivo em memória */
    fdTable = init_fd_table();
    numFileDescriptors = 0;
}

int fs_mkfs(void) {
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
    buffer = (char*) malloc(BLOCK_SIZE * sizeof(char));

    /* Zera os bytes do buffer, copia os bytes da estrutura superblock para o buffer e escreve o conteúdo no primeiro bloco (superblock) do disco */
    bzero(buffer, BLOCK_SIZE);
    bcopy((unsigned char*) superblock, (unsigned char*) buffer, sizeof(Superblock));
    block_write(SUPERBLOCK_BLOCK_NUMBER, buffer);

    /* Escreve bytes 0s nos blocos alocados para armazenar inodes */
    int bufferSize = BLOCK_SIZE * (int) ceil((double) (NUMBER_OF_INODES * sizeof(Inode)) / BLOCK_SIZE);
    buffer = realloc(buffer, bufferSize);
    bzero(buffer, bufferSize);

    /* Calcula quantos blocos são necessários para alocar a quantidade de inodes projetada */
    int numInodeBlocks = bufferSize / BLOCK_SIZE;

    /* Escreve os bytes 0s nos blocos alocados para guardar inodes */
    for(int i = 0; i < numInodeBlocks; i++) {
        block_write(i + INODE_START, &buffer[i * BLOCK_SIZE]);
    }

    /* Cria o primeiro inode como sendo o inode correspondente ao diretório raiz */
    Inode* inode = create_new_inode();
    inode->type = DIRECTORY;
    inode->size = 2 * sizeof(DirectoryItem);
    inode->direct[0] = DATA_BLOCK_START;
    save_inode(inode, ROOT_DIRECTORY_INODE);

    /* Inicializa um vetor de diretórios e insere as entradas '.' e '..' inicialmente como entradas do diretório raiz */
    DirectoryItem* rootDirectory = (DirectoryItem*) malloc(2 * sizeof(DirectoryItem));
    bcopy((unsigned char*) dirEntry1, (unsigned char*) rootDirectory[0].name, MAX_FILE_NAME);
    rootDirectory[0].inode = 0;
    bcopy((unsigned char*) dirEntry2, (unsigned char*) rootDirectory[1].name, MAX_FILE_NAME);
    rootDirectory[1].inode = 0;

    /* Escreve no primeiro bloco de dados o diretório raiz */
    buffer = realloc(buffer, BLOCK_SIZE);
    bzero(buffer, BLOCK_SIZE);
    bcopy((unsigned char*) rootDirectory, (unsigned char*) buffer, 2 * sizeof(DirectoryItem));
    block_write(DATA_BLOCK_START, buffer);

    /* Zera os bytes da variável buffer, seta o bit correspondende ao primeiro inode para 1 (ocupado - diretório raiz) e escreve o buffer no bloco onde fica o mapa de bits dos inodes */
    bzero(buffer, BLOCK_SIZE);
    set_bit(buffer, 0, 7);
    save_bitmap(buffer, I_MAP_BLOCK);

    /* Zera os bytes da variável buffer, seta o bit correspondende ao primeiro bloco de dados para 1 (ocupado - diretório raiz) e escreve o buffer no bloco onde fica o mapa de bits dos blocos de dados */
    bzero(buffer, BLOCK_SIZE);
    set_bit(buffer, 0, 7);
    save_bitmap(buffer, D_MAP_BLOCK);

    /* Libera memória alocada dinâmicamente */
    free(buffer);
    free(inode);
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
    if(flags == FS_O_RDONLY) {
        /* Verifica se encontrou o arquivo com nome fileName no diretório atual */
        if(directoryItem == NULL) {
            return -1;
        }
    /* FS O WRONLY ou FS O RDWR */
    } else if(flags == FS_O_WRONLY || flags == FS_O_RDWR) {
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
            if(type == DIRECTORY) {
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
        fd->wasTouched = 0;
    } else {
        /* Cria um novo descritor de arquivos */
        fd = (File*) malloc(sizeof(File));
        bcopy((unsigned char*) fileName, (unsigned char*) fd->name, strlen(fileName) + 1);
        fd->mode = flags;
        fd->offset = 0;
        fd->inode = directoryItem->inode;
        fd->directoryInode = superblock->workingDirectory;
        fd->wasTouched = 0;

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

    /* Carrega inode correspondente ao arquivo do descritor de arquivos */
    Inode* inode = find_inode(fdTable[fd]->inode);

    /* Guarda o nome do arquivo ao qual se refere o descritor de arquivos */
    char filename[MAX_FILE_NAME];
    bcopy((unsigned char*) fdTable[fd]->name, (unsigned char*) filename, strlen(fdTable[fd]->name) + 1);

    /* Guarda o inode do diretório ao qual o arquivo pertence */
    int directoryInode = fdTable[fd]->directoryInode;

    /* Libera a memória alocada para o descritor */
    free(fdTable[fd]);
    /* Atualiza o ponteiro no vetor de descritores de arquivos para NULL */
    fdTable[fd] = NULL;
    /* Decrementa o número de descritores de arquivos */
    numFileDescriptors--;

    /* Verifica se o número de referências para o arquivo é 0 */
    if(inode->linkCount < 1) {
        /* Modifica a contagem de argumentos adicionais da função fs_unlink para executar um unlink */
        unlink_addarg_count = 1;
        /* Chama função fs_unlink para remover arquivo, pois não há referências para ele */
        fs_unlink(filename, directoryInode);
        /* Modifica a contagem de argumentos adicionais da função fs_unlink para o valor padrão 0 */
        unlink_addarg_count = 0;
    }

    /* Libera memória alocada dinâmicamente */
    free(inode);

    return 0;
}

int fs_read(int fd, char *buf, int count) {
    /* Verifica o modo do descritor de arquivos e retorna erro se não é compatível com a operação a ser realizada */
    if(fdTable[fd]->mode != FS_O_RDONLY && fdTable[fd]->mode != FS_O_RDWR)
        return -1;

    /* Recupera inode através do descritor de arquivos */
    Inode* inode = find_inode(fdTable[fd]->inode);

    /* Verifica se o inode se trata de um diretório */
    if(inode->type != FILE_TYPE)
        return -1;

    /* Verifica se a posição do ponteiro está depois do fim do arquivo */
    if(fdTable[fd]->offset >= inode->size)
        return -1;
    
    /* Calcula quantos bytes podem ser lidos no máximo */
    int availableBytes = inode->size - fdTable[fd]->offset;
    int bytesCount;

    /* Verifica se a quantidade de bytes a ser lida é menor ou igual a quantidade de bytes disponíveis para leitura */
    if(count <= availableBytes) {
        bytesCount = count;
    } else {
        bytesCount = availableBytes;
    }

    /* Calcula os blocos de inicio e término da leitura e byte de início */
    int blockStart = fdTable[fd]->offset / BLOCK_SIZE;
    int blockEnd = (fdTable[fd]->offset + bytesCount - 1) / BLOCK_SIZE;
    int byteStart = fdTable[fd]->offset % BLOCK_SIZE;

    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc((blockEnd - blockStart + 1) * BLOCK_SIZE * sizeof(char));

    /* Lê os blocos de dados e guarda no buffer */
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Copia bytesCount bytes para a variável buf */
    bcopy((unsigned char*) &buffer[byteStart], (unsigned char*) buf, bytesCount);

    fdTable[fd]->offset += bytesCount;

    /* Libera memória alocada dinâmicamente */
    free(inode);
    free(buffer);

    /* Retorna quantidade de bytes lidos */
    return bytesCount;
}
    
int fs_write(int fd, char *buf, int count) {
    /* Verifica o modo do descritor de arquivos e retorna erro se não é compatível com a operação a ser realizada */
    if(fdTable[fd]->mode != FS_O_WRONLY && fdTable[fd]->mode != FS_O_RDWR)
        return -1;

    /* Recupera inode através do descritor de arquivos */
    Inode* inode = find_inode(fdTable[fd]->inode);

    /* Verifica se o inode se trata de um diretório */
    if(inode->type != FILE_TYPE)
        return -1;

    /* Verifica se o ponteiro de escrita está após o fim do arquivo */
    if(fdTable[fd]->offset > inode->size) {
        /* Preenche o intermédio entre o fim do arquivo e o ponteiro de escrita com bytes 0s */
        fill_with_zero_bytes(inode, fdTable[fd]);
    }

    /* Calcula os blocos de inicio e término da escrita e byte de início */
    int blockStart = fdTable[fd]->offset / BLOCK_SIZE;
    int blockEnd = (fdTable[fd]->offset + count - 1) / BLOCK_SIZE;
    int byteStart = fdTable[fd]->offset % BLOCK_SIZE;

    /* Carrega mapa de bits dos blocos de dados */
    char dmap[D_MAP_SIZE];
    load_bitmap(dmap, D_MAP_BLOCK);

    /* Variáveis de controle */
    int blockNumber;
    int blockCount = 0;

    /* Aloca blocos de dados para a escrita */
    for(int i = blockStart; i < blockEnd + 1 && i < (sizeof(inode->direct) / 4); i++) {
        /* Verifica se não há um bloco de dados alocado ni i-ésimo ponteiro direto */
        if(inode->direct[i] == -1) {
            /* Busca um bloco de dados livre */
            blockNumber = find_free_bit_number(dmap);

            /* Verifica se foi possível encontrar um bloco de dados disponível */
            if(blockNumber == -1)
                break;

            /* Atualiza o i-ésimo ponteiro direto com o número do bloco de dados livre encontrado */
            inode->direct[i] = blockNumber + DATA_BLOCK_START;
        }

        /* Incrementa a quantidade de blocos necessários para realizar a escrita */
        blockCount++;
    }

    /* Calcula quantos bytes estão disponíveis para escrita */
    int bytesCount = blockCount * BLOCK_SIZE - byteStart;

    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc(blockCount * BLOCK_SIZE * sizeof(char));

    /* Lê os blocos de dados onde será feita a escrita e guarda no buffer */
    for(int i = blockStart; i < blockStart + blockCount; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Verifica se a quantidade de bytes a ser escrita é menor ou igual a quantidade de bytes disponíveis */
    if(count <= bytesCount) {
        /* Copia count bytes para a variável buffer */
        bcopy((unsigned char*) buf, (unsigned char*) &buffer[byteStart], count);
    } else {
        /* Copia bytesCount bytes para a variável buffer */
        bcopy((unsigned char*) buf, (unsigned char*) &buffer[byteStart], bytesCount);
    }

    /* Escreve os bytes do buffer nos blocos de dados correspondente ao do arquivo */
    for(int i = blockStart; i < blockStart + blockCount; i++) {
        block_write(inode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Libera os blocos de dados sobrando quando o arquivo diminui de tamanho */
    if(!fdTable[fd]->wasTouched) {
        for(int i = blockStart + blockCount; i < ((inode->size - 1) / BLOCK_SIZE) + 1; i++) {
            unset_bit(dmap, (inode->direct[i] - DATA_BLOCK_START) / 8, (inode->direct[i] - DATA_BLOCK_START) % 8);
            inode->direct[i] = -1;
        }
    }

    /* Atualiza o deslocamento dentro do arquivo */
    fdTable[fd]->offset += count;

    /* Verifica se o deslocamento ficou maior que o tamanho do arquivo */
    if(fdTable[fd]->offset > inode->size || (fdTable[fd]->offset < inode->size && !fdTable[fd]->wasTouched)) {
        /* Atualiza o tamanho do arquivo no seu respectivo inode */
        inode->size = fdTable[fd]->offset;
    }

    /* Atualiza variável para dizer que já foi realizado alguma operação de escrita no arquivo */
    fdTable[fd]->wasTouched = 1;

    /* Salva o mapa de bits dos blocos de dados atualizado */
    save_bitmap(dmap, D_MAP_BLOCK);
    /* Salva o inode atualizado referente ao arquivo onde foi feita a escrita */
    save_inode(inode, fdTable[fd]->inode);

    /* Libera memória alocada dinâmicamente */
    free(inode);
    free(buffer);

    /* Verifica se a quantidade de bytes a ser escrita é menor ou igual a quantidade de bytes disponíveis */
    if(count <= bytesCount) {
        return count;
    } else {
        return bytesCount;
    }
}

int fs_lseek(int fd, int offset) {
    /* Verifica se o descritor está aberto */
    if(fdTable[fd] == NULL)
        return -1;

    /* Recupera inode através do descritor de arquivos */
    Inode* inode = find_inode(fdTable[fd]->inode);

    /* Verifica se o inode se trata de um diretório */
    if(inode->type != FILE_TYPE)
        return -1;

    /* Atualiza o deslocamento do arquivo aberto */
    fdTable[fd]->offset = offset;

    /* Libera memória alocada dinâmicamente */
    free(inode);

    /* Retorna o novo deslocamento */
    return offset;
}

int fs_mkdir(char *fileName) {
    /* Verifica se já existe um diretório com o mesmo nome */
    if(item_exists(fileName))
        return -1;

    /* Aloca memória para variável buffer */
    buffer = (char*) malloc(BLOCK_SIZE * sizeof(char));

    /* Declara vetor de bits para guardar o vetor de bits dos inodes */
    char imap[I_MAP_SIZE];
    load_bitmap(imap, I_MAP_BLOCK);

    /* Encontra inode livre */
    int inodeNumber = find_free_bit_number(imap);

    /* Checa se houve sucesso em encontrar um inode livre */
    if(inodeNumber == -1)
        return -1;

    /* Salva mapa de bits atualizado no disco */
    save_bitmap(imap, I_MAP_BLOCK);

    /* Declara vetor de bits para guardar o vetor de bits dos blocos de dados */
    char dmap[D_MAP_SIZE];
    load_bitmap(dmap, D_MAP_BLOCK);

    /* Encontra bloco de dados livre */
    int blockNumber = find_free_bit_number(dmap);

    /* Checa se houve sucesso em encontrar um bloco de dados livre */
    if(blockNumber == -1)
        return -1;

    /* Salva mapa de bits atualizado no disco */
    save_bitmap(dmap, D_MAP_BLOCK);

    /* Cria as entradas . e .. de um diretório vazio */
    DirectoryItem* newDirectory = (DirectoryItem*) malloc(2 * sizeof(DirectoryItem));
    bcopy((unsigned char*) dirEntry1, (unsigned char*) newDirectory[0].name, MAX_FILE_NAME);
    newDirectory[0].inode = inodeNumber;
    bcopy((unsigned char*) dirEntry2, (unsigned char*) newDirectory[1].name, MAX_FILE_NAME);
    newDirectory[1].inode = superblock->workingDirectory;

    /* Escreve no disco o diretório criado no bloco correspondente ao diretório */
    bzero(buffer, BLOCK_SIZE);
    bcopy((unsigned char*) newDirectory, (unsigned char*) buffer, 2 * sizeof(DirectoryItem));
    block_write(blockNumber + DATA_BLOCK_START, buffer);

    free(buffer);

    /* Aloca inode para o novo diretório criado */
    Inode* newInode = create_new_inode();
    newInode->type = DIRECTORY;
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
    int blockStart = inode->size / BLOCK_SIZE;
    int blockEnd = (inode->size + sizeof(DirectoryItem) - 1) / BLOCK_SIZE;
    int byteStart = inode->size % BLOCK_SIZE;

    /* Aloca memória para ler os bytes do diretório */
    buffer = (char*) malloc((blockEnd - blockStart + 1) * BLOCK_SIZE * sizeof(char));

    /* Lê os blocos onde o diretório atual está salvo */
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Cria entrada do novo diretório */
    DirectoryItem* directory = (DirectoryItem*) malloc(sizeof(DirectoryItem));
    bcopy((unsigned char*) fileName, (unsigned char*) directory->name, strlen(fileName) + 1);
    directory->inode = inodeNumber;

    /* Guarda o novo diretório criado dentro do bloco de dados do diretório atual */
    bcopy((unsigned char*) directory, (unsigned char*) &buffer[byteStart], sizeof(DirectoryItem));
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_write(inode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Atualiza o tamanho do diretório atual */
    inode->size += sizeof(DirectoryItem);
    save_inode(inode, superblock->workingDirectory);

    /* Libera memória alocada dinamicamente */
    free(buffer);
    free(directory);
    free(newDirectory);
    free(newInode);
    free(inode);
    
    return 0;
}

int fs_rmdir( char *fileName) {
    /* Verifica se o diretório existe */
    if(!item_exists(fileName))
        return -1;
    
    /* Certifica-se de que não seja os diretórios . ou .. */
    if(same_string(fileName, ".") || same_string(fileName, ".."))
        return -1;

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

            /* Aloca memória para o buffer */
            buffer = (char*) malloc(BLOCK_SIZE * sizeof(char));

            /* Carrega o inode do diretório a ser removido */
            Inode* dirInode = find_inode(directoryItems[i].inode);

            /* Verifica se o inode não é de um diretório ou se o diretório não está vazio */
            if(dirInode->type != DIRECTORY || dirInode->size != (2 * sizeof(DirectoryItem)))
                return -1;

            /* Carrega o vetor de bits dos inodes para um vetor */
            char imap[I_MAP_SIZE];
            load_bitmap(imap, I_MAP_BLOCK);
            
            /* Seta para 0 o bit correspondente ao inode do diretório a ser removido */
            unset_bit(imap, (directoryItems[i].inode / 8), (directoryItems[i].inode % 8));

            /* Copia o vetor de bits dos inodes modificado de volta para o disco */
            save_bitmap(imap, I_MAP_BLOCK);

            /* Move os diretórios uma posição a menos a partir do diretório removido */
            for(int j = i; j < (inode->size / sizeof(DirectoryItem)) - 1; j++) {
                directoryItems[j] = directoryItems[j + 1];
            }

            /* Altera o tamanho do diretório no seu inode e salva no disco o inode */
            inode->size -= sizeof(DirectoryItem);
            save_inode(inode, superblock->workingDirectory);

            int numBlocks;
            
            /* Calcula quantos blocos são necessários para guardar os diretórios restantes */
            numBlocks = ((inode->size - 1) / BLOCK_SIZE) + 1;

            /* Copia a lista de diretórios modificada de volta para a variável buffer */
            buffer = realloc(buffer, numBlocks * BLOCK_SIZE * sizeof(char));
            bcopy((unsigned char*) directoryItems, (unsigned char*) buffer, inode->size);

            /* Salva no disco o conteúdo da variável buffer nos seus respectivos blocos de dados */
            for(int i = 0; i < numBlocks; i++) {
                block_write(inode->direct[i], &buffer[i * BLOCK_SIZE]);
            }

            /* Carrega o vetor de bits dos blocos de dados para um vetor */
            char dmap[D_MAP_SIZE];
            load_bitmap(dmap, D_MAP_BLOCK);

            /* Calcula quantos blocos de dados o diretório removido estava ocupando */
            numBlocks = ((dirInode->size - 1) / BLOCK_SIZE) + 1;

            /* Seta para 0 os bits correspondentes aos blocos de dados do diretório removido */
            for(int i = 0; i < numBlocks; i++) {
                unset_bit(dmap, (dirInode->direct[i] - DATA_BLOCK_START) / 8, (dirInode->direct[i] - DATA_BLOCK_START) % 8);
            }
            
            /* Copia o vetor de bits dos blocos de dados modificado de volta para o disco */
            save_bitmap(dmap, D_MAP_BLOCK);
            
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
    if(directoryItem == NULL)
        return -1;

    /* Recupera o inode do diretório */
    Inode* inode = find_inode(directoryItem->inode);

    /* Verifica se realmente é um diretório */
    if(inode->type != DIRECTORY)
        return -1;

    /* Altera o inode que corresponde ao diretório de trabalho atual no superbloco */
    superblock->workingDirectory = directoryItem->inode;

    /* Libera memória alocada */
    free(directoryItem);
    free(inode);

    return 0;
}

int fs_link(char *old_fileName, char *new_fileName) {
    /* Verifica se já há um diretório/arquivo com o mesmo nome do soft link e retorna um erro caso sim */
    if(item_exists(new_fileName))
        return -1;

    /* Recupera o item referente ao arquivo buscado */
    DirectoryItem* directoryItem = get_directory_item(old_fileName);

    /* Verifica se não encontrou um arquivo com nome igual a old_fileName */
    if(directoryItem == NULL)
        return -1;

    /* Recupera o inode referente ao arquivo para o qual será criado um soft link */
    Inode* inode = find_inode(directoryItem->inode);

    /* Verifica se o inode corresponde a um inode de diretório e caso sim retorna erro */
    if(inode->type != FILE_TYPE)
        return -1;

    /* Cria um novo item de diretório com número de inode igual ao número do inode do arquivo old_fileName */
    DirectoryItem* newDirectoryItem = (DirectoryItem*) malloc(sizeof(DirectoryItem));
    bcopy((unsigned char*) new_fileName, (unsigned char*) newDirectoryItem->name, strlen(new_fileName) + 1);
    newDirectoryItem->inode = directoryItem->inode;

    /* Carrega o inode do diretório atual */
    Inode* workingDirectoryInode = find_inode(superblock->workingDirectory);

    /* Calcula o bloco de inicio e termino correspondente ao diretório atual e o próximo byte livre para ser escrito (byteStart) */
    int blockStart = workingDirectoryInode->size / BLOCK_SIZE;
    int blockEnd = (workingDirectoryInode->size + sizeof(DirectoryItem) - 1) / BLOCK_SIZE;
    int byteStart = workingDirectoryInode->size % BLOCK_SIZE;

    /* Declara vetor de bits para guardar o vetor de bits dos blocos de dados */
    char dmap[D_MAP_SIZE];
    load_bitmap(dmap, D_MAP_BLOCK);

    /* Variáveis de controle */
    int blockNumber;
    int blockCount = 0;

    /* Aloca blocos de dados para a escrita */
    for(int i = blockStart; i < blockEnd + 1 && i < (sizeof(workingDirectoryInode->direct) / 4); i++) {
        /* Verifica se não há um bloco de dados alocado ni i-ésimo ponteiro direto */
        if(workingDirectoryInode->direct[i] == -1) {
            /* Busca um bloco de dados livre */
            blockNumber = find_free_bit_number(dmap);

            /* Verifica se foi possível encontrar um bloco de dados disponível */
            if(blockNumber == -1)
                break;

            /* Atualiza o i-ésimo ponteiro direto com o número do bloco de dados livre encontrado */
            workingDirectoryInode->direct[i] = blockNumber + DATA_BLOCK_START;
        }

        /* Incrementa a quantidade de blocos necessários para realizar a escrita */
        blockCount++;
    }

    /* Calcula quantos bytes estão disponíveis para escrita */
    int bytesCount = blockCount * BLOCK_SIZE - byteStart;

    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc(blockCount * BLOCK_SIZE * sizeof(char));

    /* Lê os blocos de dados onde será feita a escrita e guarda no buffer */
    for(int i = blockStart; i < blockStart + blockCount; i++) {
        block_read(workingDirectoryInode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Verifica se a quantidade de bytes a ser escrita é menor ou igual a quantidade de bytes disponíveis */
    if(sizeof(DirectoryItem) <= bytesCount) {
        /* Copia sizeof(DirectoryItem) bytes para a variável buffer */
        bcopy((unsigned char*) newDirectoryItem, (unsigned char*) &buffer[byteStart], sizeof(DirectoryItem));
    } else {
        /* Retorna erro caso não haja espaço disponível para inserir o novo link */
        return -1;
    }

    /* Escreve os bytes do buffer nos blocos de dados correspondente ao do arquivo */
    for(int i = blockStart; i < blockStart + blockCount; i++) {
        block_write(workingDirectoryInode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Atualiza o tamanho do diretório atual */
    workingDirectoryInode->size += sizeof(DirectoryItem);
    /* Incrementa a quantidade de soft links do arquivo para o qual está sendo criado um novo link */
    inode->linkCount++;

    /* Salva o mapa de bits dos blocos de dados atualizado */
    save_bitmap(dmap, D_MAP_BLOCK);
    /* Salva o inode atualizado referente ao arquivo onde foi feita a escrita */
    save_inode(workingDirectoryInode, superblock->workingDirectory);
    /* Salva o inode modificado do arquivo que está sendo criado o soft link */
    save_inode(inode, directoryItem->inode);

    /* Libera memória alocada dinâmicamente */
    free(buffer);
    free(inode);
    free(workingDirectoryInode);
    free(directoryItem);
    free(newDirectoryItem);

    return 0;
}

int fs_unlink(char *fileName, ...) {
    /* Variável utilizada para guardar o número do inode do diretório onde o arquivo que será removido pertence */
    int dirInodeNumber;

    /* Verifica se não está sendo passado o número do inode do diretório na função fs_unlink */
    if(unlink_addarg_count == 0) {
        /* Se não, seta o número do inode para o número do inode do diretório atual */
        dirInodeNumber = superblock->workingDirectory;
    } else {
        /* Se sim, seta o número do inode para o número do inode recebido no parâmetro da função */
        va_list args;

        va_start(args, fileName);
        dirInodeNumber = va_arg(args, int); 

        va_end(args);
    }

    /* Recupera o item referente ao arquivo buscado */
    DirectoryItem* directoryItem = get_directory_item(fileName, dirInodeNumber);

    /* Verifica se não encontrou um arquivo com nome igual a fileName */
    if(directoryItem == NULL)
        return -1;

    /* Recupera o inode referente ao arquivo para o qual será excluido um soft link */
    Inode* softLinkInode = find_inode(directoryItem->inode);

    /* Verifica se o inode corresponde a um inode de diretório e caso sim retorna erro */
    if(softLinkInode->type != FILE_TYPE)
        return -1;

    /* Verifica se há um descritor de arquivos aberto e se o arquivo só tem uma referência para ele */
    for(int i = 0; i < FD_TABLE_SIZE; i++)  {
        if(fdTable[i] != NULL && fdTable[i]->inode == directoryItem->inode && softLinkInode->linkCount <= 1) {
            /* Decrementa número de referências do arquivo para 0 */
            softLinkInode->linkCount = 0;
            /* Salva o inode do arquivo modificado */
            save_inode(softLinkInode, directoryItem->inode);

            /* Atualiza o nome do descritor de arquivos para o nome da última referência para o arquivo */
            bcopy((unsigned char*) directoryItem->name, (unsigned char*) fdTable[i]->name, strlen(directoryItem->name) + 1);

            return 0;
        }
    }

    /* Recupera a lista de diretórios/arquivos dentro do diretório atual */
    DirectoryItem* directoryItems = get_directory_items(dirInodeNumber);

    /* Verifica se foi retornado uma lista de diretórios/arquivos */
    if(directoryItems == NULL)
        return -1;

    /* Recupera inode do diretório atual */
    Inode* inode = find_inode(dirInodeNumber);

    /* Percorre a lista de diretórios/arquivos */
    for(int i = 0; i < (inode->size / sizeof(DirectoryItem)); i++) {
        /* Verifica se o nome do arquivo é o mesmo que está sendo procurado */
        if(same_string(directoryItems[i].name, fileName)) {
            int numBlocks;

            /* Verifica se há somente uma referência para o arquivo */
            if(softLinkInode->linkCount <= 1) {
                /* Carrega o vetor de bits dos inodes para um vetor */
                char imap[I_MAP_SIZE];
                load_bitmap(imap, I_MAP_BLOCK);

                /* Seta para 0 o bit correspondente ao inode do arquivo a ser removido */
                unset_bit(imap, (directoryItems[i].inode / 8), (directoryItems[i].inode % 8));

                /* Salva o vetor de bits dos inodes atualizado */
                save_bitmap(imap, I_MAP_BLOCK);

                /* Carrega o vetor de bits dos blocos de dados para um vetor */
                char dmap[D_MAP_SIZE];
                load_bitmap(dmap, D_MAP_BLOCK);

                /* Calcula quantos blocos de dados o arquivo removido estava ocupando */
                numBlocks = (int) ceil((double) softLinkInode->size / BLOCK_SIZE);

                /* Seta para 0 os bits correspondentes aos blocos de dados do arquivo removido */
                for(int i = 0; i < numBlocks; i++) {
                    unset_bit(dmap, (softLinkInode->direct[i] - DATA_BLOCK_START) / 8, (softLinkInode->direct[i] - DATA_BLOCK_START) % 8);
                }

                /* Salva o vetor de bits dos blocos de dados atualizado */
                save_bitmap(dmap, D_MAP_BLOCK);
            } else {
                /* Decrementa a quantidade de referências para o arquivo */
                softLinkInode->linkCount--;
                /* Salva o inode atualizado do arquivo para o qual foi decrementado uma referência */
                save_inode(softLinkInode, directoryItems[i].inode);
            }

            /* Move os itens uma posição a menos a partir do arquivo removido */
            for(int j = i; j < (inode->size / sizeof(DirectoryItem)) - 1; j++) {
                directoryItems[j] = directoryItems[j + 1];
            }

            /* Altera o tamanho do diretório atual no seu inode e salva no disco o inode */
            inode->size -= sizeof(DirectoryItem);
            save_inode(inode, dirInodeNumber);

            /* Calcula quantos blocos são necessários para guardar os diretórios/arquivos restantes */
            numBlocks = (int) ceil((double) inode->size / BLOCK_SIZE);

            /* Copia a lista de diretórios/arquivos modificada de volta para a variável buffer */
            buffer = (char*) malloc(numBlocks * BLOCK_SIZE * sizeof(char));
            bcopy((unsigned char*) directoryItems, (unsigned char*) buffer, inode->size);

            /* Salva no disco o conteúdo da variável buffer nos seus respectivos blocos de dados */
            for(int i = 0; i < numBlocks; i++) {
                block_write(inode->direct[i], &buffer[i * BLOCK_SIZE]);
            }
            
            /* Libera memória alocada dinâmicamente */
            free(buffer);

            /* Encerra o loop pela lista de diretórios */
            break;
        }
    }

    /* Libera memória alocada dinâmicamente */
    free(directoryItems);
    free(directoryItem);
    free(softLinkInode);
    free(inode);

    return 0;
}

int fs_stat( char *fileName, fileStat *buf) {
        /* Recupera o item referente ao arquivo buscado */
    DirectoryItem* directoryItem = get_directory_item(fileName);

    /* Verifica se não encontrou um arquivo com nome igual a old_fileName */
    if(directoryItem == NULL)
        return -1;

    /* Recupera o inode referente ao arquivo para o qual será criado um soft link */
    Inode* inode = find_inode(directoryItem->inode);

    /* Copia as informações sobre o arquivo/diretório para o buffer passado por referência */
    buf->inodeNo = directoryItem->inode;
    buf->type = inode->type;
    buf->links = inode->linkCount;
    buf->size = inode->size;
    buf->numBlocks = ceil((double) inode->size / BLOCK_SIZE);

    return 0;
}

int fs_ls() {
    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(superblock->workingDirectory);

    /* Recupera a lista de diretórios dentro do diretório atual */
    DirectoryItem* directoryItems = get_directory_items(superblock->workingDirectory);

    /* Verifica se foi retornado uma lista de diretórios */
    if(directoryItems == NULL)
        return -1;

    /* Ponteiro para inode de um item de diretório */
    Inode* itemInode;

    /* Percorre a lista de diretórios */
    for(int i = 0; i < (inode->size / sizeof(DirectoryItem)); i++) {
        /* Busca o inode referente ao arquivo/diretório */
        itemInode = find_inode(directoryItems[i].inode);

        /* Muda a cor do texto para nomes de diretórios */
        if(itemInode->type == DIRECTORY)
            printf("\033[1;34m");

        /* Imprime nomes dos arquivos/diretórios */
        printf("%-3d %-3d %-32s\n", directoryItems[i].inode, itemInode->linkCount, directoryItems[i].name);

        /* Retaura a cor do texto padrão após mudar a cor para nomes de diretórios */
        if(itemInode->type == DIRECTORY)
            printf("\033[0m");

        /* Libera memória alocada dinâmicamente */
        free(itemInode);
    }

    /* Libera memória alocada dinâmicamente */
    free(directoryItems);
    free(inode);

    return 0;
}