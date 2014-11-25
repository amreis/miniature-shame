#include "aux.h"
#include <stdio.h>
#include <string.h>

void copyRecord(t2fs_record *dst, const t2fs_record *org)
{
    memcpy(dst->dataPtr, org->dataPtr, 4*sizeof(int));
    dst->singleIndPtr = org->singleIndPtr;
    dst->doubleIndPtr = org->doubleIndPtr;
    strcpy(dst->name, org->name);
    dst->bytesFileSize = org->bytesFileSize;
    dst->blocksFileSize = org->blocksFileSize;
    dst->TypeVal = org->TypeVal;
}

int strncpy2(char *dst, const char *org, int n)
{
	int i = 0;
    while (org[i] != STR_END && i < n - 1)
    {
        dst[i] = org[i];
        i++;

    }
    dst[i] = STR_END;
	return 0;
}

bool streq2(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

void printRecordInfo(t2fs_record *r)
{
    
	printf("Type: ");
	switch (r->TypeVal)
	{
		case 1:
			printf("file\n");
			break;
		case 2:
			printf("directory\n");
			break;
		case 0xFF:
			printf("invalid\n");
			break;
		default:
			printf("Odd\n");
	}
	printf("Name: %s\n", r->name);
	printf("File size in blocks: %d\n", r->blocksFileSize);
	printf("File size in bytes: %d\n", r->bytesFileSize);
	printf("First pointers...\n");
	printf("%d\n%d\n%d\n%d\n",r->dataPtr[0], r->dataPtr[1], r->dataPtr[2], r->dataPtr[3]);
}

void printSuperblockInfo(SUPERBLOCK *s)
{
 	printf("Id: %0x\n", s->Id);
	printf("Version: %0x %0x\n", s->Version_Lo, s->Version_Hi);
	printf("Superblock size: %0x %0x (%d) \n", s->SuperBlockSize_Lo, s->SuperBlockSize_Hi, (((int)s->SuperBlockSize_Hi) << 8) + s->SuperBlockSize_Lo);
	printf("Disk size: %d\n", s->DiskSize);
	printf("N of blocks: %d\n", s->NofBlocks);
	printf("Block Size: %d\n", s->BlockSize);
	printf("Bitmap info:\n");
	printRecordInfo(&s->BitMapReg);
	printf("Rootdir info:\n");
	printRecordInfo(&s->RootDirReg);
}


int readSuperblock()
{
	if (read_sector(0, (char*) & partitionInfo) != 0) return -1;
	cwdDescriptorSector = 0; // cwd is "/"
    cwdDescriptor = partitionInfo.RootDirReg;
    bitmapDescriptor = partitionInfo.BitMapReg;
	sectorsPerBlock = partitionInfo.BlockSize/SECTOR_SIZE;
    printf("Sectors per block: %d\n", sectorsPerBlock);
    entriesInIndexBlock = partitionInfo.BlockSize/sizeof(int);
    maxNofBlocks = 4 + entriesInIndexBlock + entriesInIndexBlock*entriesInIndexBlock;
	return 0;
}

int read_block(int block, char *out)
{
    int startSector = BLOCK_TO_SECTOR(block);
    int i;
    for (i = 0; i < sectorsPerBlock; ++i)
    {
        if (read_sector(startSector + i, out + SECTOR_SIZE*i) != 0) return -1;
    }
    return 0;
}

int write_block(int block, char *data)
{
    int startSector = BLOCK_TO_SECTOR(block);
    int i;
    for (i = 0; i < sectorsPerBlock; ++i)
    {
        if (write_sector(startSector + i, data + SECTOR_SIZE*i) != 0) return -1;
    }
    return 0;
}

// Gets a free block, marks it on the bitmap, saves the bitmap to disk
// and returns the number of the block.
// INCOMPLETE
int allocNewBlock()
{
    char block[sectorsPerBlock*SECTOR_SIZE];
    int b;
    if (bitmapDescriptor.blocksFileSize <= 4)
    {
        for (b = 0; b < bitmapDescriptor.blocksFileSize; ++b)
        {
            read_block(bitmapDescriptor.dataPtr[b], block);
            int i, j;
            for (i = 0; sectorsPerBlock*SECTOR_SIZE*b + i < bitmapDescriptor.bytesFileSize; ++i)
            {
                char byte = block[i];
                for (j = 0; j < 8; ++j)
                {
                    if ((byte & (1 << j)) == 0)
                    {
                        // FREE BLOCK!
                        block[i] |= (1 << j); // Turn bit on
                        write_block(bitmapDescriptor.dataPtr[b], block);
                        return 1024*8*b + 8*i + j;
                    }
                }
            }
        }
        return -1; // No free blocks
    
    }
    printf("Bitmap is too big, must handle it...\n");
    return -1;
}

int freeBlock(int block)
{
    char buffer[partitionInfo.BlockSize];
    // If block is in one of the first 4 blocks of the bitmap...
    if (block <= SECTOR_SIZE*8*sectorsPerBlock*4)
    {
        int blockToRead = (block/SECTOR_SIZE*8*sectorsPerBlock);
        read_block(blockToRead, buffer);
        int byteToRead = (block - blockToRead*partitionInfo.BlockSize)/8;
        // Clear associated bit
        buffer[byteToRead] &= ~(1 << (block - blockToRead*partitionInfo.BlockSize) % 8);
        write_block(blockToRead, buffer);
        return 0;
    }
    return -1;
}

int readNthBlock(const t2fs_record *rec, int n, char *out)
{
    if (n < 4)
    {
        return read_block(rec->dataPtr[n], out);
    }
    else if (n < 4 + entriesInIndexBlock) // Uses only single indirect
    {
        int ib[partitionInfo.BlockSize/sizeof(int)];
        if (read_block(rec->singleIndPtr, (char*) ib) != 0) return -1;
        n -= 4; // The 4 first are direct pointers (first if)
        read_block(ib[n], out);
        return 0;
    }
    else {
        int ib[partitionInfo.BlockSize/sizeof(int)];
        if (read_block(rec->doubleIndPtr, (char*) ib) != 0) return -1;
        n = (n-68);
        int firstInd = n/(sizeof(ib)/sizeof(int));
        int secondInd = n % (sizeof(ib)/sizeof(int));
        if (read_block(ib[firstInd], (char*) ib) != 0) return -1;
        read_block(ib[secondInd], out);
        return 0;
    }
}

int writeNthBlock(const t2fs_record* rec, int n, char *data)
{
    if (n < 4)
    {
        return write_block(rec->dataPtr[n], data);
    }
    else if (n < 4 + entriesInIndexBlock) // Uses only single indirect
    {
        int ib[partitionInfo.BlockSize/sizeof(int)];
        if (read_block(rec->singleIndPtr, (char*) ib) != 0) return -1;
        n -= 4; // The 4 first are direct pointers (first if)
        write_block(ib[n], data);
        return 0;
    }
    else {
        int ib[partitionInfo.BlockSize/sizeof(int)];
        if (read_block(rec->doubleIndPtr, (char*) ib) != 0) return -1;
        n = (n-68);
        int firstInd = n/(sizeof(ib)/sizeof(int));
        int secondInd = n % (sizeof(ib)/sizeof(int));
        if (read_block(ib[firstInd], (char*) ib) != 0) return -1;
        write_block(ib[secondInd], data);
        return 0;
    }
}

int updateDescriptorOnDisk(const t2fs_record* desc)
{
    // Update on .. and on .
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    t2fs_record buffer2[partitionInfo.BlockSize/sizeof(t2fs_record)];
    read_block(desc->dataPtr[0], (char*)buffer);
    buffer[0].blocksFileSize = desc->blocksFileSize; // sera?
    buffer[0].bytesFileSize = desc->bytesFileSize;
    if (desc->blocksFileSize <= 4)
    {
        buffer[0].dataPtr[desc->blocksFileSize-1] = desc->dataPtr[desc->blocksFileSize-1];
    }
    else if (desc->blocksFileSize == 5)
    {
        buffer[0].singleIndPtr = desc->singleIndPtr;
    }
    else if (desc->blocksFileSize == 4 + entriesInIndexBlock + 1)
    {
        buffer[0].doubleIndPtr = desc->doubleIndPtr;
    }
    write_block(desc->dataPtr[0], (char*)buffer);
    t2fs_record dotdot = buffer[1];
    if (streq2(desc->name, "/"))
    {
        // Is root: update superblock.
        if (desc->blocksFileSize <= 4)
        {
            partitionInfo.RootDirReg.dataPtr[desc->blocksFileSize-1] = desc->dataPtr[desc->blocksFileSize-1];
        }
        else if (desc->blocksFileSize == 5)
        {
            partitionInfo.RootDirReg.singleIndPtr = desc->singleIndPtr;
        }
        else if (desc->blocksFileSize == 4 + entriesInIndexBlock + 1)
        {
            partitionInfo.RootDirReg.doubleIndPtr = desc->doubleIndPtr;
        }
        partitionInfo.RootDirReg.blocksFileSize = desc->blocksFileSize;
        partitionInfo.RootDirReg.bytesFileSize = desc->bytesFileSize;
        write_sector(0, (char*) &partitionInfo);
    }
    else
    {
        bool done = false;
        int i, j;
        for (i = 0; i < dotdot.blocksFileSize && !done; ++i)
        {
            readNthBlock(&dotdot, i, (char*)buffer);
            for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record) && i*partitionInfo.BlockSize + sizeof(t2fs_record)*j < dotdot.bytesFileSize && !done; ++j)
            {
                if (streq2(buffer[j].name, desc->name))
                {
                    if (desc->blocksFileSize <= 4)
                    {
                        buffer[j].dataPtr[desc->blocksFileSize-1] = desc->dataPtr[desc->blocksFileSize-1];
                    }
                    else if (desc->blocksFileSize == 5)
                    {
                        buffer[j].singleIndPtr = desc->singleIndPtr;
                    }
                    else if (desc->blocksFileSize == 4 + entriesInIndexBlock + 1)
                    {
                        buffer[j].doubleIndPtr = desc->doubleIndPtr;
                    }
                    buffer[j].blocksFileSize = desc->blocksFileSize;
                    buffer[j].bytesFileSize = desc->bytesFileSize;
                    writeNthBlock(&dotdot, i, (char*)buffer);
                    done = true;
                }
            }
        }
    }
    int i, j;
    for (i = 0; i < desc->blocksFileSize; ++i)
    {
        readNthBlock(desc, i, (char*)buffer);
        for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record) && i*partitionInfo.BlockSize + sizeof(t2fs_record)*j < desc->bytesFileSize; ++j)
        {
            if (buffer[j].TypeVal == TYPEVAL_DIR && !(streq2(buffer[j].name, ".") || (!streq2(desc->name, "/") && streq2(buffer[j].name, ".."))))
            {
                readNthBlock(&buffer[j], 0, (char*)buffer2);
                if (desc->blocksFileSize <= 4)
                {
                    buffer2[1].dataPtr[desc->blocksFileSize-1] = desc->dataPtr[desc->blocksFileSize-1];
                }
                else if (desc->blocksFileSize == 5)
                {
                    buffer2[1].singleIndPtr = desc->singleIndPtr;
                }
                else if (desc->blocksFileSize == 4 + entriesInIndexBlock + 1)
                {
                    buffer2[1].doubleIndPtr = desc->doubleIndPtr;
                }
                buffer2[1].blocksFileSize = desc->blocksFileSize;
                buffer2[1].bytesFileSize = desc->bytesFileSize;
                writeNthBlock(&buffer[j], 0, (char*)buffer2);
            }
        }
    }
    return 0;
}

int createEntryDir(t2fs_record* dir, const char *filename, int beginBlock, int type, t2fs_record* descriptorOut)
{
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    int i = 0, j;
    bool created = false;
    for (i = 0; i < maxNofBlocks && !created; ++i)
    {
        printf("Trying to create file %s, reading block %d\n", filename, i);
        if (readNthBlock(dir, i, (char*)buffer) == -1)
        {
            // If we get invalid read on a block, we should allocate a new block and update
            // the entry in this directory's parent and in every child to reflect that.
            printf("Block %d returned invalid pointer, allocating...\n", i);
            int newBlock = allocNewBlock();
            if (newBlock == -1) return -1;
            
            if (dir->blocksFileSize < 4)
            {
                dir->dataPtr[dir->blocksFileSize] = newBlock;
                dir->blocksFileSize++;
                //dir.bytesFileSize += sizeof(int);
            }
            else if (dir->blocksFileSize == 4)
            {
                // allocate first index block too
                int indirectBlock = allocNewBlock();
                dir->singleIndPtr = indirectBlock;
                int j;
                int buffer2[partitionInfo.BlockSize/sizeof(int)];
                for (j = 0; j < sizeof(buffer2)/sizeof(int); ++j)
                {
                    buffer2[j] = UNUSED_POINTER;
                }
                buffer2[0] = newBlock;
                dir->blocksFileSize++;
                //dir.bytesFileSize += sizeof(int);
                write_block(indirectBlock, (char*)buffer2);
            }
            else if (dir->blocksFileSize < 4 + entriesInIndexBlock)
            {
                int buffer2[partitionInfo.BlockSize/sizeof(int)];
                read_block(dir->singleIndPtr, (char*) buffer2);
                int j;
                for (j = 0; j < sizeof(buffer2)/sizeof(int); ++j)
                {
                    if (buffer2[j] == UNUSED_POINTER)
                    {
                        buffer2[j] = newBlock;
                        break;
                    }
                }
                dir->blocksFileSize++;
                //dir.bytesFileSize+=sizeof(int);
                write_block(dir->singleIndPtr, (char*) buffer2);
            }
            else if (dir->blocksFileSize == 4 + entriesInIndexBlock)
            {
                // Need to use double indirection
                int doubleIndirectBlock = allocNewBlock();
                dir->doubleIndPtr = doubleIndirectBlock;
                int j;
                int buffer2[partitionInfo.BlockSize/sizeof(int)];
                for (j = 0; j < sizeof(buffer2)/sizeof(int); ++j)
                    buffer2[j] = UNUSED_POINTER;
                int simpleIndirectBlock = allocNewBlock();
                buffer2[0] = simpleIndirectBlock;
                write_block(doubleIndirectBlock, (char*)buffer2);
                buffer2[0] = newBlock;
                write_block(simpleIndirectBlock, (char*)buffer2);
                dir->blocksFileSize++;
                //dir.bytesFileSize+= sizeof(int);
            }
            else if (dir->blocksFileSize < maxNofBlocks && (dir->blocksFileSize - entriesInIndexBlock - 4)%entriesInIndexBlock == 0)
            {
                int buffer2[partitionInfo.BlockSize/sizeof(int)];
                int buffer3[partitionInfo.BlockSize/sizeof(int)];
                read_block(dir->doubleIndPtr, (char*) buffer2);
                int j, k;
                for (j = 0; j < entriesInIndexBlock; ++j)
                {
                    if (buffer2[j] == UNUSED_POINTER)
                    {
                        int singleIndBlock = allocNewBlock();
                        buffer2[j] = singleIndBlock;
                        buffer3[k] = newBlock;
                        for (k = 1; k < entriesInIndexBlock; ++k)
                            buffer3[k] = UNUSED_POINTER;
                        write_block(singleIndBlock, (char*)buffer3);
                        break;
                    }
                }
                dir->blocksFileSize++;
                //dir.bytesFileSize += sizeof(int);
                write_block(dir->doubleIndPtr, (char*) buffer2);
            }
            else if (dir->blocksFileSize < maxNofBlocks)
            {
                int dBlockToRead = -1;
                int j;
                int buffer2[partitionInfo.BlockSize/sizeof(int)];
                read_block(dir->doubleIndPtr, (char*) buffer2);
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
                dir->blocksFileSize++;
                //dir.bytesFileSize += sizeof(int);
                write_block(dBlockToRead, (char*) buffer2);
            }
            dir->bytesFileSize += partitionInfo.BlockSize;
            // read_block(newBlock, (char*)buffer);
            for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record); ++j)
            {
                buffer[j] = (t2fs_record) { .TypeVal = TYPEVAL_INVALID } ;
            }
            write_block(newBlock, (char*)buffer);

            //updateDescriptorOnDisk(&dir);
        }
        for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record) && 
                    i*(partitionInfo.BlockSize) + j*sizeof(t2fs_record) < dir->bytesFileSize; ++j)
        {
            t2fs_record r = buffer[j];
            if (r.TypeVal != TYPEVAL_INVALID && !strcmp(r.name, filename))
            {
                printf("Name conflict!\n");
                return -1;            
            }
        }
        for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record) && !created ; ++j)
        {
            t2fs_record r = buffer[j];
            if (r.TypeVal != TYPEVAL_DIR && r.TypeVal != TYPEVAL_FILE)
            {
                r.TypeVal = type;
                if (strncpy2(r.name, filename, sizeof(r.name)) == -1){
                    printf("Name too big: %s\n", filename);
                    return -1;
                }
                r.blocksFileSize = 1;
                r.bytesFileSize = 0;
                r.dataPtr[0] = beginBlock;
                r.dataPtr[1] = r.dataPtr[2] = r.dataPtr[3] = UNUSED_POINTER;
                r.singleIndPtr = UNUSED_POINTER;
                r.doubleIndPtr = UNUSED_POINTER;
                
                buffer[j] = r;
                
                
                *descriptorOut = r;
                created = true;
            }
        }
    }
    i--;
    writeNthBlock(dir, i, (char*)buffer);
    updateDescriptorOnDisk(dir);
    
    return 0;
}

t2fs_record findParentDescriptor(const char *filename, char *outName)
{
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    char l_filename[MAX_FILE_NAME_SIZE + 1];
    strcpy(l_filename, filename);
    t2fs_record dir;
    if (filename[0] == '/')
        dir = partitionInfo.RootDirReg;
    else
        dir = cwdDescriptor;
    char *token;
    char lastToken[MAX_FILE_NAME_SIZE + 1];
    if (strstr(filename, "/") != NULL)
    {
        token = strtok(l_filename, "/");
        do 
        {
            strcpy(lastToken, token);
            token = strtok(NULL, "/");
            if (token == NULL) break;
            int i, j;
            bool found = false;
            for (i = 0; i < dir.blocksFileSize && !found; ++i)
            {
                if (readNthBlock(&dir, i, (char*) buffer) != 0)  break;
                for (j = 0; i*(partitionInfo.BlockSize) + j*sizeof(t2fs_record) <= dir.bytesFileSize && !found; ++j)
                {
                    if (streq2(lastToken, buffer[j].name))
                    {
                        dir = buffer[j];
                        found = true;
                    }
                }
            }
            if (!found) return (t2fs_record) { .TypeVal = TYPEVAL_INVALID };
            
        } while (token != NULL);
        outName != NULL ? strcpy(outName, lastToken) : 0 ;
        return dir;
    }
    else { strcpy(outName, filename); return dir; }
}

t2fs_record findFileInDir(const t2fs_record *dir, const char *filename)
{
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    int i;
    for (i = 0; i < dir->blocksFileSize; ++i)
    {
        readNthBlock(dir, i, (char*)buffer);
        int j;
        for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record); ++j)
        {
            if (buffer[j].TypeVal == TYPEVAL_FILE && streq2(filename, buffer[j].name))
            {
                return buffer[j];
            }
        }
    }
    return (t2fs_record) { .TypeVal = TYPEVAL_INVALID };
}


int createEntryAbsolute(const char *filename, int beginBlock, int type, t2fs_record* descriptorOut)
{
    char l_filename[MAX_FILE_NAME_SIZE + 1];
    strcpy(l_filename, filename);
    t2fs_record dir = partitionInfo.RootDirReg;
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    char *token;
    char lastToken[MAX_FILE_NAME_SIZE + 1];
    
    if (strstr(l_filename, "/") != NULL)
    {
        token = strtok(l_filename, "/");
        do 
        {
            strcpy(lastToken, token);
            token = strtok(NULL, "/");
            if (token == NULL) break;
            int i, j;
            bool found = false;
            for (i = 0; i < dir.blocksFileSize && !found; ++i)
            {
                if (readNthBlock(&dir, i, (char*) buffer) != 0)  break;
                for (j = 0; i*(partitionInfo.BlockSize) + j*sizeof(t2fs_record) <= dir.bytesFileSize && !found; ++j)
                {
                    if (streq2(lastToken, buffer[j].name))
                    {
                        dir = buffer[j];
                        found = true;
                    }
                }
            }
            if (!found) return -1;
            
        } while (token != NULL);
        if (createEntryDir(&dir, lastToken, beginBlock, type, descriptorOut) == -1)
        {
            return -1;
        }
        if (streq2(dir.name, "/"))
        {
            partitionInfo.RootDirReg = dir;
        }
        if (streq2(cwdDescriptor.name, dir.name))
        {
            cwdDescriptor = dir;
        }
        return 0;
    }
    else {
        if (createEntryDir(&dir, filename, beginBlock, type, descriptorOut) == -1)
        {
            return -1;
        }
        if (streq2(dir.name, "/"))
        {
            partitionInfo.RootDirReg = dir;
        }
        if (streq2(cwdDescriptor.name, dir.name))
        {
            cwdDescriptor = dir;
        }
    }
    return 0;
}

int createEntryRelative(const char *filename, int beginBlock, int type, t2fs_record* descriptorOut)
{
    char l_filename[MAX_FILE_NAME_SIZE + 1];
    strcpy(l_filename, filename);
    t2fs_record dir = cwdDescriptor;
    t2fs_record buffer[partitionInfo.BlockSize/sizeof(t2fs_record)];
    char *token;
    char lastToken[MAX_FILE_NAME_SIZE + 1];
    if (strstr(l_filename, "/") != NULL)
    {
        token = strtok(l_filename, "/");
        do 
        {
            strcpy(lastToken, token);
            token = strtok(NULL, "/");
            if (token == NULL) break;
            int i, j;
            bool found = false;
            for (i = 0; i < dir.blocksFileSize && !found; ++i)
            {
                readNthBlock(&dir, i, (char*) buffer);
                for (j = 0; i*(partitionInfo.BlockSize) + j*sizeof(t2fs_record) <= dir.bytesFileSize && !found; ++j)
                {
                    if (streq2(lastToken, buffer[j].name))
                    {
                        dir = buffer[j];
                        found = true;
                    }
                }
            }
            if (!found) return -1;
            
        } while (token != NULL);
        if (createEntryDir(&dir, lastToken, beginBlock, type, descriptorOut) == -1)
        {
            return -1;
        }
        if (streq2(dir.name, "/"))
        {
            partitionInfo.RootDirReg = dir;
        }
        if (streq2(cwdDescriptor.name, dir.name))
        {
            cwdDescriptor = dir;
        }
        return 0;
    }
    else {
        if (createEntryDir(&dir, filename, beginBlock, type, descriptorOut) == -1)
        {
            return -1;
        }
        if (streq2(dir.name, "/"))
        {
            partitionInfo.RootDirReg = dir;
        }
        if (streq2(cwdDescriptor.name, dir.name))
        {
            cwdDescriptor = dir;
        }
    }
    return 0;
}

