

#ifndef __LIBT2FS___
#define __LIBT2FS___

#define STR_END '\0'
#define UNUSED_POINTER 0xFFFFFFFF
#define BLOCK_TO_SECTOR(b) (b == -1 ? -1 : (b*sectorsPerBlock + 1))
#define MAX_OPEN_FILES 20

#define true 1
#define false 0
#define bool char

#define TYPEVAL_FILE 0x01
#define TYPEVAL_DIR 0x02
#define TYPEVAL_INVALID 0xFF

typedef int FILE2;
typedef int DIR2;

#define MAX_FILE_NAME_SIZE 255
typedef struct {
    char name[MAX_FILE_NAME_SIZE+1];
    int fileType;   // ==1, is directory; ==0 is file
    unsigned long fileSize;
} DIRENT2;

typedef struct {
    unsigned char TypeVal; // 0xFF invalid, 0x01 file, 0x02 directory
    char name[31];
    int blocksFileSize;
    int bytesFileSize;
    int dataPtr[4];
    int singleIndPtr;
    int doubleIndPtr;
} t2fs_record;

int maxNofBlocks;

typedef struct {
    int Id;
    char Version_Lo;
    char Version_Hi;
    char SuperBlockSize_Lo;
    char SuperBlockSize_Hi;
    int DiskSize;
    int NofBlocks;
    int BlockSize;
    char Reserved[108];
    t2fs_record BitMapReg;
    t2fs_record RootDirReg;
} SUPERBLOCK;

typedef struct {
    t2fs_record descriptor;
    t2fs_record dirDescriptor;
    int offset;
    bool open;
    char absolutePath[1024];
} OPEN_FILE;

typedef struct {
    t2fs_record descriptor;
    int index;
    bool open;
} OPEN_DIR;

int identify2 (char *name, int size);

FILE2 create2 (char *filename);
int delete2 (char *filename);
FILE2 open2 (char *filename);
int close2 (FILE2 handle);
int read2 (FILE2 handle, char *buffer, int size);
int write2 (FILE2 handle, char *buffer, int size);
int seek2 (FILE2 handle, unsigned int offset);

int mkdir2 (char *pathname);
int rmdir2 (char *pathname);

DIR2 opendir2 (char *pathname);
int readdir2 (DIR2 handle, DIRENT2 *dentry);
int closedir2 (DIR2 handle);

int chdir2 (char *pathname);
int getcwd2 (char *pathname, int size);

#endif
