#include "apidisk.h"
#include "t2fs.h"
#include <stdio.h> // TODO - Remove
#include <stdlib.h>

#define TEST_INIT_PARTINFO if (!partitionInfoInitialized) { \
	if (readSuperblock() != 0) return -1; \
	partitionInfoInitialized = true;\
	}

OPEN_FILE openFiles[20];
int nOpenFiles = 0;


SUPERBLOCK partitionInfo;
bool partitionInfoInitialized = false;

int cwdDescriptorSector;

t2fs_record cwdDescriptor;
t2fs_record bitmapDescriptor;

int sectorsPerBlock;

// TODO - OPEN DIRS
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
    int i = 0;
    while (s1[i] != '\0')
    {
        if (s2[i] != s1[i])
            return false;
        i++;
    }
    return s2[i] == '\0';
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
    maxNofBlocks = 4 + nEntriesIndexBlock + nEntriesIndexBlock*nEntriesIndexBlock;
    
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
    else if (n < 68) // Uses only single indirect
    {
        INDEX_BLOCK ib;
        if (read_block(rec->singleIndPtr, (char*) &ib) != 0) return -1;
        n -= 4; // The 4 first are direct pointers (first if)
        read_block(ib.pointers[n], out);
        return 0;
    }
    else {
        INDEX_BLOCK ib;
        if (read_block(rec->doubleIndPtr, (char*) &ib) != 0) return -1;
        n = (n-68);
        int firstInd = n/nEntriesIndexBlock;
        int secondInd = n % nEntriesIndexBlock;
        if (read_block(ib.pointers[firstInd], (char*) &ib) != 0) return -1;
        read_block(ib.pointers[secondInd], out);
        return 0;
    }
}

int writeNthBlock(const t2fs_record* rec, int n, char *data)
{
    if (n < 4)
    {
        return write_block(rec->dataPtr[n], data);
    }
    else if (n < 68) // Uses only single indirect
    {
        INDEX_BLOCK ib;
        if (read_block(rec->singleIndPtr, (char*) &ib) != 0) return -1;
        n -= 4; // The 4 first are direct pointers (first if)
        write_block(ib.pointers[n], data);
        return 0;
    }
    else {
        INDEX_BLOCK ib;
        if (read_block(rec->doubleIndPtr, (char*) &ib) != 0) return -1;
        n = (n-68);
        int firstInd = n/nEntriesIndexBlock;
        int secondInd = n % nEntriesIndexBlock;
        if (read_block(ib.pointers[firstInd], (char*) &ib) != 0) return -1;
        write_block(ib.pointers[secondInd], data);
        return 0;
    }
}


int createEntryCwd(char *filename, int beginBlock, int type, t2fs_record* descriptorOut)
{
    t2fs_record buffer[SECTOR_SIZE*sectorsPerBlock/sizeof(t2fs_record)];
    int i = 0, j;
    bool created = false;
    for (i = 0; i < maxNofBlocks && !created; ++i)
    {
        if (readNthBlock(&cwdDescriptor, i, (char*)buffer) == -1)
        {
            // If we get invalid read on a block, we should allocate a new block and update
            // the entry in this directory's parent to reflect that.
            printf("OH NOES, MUST USE INDIRECTIONZ!!!\n");
            return -1;
        }
        for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record) && !created ; ++j)
        {
            t2fs_record r = buffer[j];
            if (r.TypeVal == TYPEVAL_FILE && streq2(r.name, filename))
                return -1;
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
    writeNthBlock(&cwdDescriptor, i, (char*)buffer);
    return 0;
}

int identify2(char *name, int size)
{
	if (!partitionInfoInitialized)
	{
		if (!readSuperblock()) return -1;
		partitionInfoInitialized = true;
	}
	char id[] = "Alister M. - 220494, Eduardo V. - 218313";
	if (size < sizeof(id))
		return -1;
	else strncpy2(name, id, sizeof(id));
	return 0;
}

FILE2 create2(char *filename)
{
    if (nOpenFiles == 20) return -1;
	TEST_INIT_PARTINFO
    // find free block
    // alloc free block
    // add entry to directory
    t2fs_record r;
    int newBlock = allocNewBlock();
    if (newBlock == -1) return -1;
    
    if (createEntryCwd(filename, newBlock, TYPEVAL_FILE, &r) != 0)
    {
        freeBlock(newBlock);
        return -1;
    }
    
    openFiles[nOpenFiles] = (OPEN_FILE) {
        .descriptor = r,
        .offset = 0,
        .dirDescriptor = cwdDescriptor
    };
    nOpenFiles++;
    return nOpenFiles - 1;
}



// Test
int main()
{ 
    char buffer[1024];
    char filename[] = "hello2.txt";
    TEST_INIT_PARTINFO

	printf("Creating...\n");
    int i;
    for (i = '0'; i-'0' <= 16; ++i)
    {
        filename[5] = i;
        if (create2(filename) == -1) printf("Didnt create file %s.\n", filename);
    }
    int j;
    for(j = 0; j < 4; ++j)
    {
        if (readNthBlock(&cwdDescriptor, j, buffer) == -1) break;
        for (i = 0; i < 16; ++i)
        {
            printf("----------- RECORD %d ------------\n", i);
            printRecordInfo((t2fs_record*) buffer + i);
        }
    }
    
    return 0;
}
