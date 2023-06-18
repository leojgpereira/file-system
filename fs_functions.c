File** init_fd_table() {
    /* Cria um vetor de descritores de arquivos */
    File** fd_table = (File**) malloc(FD_TABLE_SIZE * sizeof(File));

    /* Seta todas as posições do vetor de descritores de arquivos para NULL */
    for(int i = 0; i < FD_TABLE_SIZE; i++) {
        fd_table[i] = NULL;
    }

    /* Retorna referência para o vetor de descritores de arquivos */
    return fd_table;
}

void set_bit(char* bytes, int index, int n) {
    /* Seta n-esimo bit de um byte na posição index de um vetor de bytes para 1 */
    bytes[index] = ((1 << n) | bytes[index]);
}

void unset_bit(char* bytes, int index, int n) {
    /* Seta n-esimo bit de um byte na posição index de um vetor de bytes para 0 */
    bytes[index] = bytes[index] & (~(1 << (7 - n)));
}

int get_bit(char* bytes, int index, int n) {
    /* Retorna n-esimo bit de um byte na posição index de um vetor de bytes */
    return (bytes[index] >> n) & 1;
}

void load_bitmap(char* bitmap, int block) {
    int size;

    /* Define tamanho do vetor de bits */
    if(block == D_MAP_BLOCK)
        size = D_MAP_SIZE;
    else
        size = I_MAP_SIZE;

    /* Aloca memória para variável buffer */
    char* buffer = (char*) malloc(BLOCK_SIZE * sizeof(char));

    /* Busca o vetor de bits e copia para variável bitmap */
    block_read(block, buffer);
    bcopy((unsigned char*) buffer, (unsigned char*) bitmap, size);

    /* Libera memória alocada para a variável buffer */
    free(buffer);
}

int find_free_bit_number(char* bitmap, int size) {
    /* Variável utilizada para guardar número de um bit livre */
    int bitNumber = -1;

    /* Percorre cada byte do vetor de bits */
    for(int i = 0; i < size; i++) {
        /* Percorre cada bit do byte do vetor de bits */
        for(int j = 7; j > -1; j--) {
            /* Descobre o valor de um bit dentro de um byte */
            int bit = get_bit(bitmap, i, j);

            /* Verifica se o bit é 0, ou seja, se o bit correspondente está livre */
            if(bit == 0) {
                /* Calcula o número do bit livre */
                bitNumber = 7 - j + (i * 8);

                /* Seta bit para 1, marcando o bit correspondente como ocupado */
                set_bit(bitmap, i, j);
                break;
            }
        }

        /* Encerra loop se já encontrou um bit livre */
        if(bitNumber != -1)
            break;
    }

    /* Retorna número do bit livre ou -1 caso não tenha bits livres */
    return bitNumber;
}

void save_bitmap(char* bitmap, int block) {
    int size;

    /* Define tamanho do vetor de bits */
    if(block == D_MAP_BLOCK)
        size = D_MAP_SIZE;
    else
        size = I_MAP_SIZE;

    /* Aloca memória para variável buffer */
    char* buffer = (char*) malloc(BLOCK_SIZE * sizeof(char));
    bzero(buffer, BLOCK_SIZE);

    /* Copia vetor de bits para a variável buffer */
    bcopy((unsigned char*) bitmap, (unsigned char*) buffer, size);
    block_write(block, buffer);

    /* Libera memória alocada para a variável buffer */
    free(buffer);
}

Inode* find_inode(int inodeNumber) {
    /* Verifica se o número de inode é um número válido */
    if(inodeNumber < 0 || inodeNumber > NUMBER_OF_INODES - 1)
        return NULL;

    /* Aloca memória para a variável buffer */
    char* buffer = (char*) malloc(2 * BLOCK_SIZE * sizeof(char));

    /* Zera todos os bytes da variável buffer e copia o inode correspondente para o buffer */
    bzero(buffer, 2 * BLOCK_SIZE);

    /* Calcula em quais blocos estão o inode a ser encontrado */
    int start = inodeNumber * sizeof(Inode);
    int end = start + sizeof(Inode) - 1;
    int blockStart = start / BLOCK_SIZE;
    int blockEnd = end / BLOCK_SIZE;

    /* Copia os blocos para a variável buffer */
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(i + INODE_START, &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Copia os bytes do buffer para uma estrutura inode */
    Inode* inode = (Inode*) malloc(sizeof(Inode));
    bcopy((unsigned char*) &buffer[start % BLOCK_SIZE], (unsigned char*) inode, sizeof(Inode));

    /* Libera memória alocada */
    free(buffer);

    /* Retorna a referência para o inode encontrado */
    return inode;
}

Inode* create_new_inode() {
    /* Aloca dinâmicamente um novo inode */
    Inode* inode;
    inode = (Inode*) malloc(sizeof(Inode));
    
    /* Define o valor inicial da contagem de links para 1 */
    inode->linkCount = 1;

    /* Retorna um ponteiro para o inode criado */
    return inode;
}

void save_inode(Inode* inode, int inodeNumber) {
    /* Aloca memória para a variável buffer */
    char* buffer_ = (char*) malloc(2 * BLOCK_SIZE * sizeof(char));

    /* Zera todos os bytes da variável buffer e copia o inode correspondente para o buffer */
    bzero(buffer_, 2 * BLOCK_SIZE);

    /* Calcula em quais blocos estão o inode a ser salvo */
    int start = inodeNumber * sizeof(Inode);
    int end = start + sizeof(Inode) - 1;
    int blockStart = start / BLOCK_SIZE;
    int blockEnd = end / BLOCK_SIZE;

    /* Copia os blocos para a variável buffer */
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(i + INODE_START, &buffer_[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Copia os bytes correspondentes ao inode na variável buffer na posição correspondente a posição do inode */
    bcopy((unsigned char*) inode, (unsigned char*) &buffer_[start % BLOCK_SIZE], sizeof(Inode));

    /* Copia os blocos da variável buffer para o disco */
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_write(i + INODE_START, &buffer_[(i - blockStart) * BLOCK_SIZE]);
    }

    /* Libera memória alocada dinâmicamente */
    free(buffer_);
}

DirectoryItem* get_directory_items(int inodeNumber) {
    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(inodeNumber);

    /* Verifica se o inode corresponde a um inode de diretório */
    if(inode->type != DIRECTORY)
        return NULL;

    /* Calcula quantos blocos de dados o diretório utiliza */
    int numBlocks = (int) ceil((double) inode->size / BLOCK_SIZE);

    /* Cria uma variável buffer para armazenar os blocos de dados */
    char* buffer = (char*) malloc(numBlocks * BLOCK_SIZE * sizeof(char));

    /* Lê cada bloco de dados correspondente ao diretório a ser consultado */
    for(int i = 0; i < numBlocks; i++) {
        block_read(inode->direct[i], &buffer[i * BLOCK_SIZE]);
    }

    /* Aloca um vetor de diretórios dinamicamente */
    DirectoryItem* directoryItems = (DirectoryItem*) malloc(inode->size);
    /* Copia os bytes do buffer para o vetor de diretórios */
    bcopy((unsigned char*) buffer, (unsigned char*) directoryItems, inode->size);

    /* Libera a memória alocada dinâmicamente */
    free(buffer);
    free(inode);

    /* Retorna um ponteiro para a lista de diretórios */
    return directoryItems;
}

DirectoryItem* get_directory_item(char* itemName, ...) {
    /* Variável utilizada para guardar o número do inode do diretório onde o arquivo que será buscado pertence */
    int dirInodeNumber;

    /* Verifica se não está sendo passado um parâmetro adicional para a função */
    if(unlink_addarg_count == 0) {
        /* Se não, seta o número do inode para o número do inode do diretório atual */
        dirInodeNumber = superblock->workingDirectory;
    } else {
        /* Se sim, seta o número do inode para o número do inode recebido no parâmetro da função */
        va_list args;

        va_start(args, itemName);

        dirInodeNumber = va_arg(args, int);

        va_end(args);
    }

    /* Recupera o inode do diretório */
    Inode* inode = find_inode(dirInodeNumber);
    /* Recupera a lista de diretórios dentro do diretório */
    DirectoryItem* directoryItems = get_directory_items(dirInodeNumber);

    /* Declara ponteiro para um item de diretório */
    DirectoryItem* directoryItem = NULL;

    /* Verifica se foi retornado uma lista de items de diretórios */
    if(directoryItems == NULL)
        return NULL;

    /* Percorre a lista de itens do diretórios */
    for(int i = 0; i < (inode->size / sizeof(DirectoryItem)); i++) {
        /* Verifica se há um diretório igual a itemName */
        if(same_string(itemName, directoryItems[i].name)) {
            /* Aloca memória para guardar o item encontrado */
            directoryItem = (DirectoryItem*) malloc(sizeof(DirectoryItem));

            /* Copia os bytes do item encontrado no vetor de diretórios para uma variável */
            bcopy((unsigned char*) &directoryItems[i], (unsigned char*) directoryItem, sizeof(DirectoryItem));

            /* Encerra o loop */
            break;
        }
    }

    /* Libera memória alocada dinâmicamente */
    free(inode);
    free(directoryItems);

    /* Retorna ponteiro para o item encontrado */
    return directoryItem;
}

int item_exists(char* itemName) {
    /* Variável de controle */
    int exists = 0;

    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(superblock->workingDirectory);
    /* Recupera a lista de diretórios dentro do diretório atual */
    DirectoryItem* directoryItems = get_directory_items(superblock->workingDirectory);

    /* Verifica se foi retornado uma lista de diretórios */
    if(directoryItems == NULL)
        return 1;

    /* Percorre a lista de diretórios */
    for(int i = 0; i < (inode->size / sizeof(DirectoryItem)) && exists == 0; i++) {
        /* Verifica se há um diretório igual a itemName */
        if(same_string(itemName, directoryItems[i].name)) {
            /* Seta variável de controle para verdadeiro */
            exists = 1;
        }
    }

    /* Libera memória alocada dinâmicamente */
    free(inode);
    free(directoryItems);

    /* Retorna se há ou não um diretório igual a itemName no diretório atual */
    return exists;
}

DirectoryItem* create_new_file(char* fileName) {
    DirectoryItem* newDirectoryItem = NULL;

    /* Verifica se já existe um diretório/arquivo com o mesmo nome */
    if(item_exists(fileName)) {
        return NULL;
    }

    /* Declara vetor de bits para guardar o vetor de bits dos inodes */
    char imap[I_MAP_SIZE];
    load_bitmap(imap, I_MAP_BLOCK);

    /* Encontra inode livre */
    int inodeNumber = find_free_bit_number(imap, I_MAP_SIZE);

    /* Checa se houve sucesso em encontrar um inode livre */
    if(inodeNumber == -1)
        return NULL;

    /* Salva mapa de bits atualizado no disco */
    save_bitmap(imap, I_MAP_BLOCK);

    /* Aloca inode para o novo arquivo criado */
    Inode* newInode = create_new_inode();
    newInode->type = FILE_TYPE;
    newInode->size = 0;

    for(int i = 0; i < (sizeof(newInode->direct) / 4); i++) {
        newInode->direct[i] = -1;
    }

    newInode->singleIndirect = -1;

    /* Salva o inode correspondente ao arquivo no disco */
    save_inode(newInode, inodeNumber);

    /* Recupera inode do diretório de trabalho atual */
    Inode* inode = find_inode(superblock->workingDirectory);

    /* Calcula o bloco de inicio e termino correspondente ao diretório atual */
    int blockStart = inode->size / BLOCK_SIZE;
    int blockEnd = (inode->size + sizeof(DirectoryItem) - 1) / BLOCK_SIZE;
    int byteStart = inode->size % BLOCK_SIZE;

    char* buffer = (char*) malloc((blockEnd - blockStart + 1) * BLOCK_SIZE * sizeof(char));

    /* Lê os blocos onde o diretório atual está salvo */
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    newDirectoryItem = (DirectoryItem*) malloc(sizeof(DirectoryItem));
    bcopy((unsigned char*) fileName, (unsigned char*) newDirectoryItem->name, strlen(fileName) + 1);
    newDirectoryItem->inode = inodeNumber;

    /* Guarda o novo arquivo criado dentro do bloco de dados do diretório atual */
    bcopy((unsigned char*) newDirectoryItem, (unsigned char*) &buffer[byteStart], sizeof(DirectoryItem));
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_write(inode->direct[i], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }

    inode->size += sizeof(DirectoryItem);
    save_inode(inode, superblock->workingDirectory);

    /* Libera memória alocada dinamicamente */
    free(buffer);
    free(newInode);
    free(inode);

    return newDirectoryItem;
}

int fd_exists(char* fileName) {
    /* Variável de controle */
    int index = -1;

    /* Percorre todas as posições do vetor de descritores de arquivos */
    for(int i = 0; i < FD_TABLE_SIZE; i++) {
        /* Verifica se a posição tem um descritor válido e se esse tem o mesmo nome de arquivo que fileName */
        if(fdTable[i] != NULL && same_string(fileName, fdTable[i]->name) && fdTable[i]->directoryInode == superblock->workingDirectory) {
            /* Atualiza a variável de controle para guardar a posição do vetor que já contém um descritor de arquivo aberto para o arquivo fileName */
            index = i;

            /* Encerra o loop */
            break;
        }
    }

    /* Retorna o índice do descritor ou -1 caso ainda não haja um descritor aberto para o arquivo com nome igual a fileName */
    return index;
}

void read_n_blocks(int addresses[], char* buffer, int blockStart, int blockEnd) {
    int singleIndirectBlockStart = blockStart;

    for(int i = blockStart; i < blockEnd + 1 && i < 10; i++) {
        block_read(addresses[i + 1], &buffer[(i - blockStart) * BLOCK_SIZE]);

        singleIndirectBlockStart++;
    }

    /* Lê os blocos de dados onde será feita a escrita e guarda no buffer */
    for(int i = singleIndirectBlockStart; i < blockEnd + 1 && i < 138; i++) {
        block_read(addresses[i + 1], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }
}

void write_n_blocks(int addresses[], char* buffer, int blockStart, int blockEnd) {
    int singleIndirectBlockStart = blockStart;

    for(int i = blockStart; i < blockEnd + 1 && i < 10; i++) {
        block_write(addresses[i + 1], &buffer[(i - blockStart) * BLOCK_SIZE]);

        singleIndirectBlockStart++;
    }

    /* Lê os blocos de dados onde será feita a escrita e guarda no buffer */
    for(int i = singleIndirectBlockStart; i < blockEnd + 1 && i < 138; i++) {
        block_write(addresses[i + 1], &buffer[(i - blockStart) * BLOCK_SIZE]);
    }
}

void fill_with_zero_bytes(Inode* inode, File* fd) {
    /* Calcula os blocos de inicio e término da escrita e byte de início */
    int blockStart = inode->size / BLOCK_SIZE;
    int blockEnd = (fd->offset - 1) / BLOCK_SIZE;
    int byteStart = inode->size % BLOCK_SIZE;

    /* Vetor para guardar os possíveis números de blocos de dados */
    int addresses[139];

    /* Inicializa todos com -1 */
    for(int i = 0; i < 139; i++)
        addresses[i] = -1;

    /* Atribui a primeira ao número do bloco do single indirect */
    addresses[0] = inode->singleIndirect;

    /* Copia os números dos blocos de dados diretos */
    for(int i = 1; i < 11; i++)
        addresses[i] = inode->direct[i - 1];

    /* Copia os números dos blocos de dados indiretos simples */
    if(inode->singleIndirect != -1) {
        AddressBlock* addressBlock = (AddressBlock*) malloc(sizeof(AddressBlock));
        char* buffer = (char*) malloc(BLOCK_SIZE * sizeof(char));

        block_read(inode->singleIndirect, buffer);

        bcopy((unsigned char*) buffer, (unsigned char*) addressBlock, sizeof(AddressBlock));

        for(int i = 11; i < 139; i++)
            addresses[i] = addressBlock->singleIndirect[i - 11];

        free(buffer);
        free(addressBlock);
    }

    /* Carrega mapa de bits dos blocos de dados */
    char dmap[D_MAP_SIZE];
    load_bitmap(dmap, D_MAP_BLOCK);

    /* Variáveis de controle */
    int blockNumber;
    int blockCount = 0;

    /* Aloca blocos de dados para a escrita */
    for(int i = blockStart; i < blockEnd + 1 && i < 138; i++) {
        /* Verifica se não há um bloco de dados alocado ni i-ésimo ponteiro direto */
        if(addresses[i + 1] == -1) {
            /* Busca um bloco de dados livre */
            blockNumber = find_free_bit_number(dmap, D_MAP_SIZE);

            /* Verifica se foi possível encontrar um bloco de dados disponível */
            if(blockNumber == -1)
                break;

            /* Verifica se está sendo alocado um bloco para o single indirect */
            if(i == 10 && addresses[0] == -1) {
                addresses[0] = blockNumber + DATA_BLOCK_START;
                i--;
                blockCount--;
            } else {
                /* Atualiza o i-ésimo ponteiro com o número do bloco de dados livre encontrado */
                addresses[i + 1] = blockNumber + DATA_BLOCK_START;
            }
        }

        /* Incrementa a quantidade de blocos necessários para realizar a escrita */
        blockCount++;
    }

    /* Calcula quantos bytes estão disponíveis para escrita */
    int bytesCount = blockCount * BLOCK_SIZE - byteStart;

    /* Aloca memória para a variável buffer */
    char* buffer = (char*) malloc(blockCount * BLOCK_SIZE * sizeof(char));

    /* Lê os blocos de dados onde será feita a escrita e guarda no buffer */
    read_n_blocks(addresses, buffer, blockStart, blockEnd);

    bzero(&buffer[byteStart], bytesCount);

    /* Escreve os bytes do buffer nos blocos de dados correspondente ao do arquivo */
    write_n_blocks(addresses, buffer, blockStart, blockEnd);

    /* Libera memória alocada dinâmicamente */
    free(buffer);

    int additionalBytes = 0;

    /* Adiciona BLOCK_SIZE bytes se o arquivo passou a ter um bloco indireto */
    if(inode->singleIndirect == -1 && addresses[0] != -1)
        additionalBytes = BLOCK_SIZE;

    /* Atualiza o valor do bloco que contém os ponteiros indiretos */
    inode->singleIndirect = addresses[0];

    /* Atualiza os blocos de dados diretos */
    for(int i = 1; i < 11; i++)
        inode->direct[i - 1] = addresses[i];

    /* Atualiza os blocos de dados indiretos */
    if(inode->singleIndirect != -1) {
        AddressBlock* addressBlock = (AddressBlock*) malloc(sizeof(AddressBlock));
        char* buffer = (char*) malloc(BLOCK_SIZE * sizeof(char));

        for(int i = 11; i < 139; i++)
            addressBlock->singleIndirect[i - 11] = addresses[i];

        bcopy((unsigned char*) addressBlock, (unsigned char*) buffer, sizeof(AddressBlock));

        block_write(inode->singleIndirect, buffer);

        free(buffer);
        free(addressBlock);
    }

    /* Calcula o número esperado de bytes */
    int expected = fd->offset - inode->size;

    /* Verifica se foi escrito o número esperado de bytes */
    if(expected <= bytesCount) {
        inode->size = fd->offset;
    } else {
        inode->size += bytesCount;
    }

    /* Soma bytes adicional (bloco indireto) ao tamanho do arquivo */
    inode->size += additionalBytes;

    /* Salva mapa de bits dos blocos de dados */
    save_bitmap(dmap, D_MAP_BLOCK);
}