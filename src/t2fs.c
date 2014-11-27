#include "apidisk.h"
#include "t2fs.h"
#include "aux.h"
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
    char absPath[1024];
    int newBlock = allocNewBlock();
    if (newBlock == -1) return -1;
    if (filename[0] == '/')
    {
        strcpy(absPath, filename);
        if (createEntryAbsolute(filename, newBlock, TYPEVAL_FILE, &r) != 0)
        {
            freeBlock(newBlock);
            return -1;
        }
    }
    else if (createEntryRelative(filename, newBlock, TYPEVAL_FILE, &r, absPath, cwdPath) != 0)
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
            strcpy(openFiles[i].absolutePath, absPath);
            nOpenFiles++;
            break;
        }
    }
    return i;
}

int tracebackToRoot(const t2fs_record* folder, char *outPath)
{
    char curPath[1024] = { '\0' };
    t2fs_record parent = *folder;
    t2fs_record gramps;
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    t2fs_record buffer2[partitionInfo.BlockSize/sizeof(t2fs_record)];
    
    if (streq2(folder->name, "/"))
    {
        strcpy(outPath, "/");
    }
    else
    {
        bool found = false;
       
        int i, j;
        while (parent.dataPtr[0] != 0)
        {
            readNthBlock(&parent, 0, (char*) buffer);
            gramps = buffer2[1];
            for (i = 0; i < gramps.blocksFileSize && !found; ++i)
            {
                readNthBlock(&gramps, i, (char*) buffer2);
                for (j = 0; j < sizeof(buffer2)/sizeof(t2fs_record) && !found; ++j)
                {
                    if (buffer2[j].dataPtr[0] == parent.dataPtr[0])
                    {
                        // Ok, now we have its name!
                        if (streq2(buffer2[j].name, "/") || streq2(buffer2[j].name, ".") || streq2(buffer[j].name, ".."))
                        {
                            // Got to root!
                            strcpy(outPath, "/");
                            strcat(outPath, curPath);
                            return 0;
                        }
                        else {
                            strcpy(outPath, curPath);
                            strcpy(curPath, buffer[j].name);
                            strcat(curPath, "/");
                            strcat(curPath, outPath);
                            found = true;
                        }
                    }
                }
            }
            if (!found) return 0;
            found = false;
            parent = gramps;
        }
    }
    return 0;
}

FILE2 open2(char *filename)
{
    if(TEST_INIT_PARTINFO() != 0) return -1;
    if (nOpenFiles == 20) return -1;
    int i;
    //t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
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
                //.dirDescriptor = buffer[1]
            };
            char absPath[1024];
            tracebackToRoot(&pDescriptor, absPath);
            strcpy(openFiles[i].absolutePath, absPath);
            strcat(openFiles[i].absolutePath, nameWithoutPath);
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
    updateDescriptorOnDisk(&parent);
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

int read2 (FILE2 handle, char *buffer, int size)
{
    const int sizeRequested = size;
    if (handle < 0 || handle >= 20) return -1;
    if (!openFiles[handle].open) return -1;
    int block = openFiles[handle].offset / partitionInfo.BlockSize;
    if (openFiles[handle].offset == openFiles[handle].descriptor.bytesFileSize)
        return 0;
    t2fs_record p = findParentDescriptor(openFiles[handle].absolutePath, NULL);
    openFiles[handle].descriptor = findFileInDir(&p, strrchr(openFiles[handle].absolutePath, '/') + 1);
    char b[partitionInfo.BlockSize];
    int i = 0;
    while (size > 0 && block < openFiles[handle].descriptor.blocksFileSize)
    {
        readNthBlock(&openFiles[handle].descriptor, block, b);
        for (; size > 0 && openFiles[handle].offset < min((block+1)*partitionInfo.BlockSize, openFiles[handle].descriptor.bytesFileSize); ++openFiles[handle].offset, ++i, size--)
        {
            
            buffer[i] = b[openFiles[handle].offset%partitionInfo.BlockSize];
        }
        block++;
    }
    return sizeRequested - size;
}

int write2(FILE2 handle, char *buffer, int size)
{
    const int sizeRequested = size;
    
    if (handle < 0 || handle >= 20) return -1;
    if (!openFiles[handle].open) return -1;
    int block = openFiles[handle].offset / partitionInfo.BlockSize;
    
    char b[partitionInfo.BlockSize];
    int i = 0;
    bool first = true;
    openFiles[handle].descriptor.bytesFileSize = openFiles[handle].offset;
    while (size > 0 && block < openFiles[handle].descriptor.blocksFileSize)
    {
        readNthBlock(&openFiles[handle].descriptor, block, b);
        int bytesToCopy = min(partitionInfo.BlockSize - (
                    first ? (openFiles[handle].offset % partitionInfo.BlockSize) : 0),
            size);
        
        
        memcpy(b + (first ? (openFiles[handle].offset % partitionInfo.BlockSize) : 0), buffer + i, 
            bytesToCopy);
        if (first)
            first = false;
        size -= bytesToCopy;
        i += bytesToCopy;
        openFiles[handle].offset += bytesToCopy;
        openFiles[handle].descriptor.bytesFileSize += bytesToCopy;
        /*
        for (; size > 0 && openFiles[handle].offset < partitionInfo.BlockSize; --size, ++openFiles[handle].offset)
        {
            b[openFiles[handle].offset % partitionInfo.BlockSize] = buffer[i];
        }*/
        writeNthBlock(&openFiles[handle].descriptor, block, b);
        block++;
    }
    t2fs_record* desc = &openFiles[handle].descriptor;
    while (size > 0)
    {
        int newBlock = allocNewBlock();
        if (newBlock == -1) return -1;
        // AW GAWD, MUST ALLOC NEW BLOCKS
        if (desc->blocksFileSize < 4)
        {
            desc->dataPtr[desc->blocksFileSize] = newBlock;
        }
        else if (desc->blocksFileSize == 4)
        {
            // allocate first index block too
            int indirectBlock = allocNewBlock();
            if (indirectBlock == -1) return -1;
            desc->singleIndPtr = indirectBlock;
            int j;
            int buffer2[entriesInIndexBlock];
            buffer2[0] = newBlock;
            for (j = 1; j < sizeof(buffer2)/sizeof(int); ++j)
            {
                buffer2[j] = UNUSED_POINTER;
            }
            write_block(indirectBlock, (char*)buffer2);
        }
        else if (desc->blocksFileSize < 4 + entriesInIndexBlock)
        {
            int buffer2[entriesInIndexBlock];
            read_block(desc->singleIndPtr, (char*) buffer2);
            int j;
            for (j = 0; j < sizeof(buffer2)/sizeof(int); ++j)
            {
                if (buffer2[j] == UNUSED_POINTER)
                {
                    buffer2[j] = newBlock;
                    break;
                }
            }
            write_block(desc->singleIndPtr, (char*) buffer2);
        }
        else if (desc->blocksFileSize == 4 + entriesInIndexBlock)
        {
            // Need to use double indirection
            int doubleIndirectBlock = allocNewBlock();
            if (doubleIndirectBlock == -1) return -1;
            desc->doubleIndPtr = doubleIndirectBlock;
            int j;
            int buffer2[partitionInfo.BlockSize/sizeof(int)];
            for (j = 0; j < sizeof(buffer2)/sizeof(int); ++j)
                buffer2[j] = UNUSED_POINTER;
            int simpleIndirectBlock = allocNewBlock();
            if (simpleIndirectBlock == -1) return -1;
            buffer2[0] = simpleIndirectBlock;
            write_block(doubleIndirectBlock, (char*)buffer2);
            buffer2[0] = newBlock;
            write_block(simpleIndirectBlock, (char*)buffer2);
        }
        else if (desc->blocksFileSize < maxNofBlocks && (desc->blocksFileSize - entriesInIndexBlock - 4)%entriesInIndexBlock == 0)
        {
            int buffer2[partitionInfo.BlockSize/sizeof(int)];
            int buffer3[partitionInfo.BlockSize/sizeof(int)];
            read_block(desc->doubleIndPtr, (char*) buffer2);
            int j, k;
            for (j = 0; j < entriesInIndexBlock; ++j)
            {
                if (buffer2[j] == UNUSED_POINTER)
                {
                    int singleIndBlock = allocNewBlock();
                    if (singleIndBlock == -1) return -1;
                    buffer2[j] = singleIndBlock;
                    buffer3[k] = newBlock;
                    for (k = 1; k < entriesInIndexBlock; ++k)
                        buffer3[k] = UNUSED_POINTER;
                    write_block(singleIndBlock, (char*)buffer3);
                    break;
                }
            }
            write_block(desc->doubleIndPtr, (char*) buffer2);
        }
        else if (desc->blocksFileSize < maxNofBlocks)
        {
            int dBlockToRead = -1;
            int j;
            int buffer2[partitionInfo.BlockSize/sizeof(int)];
            read_block(desc->doubleIndPtr, (char*) buffer2);
            for (j = 0; j < sizeof(buffer2)/sizeof(int); ++j)
            {
                if (buffer2[j] == UNUSED_POINTER)
                {
                    break;
                }
                dBlockToRead = buffer2[j];
            }
            read_block(dBlockToRead, (char*) buffer2);
            for (j = 0; j < sizeof(buffer2)/sizeof(int); ++j)
            {
                if (buffer2[j] == UNUSED_POINTER)
                {
                    buffer2[j] = newBlock;
                    break;
                }
            }
            write_block(dBlockToRead, (char*) buffer2);
        }
        else { return sizeRequested - size; }
        int bytesToCopy = min(partitionInfo.BlockSize, size);
        memcpy(b, buffer + i, bytesToCopy);
        size -= bytesToCopy;
        i += bytesToCopy;
        openFiles[handle].offset += bytesToCopy;
        openFiles[handle].descriptor.bytesFileSize += bytesToCopy;
        
        write_block(newBlock, b);
        
        desc->blocksFileSize++;
        desc->bytesFileSize += min(size, partitionInfo.BlockSize);
        
        
        
    }
    t2fs_record bufferr[partitionInfo.BlockSize/sizeof(t2fs_record)];
        
    
    char fname[MAX_FILE_NAME_SIZE+1];
    t2fs_record parent = findParentDescriptor(openFiles[handle].absolutePath, fname);

    bool found = false;
    for (i = 0; i < parent.blocksFileSize && !found; ++i)
        {
            int j;
            readNthBlock(&parent, i, (char*) bufferr);
            for (j = 0; j < sizeof(bufferr)/sizeof(t2fs_record) && !found; ++j)
            {
                if (bufferr[j].dataPtr[0] == desc->dataPtr[0])
                {
                    bufferr[j] = *desc;
                    found = true;
                }
            }
            writeNthBlock(&parent, i, (char*) bufferr);
        }
    return sizeRequested - size;
    
}

int seek2(FILE2 handle, unsigned int offset)
{
    if (handle < 0 || handle >= 20) return -1;
    if (!openFiles[handle].open) return -1;
    if (offset == -1)
    {
        openFiles[handle].offset = openFiles[handle].descriptor.bytesFileSize;
    }
    else if (offset >= 0 && offset <= openFiles[handle].descriptor.bytesFileSize)
    {
        openFiles[handle].offset = offset;
    }
    else 
    {
        return -1;
    }
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
        strcpy(cwdPath, "/");
    }
    else
        dir = cwdDescriptor;
    char *token;
    //char lastToken[MAX_FILE_NAME_SIZE + 1];
    //if (strstr(pathname, "/") != NULL)
    {
        token = strtok(l_filename, "/");
        if (token == NULL)
            {
                strcpy(cwdPath, "/");
                cwdDescriptor = partitionInfo.RootDirReg;
                return 0;
            }
        do 
        {
            int i, j;
            bool found = false;
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
            //else { token = strtok(NULL, "/"); }
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

