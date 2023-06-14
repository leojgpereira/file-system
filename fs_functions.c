File* init_fd_table() {
    File* fd_table = (File*) malloc(FD_TABLE_SIZE * sizeof(File));

    for(int i = 0; i < FD_TABLE_SIZE; i++) {
        fd_table[i].fd = -1;
        fd_table[i].name = NULL;
        fd_table[i].offset = -1;
        fd_table[i].mode = 0;
    }

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

int find_free_bit_number(char* bitmap, int block) {
    /* Aloca memória para variável buffer */
    buffer = (char*) malloc(512 * sizeof(char));

    /* Variável utilizada para guardar número de um bit livre */
    int bitNumber = -1;

    /* Busca o vetor de bits e copia para variável bitmap */
    block_read(block, buffer);
    bcopy((unsigned char*) buffer, (unsigned char*) bitmap, sizeof(bitmap));

    /* Percorre cada byte do vetor de bits */
    for(int i = 0; i < (int) ceil(NUMBER_OF_INODES / 8); i++) {
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

    /* Libera memória alocada para a variável buffer */
    free(buffer);

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

int dir_exists(char* dirname) {
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
        /* Verifica se há um diretório igual a dirname */
        if(same_string(dirname, directoryItems[i].name)) {
            /* Seta variável de controle para verdadeiro */
            exists = 1;
        }
    }

    /* Libera memória alocada dinâmicamente */
    free(inode);
    free(directoryItems);

    /* Retorna se há ou não um diretório igual a dirname no diretório atual */
    return exists;
}