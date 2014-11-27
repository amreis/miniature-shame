#include "apidisk.h"
#include "t2fs.h"
#include "aux.h"
#include <stdio.h> // TODO - Remove
#include <stdlib.h>
#include <string.h>

// TODO guardar setor atual onde se encontra o descritor do diretório?
// TODO nao funciona pra nomes que terminam com /

char cwdPath[1024] = "/";

OPEN_FILE openFiles[20];
OPEN_DIR openDirs[20];
int nOpenFiles = 0;
int nOpenDirs = 0;
bool partitionInfoInitialized = false;
int TEST_INIT_PARTINFO()
{   
    if (!partitionInfoInitialized) {
        if (readSuperblock() != 0) return -1;
        int i;
        for (i = 0; i < 20; ++i)
        {
            openFiles[i] = (OPEN_FILE) { .open = false } ;
            openDirs[i] = (OPEN_DIR) { .open = false } ;
        }

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
    if(TEST_INIT_PARTINFO() != 0) return -1;
    if (nOpenFiles == 20) return -1;
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
    if (TEST_INIT_PARTINFO() != 0) return -1;
    t2fs_record parent = findParentDescriptor(pathname, NULL);
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
        
    writeNthBlock(&descr, 0, (char*)buffer);
    return 0;
}

int rmdir2(char *pathname)
{
    if (TEST_INIT_PARTINFO() != 0) return -1;
    if (strlen(pathname) == 0 ||
        streq2(pathname, ".") ||
        streq2(pathname, "..") ||
        streq2(pathname, "/"))
        return -1;
    t2fs_record parent = findParentDescriptor(pathname, NULL);
    t2fs_record r = findDirDescriptor(pathname);
    // Path not found.
    if (r.TypeVal == TYPEVAL_INVALID) return -1;
    if (r.dataPtr[0] == cwdDescriptor.dataPtr[0])
    {
        // trying to remove where I am D:
        return -1;
    }
    int i;
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    // test if dir is empty
    readNthBlock(&r, 0, (char*)buffer);
    // first and second entries are . and ..
    for (i = 2; i < sizeof(buffer)/sizeof(t2fs_record); ++i)
    {
        const t2fs_record e = buffer[i];
        if (e.TypeVal != TYPEVAL_INVALID)
            break;
    }
    if (i != sizeof(buffer)/sizeof(t2fs_record))
    {
        // not empty
        return -1;
    }
    char *res = strrchr(pathname, '/');
    res = (res == NULL ? pathname : res+1);
    bool found = false;
    int j;
    int countInvalids;
    for (i = 0; i < parent.blocksFileSize && !found; ++i)
    {
        countInvalids = 0;
        readNthBlock(&parent, i, (char*) buffer);
        for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record); ++j)
        {
            if (buffer[j].TypeVal == TYPEVAL_INVALID)
                countInvalids++;
            else if (streq2(buffer[j].name, res))
            {
                buffer[j] = (t2fs_record) { .TypeVal = TYPEVAL_INVALID } ;
                countInvalids++;
                found = true;
            }
        }
        if (countInvalids == sizeof(buffer)/sizeof(t2fs_record))
        {
            // Empty block!
            parent.blocksFileSize--;
            parent.bytesFileSize -= 1024;
            
            invalidateNthBlock(&parent, i);
            updateDescriptorOnDisk(&parent);
        }
        writeNthBlock(&parent, i, (char*)buffer);
    }
    
    freeBlock(r.dataPtr[0]);
    return 0;
}

int close2 (FILE2 handle)
{
    if (TEST_INIT_PARTINFO() != 0) return -1;
    if (handle < 0 || handle >= 20) return -1;
    nOpenFiles--;
    openFiles[handle].open = false;
    return 0;
}

int getcwd2(char *pathname, int size)
{
    if (TEST_INIT_PARTINFO() != 0) return -1;
    return strncpy2(pathname, cwdPath, size);
}

int chdir2(char *pathname)
{
    if (TEST_INIT_PARTINFO() != 0) return -1;
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    
    char saveCurPath[1024];
    strcpy(saveCurPath, cwdPath);
    
    char l_filename[MAX_FILE_NAME_SIZE + 1];
    strcpy(l_filename, pathname);
    t2fs_record dir;
    if (pathname[0] == '/')
    {
        dir = partitionInfo.RootDirReg;
    }
    else
        dir = cwdDescriptor;
    char *token;
    //char lastToken[MAX_FILE_NAME_SIZE + 1];
    //if (strstr(pathname, "/") != NULL)
    {
        token = strtok(l_filename, "/");
        do 
        {
            int i, j;
            bool found = false;
            if (token == NULL)
            {
                strcpy(cwdPath, "/");
                cwdDescriptor = partitionInfo.RootDirReg;
                return 0;
            }
            if (strlen(token) == 0)
                token = strtok(NULL, "/");
            if (token == NULL)
            {
                strcpy(cwdPath, "/");
                cwdDescriptor = partitionInfo.RootDirReg;
                return 0;
            }
            if (streq2(token, ".."))
            {
                *strrchr(cwdPath, '/') = '\0';
                if (cwdPath[0] == '\0') { cwdPath[0] = '/', cwdPath[1] = '\0'; }
            }
            else if (!streq2(token, ".")){
                if (cwdPath[1] != '\0')
                    strcat(cwdPath, "/");
                strcat(cwdPath, token);
            }
            for (i = 0; i < dir.blocksFileSize && !found; ++i)
            {
                if (readNthBlock(&dir, i, (char*) buffer) != 0)  break;
                for (j = 0; i*(partitionInfo.BlockSize) + j*sizeof(t2fs_record) <= dir.bytesFileSize && !found; ++j)
                {
                    if (streq2(token, buffer[j].name))
                    {
                        if (buffer[j].TypeVal != TYPEVAL_DIR) 
                        {
                            strcpy(cwdPath, saveCurPath);
                            return -1;
                        }
                        copyRecord(&dir,&buffer[j]);
                        found = true;
                    }
                }
            }
            if (!found) { strcpy(cwdPath, saveCurPath); return -1; }
            token = strtok(NULL, "/");
        } while (token != NULL);
    }
    copyRecord(&cwdDescriptor, &dir);
    return 0;
}

DIR2 opendir2(char *pathname)
{
    if (TEST_INIT_PARTINFO() != 0) return -1;
    if (nOpenDirs == 20) return -1;
    t2fs_record parent = findDirDescriptor(pathname);
    int i;
    for (i = 0; i < 20; ++i)
    {
        if (!openDirs[i].open)
        {
            openDirs[i] = (OPEN_DIR) { 
                .open = true,
                .descriptor = parent,
                .index = 0
            };
            nOpenDirs++;
            break;
        }
    }
    return MAX_OPEN_FILES + i;
}

int closedir2(DIR2 handle)
{
    if (TEST_INIT_PARTINFO() != 0) return -1;
    if (handle < MAX_OPEN_FILES || handle >= 20+MAX_OPEN_FILES)
    {
        return -1;
    }
    nOpenDirs--;
    openDirs[handle-MAX_OPEN_FILES].open = false;
    return 0;
}

int readdir2(DIR2 handle, DIRENT2 *dentry)
{
    handle -= MAX_OPEN_FILES;
    const int entriesInDir = partitionInfo.BlockSize/sizeof(t2fs_record);
    int block = openDirs[handle].index/entriesInDir;
    t2fs_record buffer[entriesInDir];
    if (readNthBlock(&openDirs[handle].descriptor, block, (char*)buffer) != 0) return -1;
    int indexInBlock = openDirs[handle].index % entriesInDir;
    t2fs_record entry = buffer[indexInBlock];
    while (entry.TypeVal == TYPEVAL_INVALID /*&& indexInBlock < entriesInDir*/ && block < openDirs[handle].descriptor.blocksFileSize)
    {
        indexInBlock++;
        if (indexInBlock == entriesInDir) {
            indexInBlock = 0;
            block++;
            if (readNthBlock(&openDirs[handle].descriptor, block, (char*) buffer) != 0) 
            {
                return -1;
            }
        }
        entry = buffer[indexInBlock];
    }
    if (entry.TypeVal == TYPEVAL_INVALID) return -1;
    strcpy(dentry->name, entry.name);
    dentry->fileType = entry.TypeVal;
    dentry->fileSize = entry.bytesFileSize;
    openDirs[handle].index++;
    return 0;
}

int delete2(char *filename)
{
    char name[MAX_FILE_NAME_SIZE + 1];
    t2fs_record parent = findParentDescriptor(filename, name);
    if (parent.TypeVal == TYPEVAL_INVALID) return -1;
    t2fs_record desc = findFileInDir(&parent, name);
    if (desc.TypeVal != TYPEVAL_FILE) return -1;
    int i;
    for (i = 0; i < desc.blocksFileSize; ++i)
    {
        printf("Releasing block i (%d) of file\n", i);
        if (invalidateNthBlock(&desc, i) != 0) return -1;
    }
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    int j;
    bool found = false;
    for (i = 0; i < parent.blocksFileSize && !found; ++i)
    {
        if (readNthBlock(&parent, i, (char*) buffer) != 0) return -1;
        for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record) && !found; ++j)
        {
            if (streq2(buffer[j].name, name))
            {
                buffer[j] = (t2fs_record) { .TypeVal = TYPEVAL_INVALID } ;
                found = true;
            }
        }
        if (writeNthBlock(&parent, i, (char*) buffer) != 0) return -1;
    }
    updateDescriptorOnDisk(&parent);
    return 0;
}

// Test
int main()
{ 
    TEST_INIT_PARTINFO();
    char buffer[partitionInfo.BlockSize];
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
    
    
    /*
    char str[1024];
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\"hellodir\");\n");
    chdir2("hellodir");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\".\");\n");
    chdir2(".");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\"..\");\n");
    chdir2("..");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\"..\");\n");
    chdir2("..");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\".\");\n");
    chdir2(".");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    * */
    printf("Changing to hellodir...\n");
    chdir2("hellodir");
    printRecordInfo(&cwdDescriptor);
    printf("Opening \"..\" ...\n");
    DIR2 d = opendir2("..");
    DIRENT2 dentry;
    printf("Start reading...\n");
    while (readdir2(d, &dentry) == 0)
    {
        printf("Name: %s, Type: %d, Size (B): %lu\n", dentry.name, dentry.fileType, dentry.fileSize);
    }
    printf("Closing...\n");
    closedir2(d);
    
    chdir2("/");
    create2("./hellodir/test.txt");
    rmdir2("hellodir");
    delete2("../././././hello.txt");
    printf("Opening \"/\" ...\n");
    d = opendir2("/");
    printf("/ handle: %d\n", d);
    printf("Start reading...\n");
    while (readdir2(d, &dentry) == 0)
    {
        printf("Name: %s, Type: %d, Size (B): %lu\n", dentry.name, dentry.fileType, dentry.fileSize);
    }
    closedir2(d);
    return 0;
}
