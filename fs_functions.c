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
    /* Aloca memória para variável buffer */
    buffer = (char*) malloc(512 * sizeof(char));

    /* Busca o vetor de bits e copia para variável bitmap */
    block_read(block, buffer);
    bcopy((unsigned char*) buffer, (unsigned char*) bitmap, sizeof(bitmap));

    /* Libera memória alocada para a variável buffer */
    free(buffer);
}

int find_free_bit_number(char* bitmap) {
    /* Variável utilizada para guardar número de um bit livre */
    int bitNumber = -1;

    /* Percorre cada byte do vetor de bits */
    for(int i = 0; i < (int) ceil(NUMBER_OF_INODES / 8.0); i++) {
        /* Percorre cada bit do byte do vetor de bits */
        for(int j = 7; j > -1; j--) {
            /* Descobre o valor de um bit dentro de um byte */
            int bit = get_bit(bitmap, i, j);
            printf("bit = %d\n", bit);

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
    /* Aloca memória para variável buffer */
    buffer = (char*) malloc(512 * sizeof(char));
    bzero(buffer, 512);

    /* Copia vetor de bits para a variável buffer */
    bcopy((unsigned char*) bitmap, (unsigned char*) buffer, sizeof(bitmap));
    block_write(block, buffer);

    /* Libera memória alocada para a variável buffer */
    free(buffer);
}

Inode* find_inode(int inodeNumber) {
    if(inodeNumber < 0 || inodeNumber > NUMBER_OF_INODES - 1)
        return NULL;

    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc(1024 * sizeof(char));

    /* Zera todos os bytes da variável buffer e copia o inode correspondente para o buffer */
    bzero(buffer, 1024);

    /* Calcula em quais blocos estão o inode a ser encontrado */
    int start = (inodeNumber * 44) / 512;
    int end = (start + 43) / 512;

    /* Copia os blocos para a variável buffer */
    for(int i = start; i < end + 1; i++) {
        block_read(i + INODE_START, buffer);
    }

    /* Copia os bytes do buffer para uma estrutura inode */
    Inode* inode = (Inode*) malloc(sizeof(Inode));
    bcopy((unsigned char*) &buffer[(inodeNumber * 44) % 512], (unsigned char*) inode, sizeof(Inode));

    /* Libera memória alocada */
    free(buffer);

    /* Retorna a referência para o inode encontrado */
    return inode;
}

void save_inode(Inode* inode, int inodeNumber) {
    /* Aloca memória para a variável buffer */
    char* buffer_ = (char*) malloc(1024 * sizeof(char));

    /* Zera todos os bytes da variável buffer e copia o inode correspondente para o buffer */
    bzero(buffer_, 1024);

    /* Calcula em quais blocos estão o inode a ser salvo */
    int start = (inodeNumber * 44) / 512;
    int end = (start + 43) / 512;

    /* Copia os blocos para a variável buffer */
    for(int i = start; i < end + 1; i++) {
        block_read(i + INODE_START, buffer_);
    }

    /* Copia os bytes correspondentes ao inode na variável buffer na posição correspondente a posição do inode */
    bcopy((unsigned char*) inode, (unsigned char*) &buffer_[(inodeNumber * 44) % 512], sizeof(Inode));

    /* Copia os blocos da variável buffer para o disco */
    for(int i = start; i < end + 1; i++) {
        block_write(i + INODE_START, buffer_);
    }

    /* Libera memória alocada dinâmicamente */
    free(buffer_);
}

DirectoryItem* get_directory_items(int inodeNumber) {
    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(inodeNumber);

    /* Verifica se o inode corresponde a um inode de diretório */
    if(inode->type != 1)
        return NULL;

    /* Calcula quantos blocos de dados o diretório utiliza */
    int numBlocks = (inode->size / 512) + 1;

    /* Cria uma variável buffer para armazenar os blocos de dados */
    buffer = (char*) malloc(numBlocks * 512 * sizeof(char));

    /* Lê cada bloco de dados correspondente ao diretório a ser consultado */
    for(int i = 0; i < numBlocks; i++) {
        block_read(inode->direct[i], &buffer[i * 512]);
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

DirectoryItem* get_directory_item(char* itemName) {
    /* Recupera o inode do diretório atual */
    Inode* inode = find_inode(superblock->workingDirectory);
    /* Recupera a lista de diretórios dentro do diretório atual */
    DirectoryItem* directoryItems = get_directory_items(superblock->workingDirectory);

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
    char imap[64];
    load_bitmap(imap, I_MAP_BLOCK);

    /* Encontra inode livre */
    int inodeNumber = find_free_bit_number(imap);

    /* Checa se houve sucesso em encontrar um inode livre */
    if(inodeNumber == -1)
        return NULL;

    printf("inode number = %d is free\n", inodeNumber);

    /* Salva mapa de bits atualizado no disco */
    save_bitmap(imap, I_MAP_BLOCK);

    /* Aloca inode para o novo arquivo criado */
    Inode* newInode = (Inode*) malloc(sizeof(Inode));
    newInode->type = 0;
    newInode->size = 0;
    for(int i = 0; i < (sizeof(newInode->direct) / 4); i++) {
        newInode->direct[i] = -1;
    }

    /* Salva o inode correspondente ao arquivo no disco */
    save_inode(newInode, inodeNumber);

    /* Recupera inode do diretório de trabalho atual */
    Inode* inode = find_inode(superblock->workingDirectory);

    /* Calcula o bloco de inicio e termino correspondente ao diretório atual */
    int blockStart = inode->size / 512;
    int blockEnd = (inode->size + sizeof(DirectoryItem)) / 512;
    int byteStart = inode->size % 512;

    buffer = (char*) malloc((blockEnd - blockStart + 1) * 512 * sizeof(char));

    /* Lê os blocos onde o diretório atual está salvo */
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    newDirectoryItem = (DirectoryItem*) malloc(sizeof(DirectoryItem));
    bcopy((unsigned char*) fileName, (unsigned char*) newDirectoryItem->name, strlen(fileName) + 1);
    newDirectoryItem->inode = inodeNumber;

    /* Guarda o novo arquivo criado dentro do bloco de dados do diretório atual */
    bcopy((unsigned char*) newDirectoryItem, (unsigned char*) &buffer[byteStart], sizeof(DirectoryItem));
    for(int i = blockStart; i < blockEnd + 1; i++) {
        block_write(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    inode->size += sizeof(DirectoryItem);
    save_inode(inode, superblock->workingDirectory);

    /* Libera memória alocada dinamicamente */
    printf("Freeing...\n");
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
        /* Verifica de a posição tem um descritor válido e se esse tem o mesmo nome de arquivo que fileName */
        if(fdTable[i] != NULL && same_string(fileName, fdTable[i]->name)) {
            /* Atualiza a variável de controle para guardar a posição do vetor que já contém um descritor de arquivo aberto para o arquivo fileName */
            index = i;

            /* Encerra o loop */
            break;
        }
    }

    /* Retorna o índice do descritor ou -1 caso ainda não haja um descritor aberto para o arquivo com nome igual a fileName */
    return index;
}

void fill_with_zero_bytes(Inode* inode, File* fd) {
    /* Calcula os blocos de inicio e término da escrita e byte de início */
    int blockStart = inode->size / 512;
    int blockEnd = fd->offset / 512;
    int byteStart = inode->size % 512;

    printf("%d %d %d\n", blockStart, blockEnd, byteStart);

    /* Carrega mapa de bits dos blocos de dados */
    char dmap[251];
    load_bitmap(dmap, D_MAP_BLOCK);

    /* Variáveis de controle */
    int blockNumber;
    int blockCount = 0;

    /* Aloca blocos de dados para a escrita */
    for(int i = blockStart; i < blockEnd + 1 && i < (sizeof(inode->direct) / 4); i++) {
        printf(">>>%d\n", inode->direct[i]);
        /* Verifica se não há um bloco de dados alocado ni i-ésimo ponteiro direto */
        if(inode->direct[i] == -1) {
            /* Busca um bloco de dados livre */
            blockNumber = find_free_bit_number(dmap);

            /* Verifica se foi possível encontrar um bloco de dados disponível */
            if(blockNumber == -1)
                break;

            /* Atualiza o i-ésimo ponteiro direto com o número do bloco de dados livre encontrado */
            inode->direct[i] = blockNumber + DATA_BLOCK_START;

            printf("block number = %d is free\n", blockNumber + DATA_BLOCK_START);
        }

        /* Incrementa a quantidade de blocos necessários para realizar a escrita */
        blockCount++;
    }

    /* Calcula quantos bytes estão disponíveis para escrita */
    int bytesCount = blockCount * 512 - byteStart;

    /* Aloca memória para a variável buffer */
    char* buffer = (char*) malloc(blockCount * 512 * sizeof(char));

    /* Lê os blocos de dados onde será feita a escrita e guarda no buffer */
    for(int i = blockStart; i < blockStart + blockCount; i++) {
        block_read(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    bzero(&buffer[byteStart], bytesCount);

    /* Escreve os bytes do buffer nos blocos de dados correspondente ao do arquivo */
    for(int i = blockStart; i < blockStart + blockCount; i++) {
        printf("Writing to block number %d\n", inode->direct[i]);
        block_write(inode->direct[i], &buffer[(i - blockStart) * 512]);
    }

    int count = fd->offset - inode->size;

    if(count <= bytesCount) {
        inode->size = fd->offset;
    } else {
        inode->size = bytesCount + byteStart;
    }

    printf("new file size filled with zeros: %d\n", inode->size);

    save_bitmap(dmap, D_MAP_BLOCK);
    
    free(buffer);
}