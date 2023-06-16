#ifndef FS_INCLUDED
#define FS_INCLUDED

#define FS_SIZE 2048

#define MAGIC_NUMBER "!CFS"
#define NUMBER_OF_INODES 512
#define NUMBER_OF_DATA_BLOCKS 2001
#define INODE_START 3
#define INODE_END 46
#define DATA_BLOCK_START 47
#define I_MAP_BLOCK 1
#define D_MAP_BLOCK 2
#define ROOT_DIRECTORY_INODE 0
#define FD_TABLE_SIZE 5

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
int fs_unlink( char *fileName, ...);
int fs_stat( char *fileName, fileStat *buf);

#define MAX_FILE_NAME 32
#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif

typedef struct __attribute__((packed)) {
    char magicNumber[5];
    int diskSize;
    int workingDirectory;
    int numberOfInodes;
    int numberOfDataBlocks;
    int iMapStart;
    int dMapStart;
    int inodeStart;
    int dataBlockStart;
} Superblock;

typedef struct __attribute__((packed)) {
    int type;
    int size;
    int linkCount;
    int direct[8];
} Inode;

typedef struct __attribute__((packed)) {
    char name[MAX_FILE_NAME];
    int inode;
} DirectoryItem;

typedef struct __attribute__((packed)) {
    char name[MAX_FILE_NAME];
    int inode;
    int directoryInode;
    int offset;
    int mode;
    int wasTouched;
} File;

int fs_ls();