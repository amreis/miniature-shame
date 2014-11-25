#include "apidisk.h"
#include "t2fs.h"
#include "aux.h"
#include <stdio.h> // TODO - Remove
#include <stdlib.h>
#include <string.h>

// TODO guardar setor atual onde se encontra o descritor do diretório?
// TODO nao funciona pra nomes que terminam com /

OPEN_FILE openFiles[20];
int nOpenFiles = 0;

bool partitionInfoInitialized = false;
int TEST_INIT_PARTINFO()
{   
    if (!partitionInfoInitialized) {
        if (readSuperblock() != 0) return -1;
        partitionInfoInitialized = true;
    }
    return 0;
}

/*
typedef struct {
    unsigned int pointers[sectorsPerBlock*SECTOR_SIZE/sizeof(int)];
} INDEX_BLOCK;
const int nEntriesIndexBlock = SECTOR_SIZE/sizeof(int);
*/
// TODO - OPEN DIRS

int identify2(char *name, int size)
{
	if (TEST_INIT_PARTINFO() != 0) return -1;
	char id[] = "Alister M. - 220494, Eduardo V. - 218313";
	if (size < sizeof(id))
		return -1;
	else strncpy2(name, id, sizeof(id));
	return 0;
}

FILE2 create2(char *filename)
{
    if (nOpenFiles == 20) return -1;
	if(TEST_INIT_PARTINFO() != 0) return -1;
    // find free block
    // alloc free block
    // add entry to directory
    t2fs_record r;
    int newBlock = allocNewBlock();
    if (newBlock == -1) return -1;
    if (filename[0] == '/')
    {
        if (createEntryAbsolute(filename+1, newBlock, TYPEVAL_FILE, &r) != 0)
        {
            freeBlock(newBlock);
            return -1;
        }
    }
    else if (createEntryRelative(filename, newBlock, TYPEVAL_FILE, &r) != 0)
    {
        freeBlock(newBlock);
        return -1;
    }
    
      int i;
    for (i = 0; i < MAX_OPEN_FILES; ++i)
    {
        if (!openFiles[i].open)
        {
            openFiles[i] = (OPEN_FILE) {
                .descriptor = r,
                .offset = 0,
                .dirDescriptor = cwdDescriptor,
                .open = true
            };
            nOpenFiles++;
            break;
        }
    }
    return i;
}

FILE2 open2(char *filename)
{
    if(TEST_INIT_PARTINFO() != 0) return -1;
    if (nOpenFiles == 20) return -1;
    int i;
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    for (i = 0; i < MAX_OPEN_FILES; ++i)
    {
        if (!openFiles[i].open)
        {
            char nameWithoutPath[MAX_FILE_NAME_SIZE+1];
            t2fs_record pDescriptor = findParentDescriptor(filename, nameWithoutPath);
            if (pDescriptor.TypeVal != TYPEVAL_DIR) return -1;
            t2fs_record descriptor = findFileInDir(&pDescriptor, nameWithoutPath);
            if (descriptor.TypeVal != TYPEVAL_FILE) return -1;
            openFiles[i] = (OPEN_FILE) {
                .descriptor = descriptor,
                .offset = 0,
                .open = true,
                .dirDescriptor = buffer[1]
            };
            
            nOpenFiles++;
            break;
        }
    }
    return i;
}

int mkdir2(char *pathname)
{
    printf("Pathname: %s", pathname);
    t2fs_record parent = findParentDescriptor(pathname, NULL);
    printf("Parent dir: %s\n", parent.name);
    int newBlock = allocNewBlock();
    t2fs_record descr;
    if (createEntryDir(&parent, strrchr(pathname, '/')+1, newBlock, TYPEVAL_DIR, &descr) != 0)
    {
        freeBlock(newBlock);
        return -1;
    }
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    readNthBlock(&descr, 0, (char*)buffer);
    descr.bytesFileSize = partitionInfo.BlockSize;
    copyRecord(&buffer[0], &descr);
    strcpy(buffer[0].name, ".");
    copyRecord(&buffer[1], &parent);
    strcpy(buffer[1].name, "..");
    int j;
    for (j = 2; j < sizeof(buffer)/sizeof(t2fs_record); ++j)
    {
        buffer[j] = (t2fs_record) { .TypeVal = TYPEVAL_INVALID };
    }
    
    //updateDescriptorOnDisk(&descr);
    
    writeNthBlock(&descr, 0, (char*)buffer);
    return 0;
}

int close2 (FILE2 handle)
{
    if (TEST_INIT_PARTINFO() != 0) return -1;
    if (handle < 0 || handle > 20) return -1;
    nOpenFiles--;
    openFiles[handle].open = false;
    return 0;
}


// Test
int main()
{ 
    char buffer[1024];
    char filename[] = "hello2.txt";
    //if (TEST_INIT_PARTINFO() != 0) return -1;
    SUPERBLOCK s;
    read_sector(0, (char*) &s); 
    printSuperblockInfo(&s);
	printf("Creating...\n");
    int i;
    /*
    for (i = '0'; i-'0' <= 64; ++i)
    {
        filename[5] = i;
        FILE2 handle;
        if ((handle = create2(filename)) == -1) printf("Didnt create file %s.\n", filename);
        close2(handle);
    }*/
    create2("/hello.txt");
    printf("Testing double dot.\n");
    create2("../helloDD.txt");
    create2("/../../helloDD2.txt");
    create2("./helloD.txt");
    /*
    int j;
    for(j = 0; j < 5; ++j)
    {
        if (readNthBlock(&cwdDescriptor, j, buffer) == -1) break;
        for (i = 0; i < 16; ++i)
        {
            printf("----------- RECORD %d ------------\n", i);
            printRecordInfo((t2fs_record*) buffer + i);
        }
    }*/
    
    read_sector(0, (char*) &s); 
    printSuperblockInfo(&s);
    
    if (open2("hello.txt") < 0)
    {
        printf("Didn't find hello.txt\n");
    }
    if (open2("../../hello.txt") < 0)
    {
        printf("Didn't find ../../hello.txt\n");
    }
    if (open2(".././.././hello.txt") < 0)
        printf("Didn't find .././.././hello.txt\n");
    mkdir2("/hellodir");
    
    int j;
    t2fs_record createdDir;
    for (j = 0; j < 5; ++j)
    {
        if (readNthBlock(&cwdDescriptor, j, buffer) == -1) break;
        for (i = 0; i < 16; ++i)
        {
            printf("------------- RECORD %d --------------\n", i);
            printRecordInfo((t2fs_record*) buffer + i);
            if ( ((t2fs_record*)buffer+i)->TypeVal == TYPEVAL_DIR && i > 1)
                createdDir = *((t2fs_record*)buffer+i);
        }
    }
    for (j = 0; j < 5; ++j)
    {
        if (readNthBlock(&createdDir, j, buffer) == -1) break;
        for (i = 0; i < 16; ++i)
        {
            printf("------------- RECORD %d --------------\n", i);
            printRecordInfo((t2fs_record*) buffer + i);
        }
    }
    
    return 0;
}
