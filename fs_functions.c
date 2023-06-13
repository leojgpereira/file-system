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
    bytes[index] = ((1 << n) | bytes[index]);
}

int get_bit(char* bytes, int index, int n) {
    return (bytes[index] >> n) & 1;
}

Inode* find_inode(int inodeNumber) {
    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc(1024 * sizeof(char));

    /* Zera todos os bytes da variável buffer e copia o inode correspondente para o buffer */
    bzero(buffer, 1024);

    int start = (inodeNumber * 44) / 512;
    int end = (start + 43) / 512;

    for(int i = start; i < end + 1; i++) {
        block_read(i + INODE_START, buffer);
    }

    /* Copia os bytes do buffer para uma estrutura inode */
    Inode* inode = (Inode*) malloc(sizeof(Inode));
    bcopy((unsigned char*) &buffer[(inodeNumber * 44) % 512], (unsigned char*) inode, sizeof(Inode));

    free(buffer);

    return inode;
}

void save_inode(Inode* inode, int inodeNumber) {
    /* Aloca memória para a variável buffer */
    buffer = (char*) malloc(1024 * sizeof(char));

    /* Zera todos os bytes da variável buffer e copia o inode correspondente para o buffer */
    bzero(buffer, 1024);

    int start = (inodeNumber * 44) / 512;
    int end = (start + 43) / 512;

    for(int i = start; i < end + 1; i++) {
        block_read(i + INODE_START, buffer);
    }

    bcopy((unsigned char*) inode, (unsigned char*) &buffer[(inodeNumber * 44) % 512], sizeof(Inode));

    for(int i = start; i < end + 1; i++) {
        block_write(i + INODE_START, buffer);
    }

    free(buffer);
}