#pragma once
#ifndef AUX_H
#define AUX_H

#include "t2fs.h"
#include "apidisk.h"
#include <string.h>



SUPERBLOCK partitionInfo;

int cwdDescriptorSector;

t2fs_record cwdDescriptor;
t2fs_record bitmapDescriptor;

int sectorsPerBlock;
int entriesInIndexBlock;

void copyRecord(t2fs_record *dst, const t2fs_record *org);

inline int min(int a, int b);
int strncpy2(char *dst, const char *org, int n);
bool streq2(const char *s1, const char *s2);
void printRecordInfo(t2fs_record *r);
void printSuperblockInfo(SUPERBLOCK *s);
int readSuperblock();
int read_block(int block, char *out);
int write_block(int block, char *data);
int allocNewBlock();
int freeBlock(int block);
int readNthBlock(const t2fs_record *rec, int n, char *out);
int writeNthBlock(const t2fs_record* rec, int n, char *data);
int invalidateNthBlock(t2fs_record *rec, int n);
int updateDescriptorOnDisk(const t2fs_record* desc);
int createEntryDir(t2fs_record* dir, const char *filename, int beginBlock, int type, t2fs_record* descriptorOut);
t2fs_record findParentDescriptor(const char *filename, char *outName);
t2fs_record findFileInDir(const t2fs_record *dir, const char *filename);
t2fs_record findDirDescriptor(const char *filename);
int createEntryAbsolute(const char *filename, int beginBlock, int type, t2fs_record* descriptorOut);
int createEntryRelative(const char *filename, int beginBlock, int type, t2fs_record* descriptorOut, char *absPath, const char *cwdPath);
#endif
