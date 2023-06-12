#ifndef FS_INCLUDED
#define FS_INCLUDED

#define FS_SIZE 2048

/* Define o número mágico do nosso sistma de arquivos */
#define MAGIC_NUMBER "!CFS"
#define NUMBER_OF_INODES 320
#define NUMBER_OF_DATA_BLOCKS 2044
#define INODE_START 1
#define DATA_BLOCK_START 4

/* Struct que representa o superbloco (bloco nº 0 do volume) */
typedef struct __attribute__((packed)) {
    char magicNumber[5];
    int diskSize; /* Tamanho do disco (em número de blocos) */
    int numberOfInodes;
    int numberOfDataBlocks;
    int inodeStart;
    int dataBlockStart;
} Superblock;

void fs_init( void);
int fs_mkfs( void);
int fs_open( char *fileName, int flags);
int fs_close( int fd);
int fs_read( int fd, char *buf, int count);
int fs_write( int fd, char *buf, int count);
int fs_lseek( int fd, int offset);
int fs_mkdir( char *fileName);
int fs_rmdir( char *fileName);
int fs_cd( char *dirName);
int fs_link( char *old_fileName, char *new_fileName);
int fs_unlink( char *fileName);
int fs_stat( char *fileName, fileStat *buf);

#define MAX_FILE_NAME 32
#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif
