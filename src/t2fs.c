#include "apidisk.h"
#include "t2fs.h"
#include <stdio.h> // TODO - Remove
#include <stdlib.h>

#define TEST_INIT_PARTINFO if (!partitionInfoInitialized) { \
	if (!readSuperblock()) return -1; \
	partitionInfoInitialized = true;\
	}

OPEN_FILE openFiles[20];
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
    
    maxNofBlocks = 4 + nEntriesIndexBlock + nEntriesIndexBlock*nEntriesIndexBlock;
    
	return 0;
}
// Gets a free block, marks it on the bitmap, saves the bitmap to disk
// and returns the number of the block.
// INCOMPLETE
int allocNewBlock()
{
    char sector[SECTOR_SIZE];
    int s;
    if (bitmapDescriptor.blocksFileSize <= 4)
    {
        for (s = 0; s < sectorsPerBlock && s < bitmapDescriptor.blocksFileSize; ++s)
        {
            int sectorToRead = BLOCK_TO_SECTOR(bitmapDescriptor.dataPtr[s]);
            if (sectorToRead == -1) return -1;
            int i, j;
            for (i = 0; i < bitmapDescriptor.bytesFileSize; ++i)
            {
                char byte = sector[i];
                for (j = 0; j < 8; ++j)
                {
                    if ((byte & (1 << j)) == 0)
                    {
                        // FREE BLOCK!
                        sector[i] |= (1 << j); // Turn bit on
                        write_sector(sectorToRead, sector);
                        return 256*8*s + 8*i + j;
                    }
                }
            }
        }
        return -1; // No free blocks
    
    }
    printf("Bitmap is too big, must handle it...\n");
    return -1;
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

int readNthBlock(const t2fs_record *rec, int n, char *out)
{
    int i;
    if (n < 4)
    {
        return read_block(n, out);
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

int createEntryCwd(char *filename, int beginBlock, int type)
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
       }
        for (j = 0; j < sizeof(buffer)/sizeof(t2fs_record) && !created ; ++j)
        {
            t2fs_record r = buffer[j];
            if (r.TypeVal != 0x01 && r.TypeVal != 0x02)
            {
                r.TypeVal = type;
                if (strncpy2(r.name, filename, sizeof(r.name)) == -1){
                    printf("Name too big: %s\n", filename);
                    return -1;
                }
                r.blocksFileSize = 1;
                r.bytesFileSize = 0;
                r.dataPtr[0] = beginBlock;
                memset(r.dataPtr + 1, 3*sizeof(int), 0xFF);
                r.singleIndPtr = UNUSED_POINTER;
                r.doubleIndPtr = UNUSED_POINTER;
                
                buffer[j] = r;
            
                created = true;
            }
            
        }
    }
    i--;
    return 0;
}

int identify2(char *name, int size)
{
	// TODO talvez tirar daqui e deixar só nas outras.
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

int create2(char *filename)
{
	TEST_INIT_PARTINFO
    // find free block
    // alloc free block
    // add entry to directory
    int newBlock = allocNewBlock();
    if (newBlock == -1) return -1;
    
}



// Test
int main()
{
	createEntryCwd("hello.dat", 2);
	return 0;
}
