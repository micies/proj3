#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024
#define BLOCK_SIZE 4
#define FREE 0
#define TAKEN 1

int *bitmap = NULL; //initialized when mount

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

void build_bitmap(){
	union fs_block block;
	disk_read(0,block.data);
	int ninodeblocks = block.super.ninodeblocks;
	int nblocks = block.super.nblocks;
	int ninodes = block.super.ninodes;
	int i,j,k;
	int fileblocks; // file_size/block_size
	struct fs_inode inode;
	union fs_block datablock;
	bitmap = (int *)malloc(nblocks);
	memset(bitmap,0,nblocks);
	bitmap[0] = TAKEN;
	for(i = 1; i <= ninodeblocks; i++){
		bitmap[i] = TAKEN;
		disk_read(i,block.data);
		for(j = 0; j < INODES_PER_BLOCK; j++){
			inode = block.inode[j];
			if(inode.isvalid){
				fileblocks = inode.size / BLOCK_SIZE;
				for(k = 0; k < POINTERS_PER_INODE; k++){
					bitmap[inode.direct[k]] = TAKEN;
				}
				if(fileblocks > POINTERS_PER_INODE){
					disk_read(inode.indirect,datablock.data);
					for(k = 0; k < (fileblocks-POINTERS_PER_INODE); k++){
						bitmap[datablock.pointers[k]] = TAKEN;
					}
				}
			}
		}
	}
}

//an attempt to format an already-mounted disk should do nothing and return failure
int fs_format()
{
	return 0;
}

//Scan a mounted filesystem and report on how the inodes and blocks are organized
void fs_debug()
{
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
}

//build a new free block bitmap
int fs_mount()
{
	union fs_block block;
	disk_read(0,block.data);
	if(block.super.magic != FS_MAGIC){
		int nblocks = block.super.nblocks;		
		bitmap = (int *)malloc(nblocks);
		memset(bitmap,0,nblocks);
		block.super.magic = FS_MAGIC;
		return 1;
	}
	printf("It has already been mounted!\n");
	return 0;
}

int fs_create()
{
	return -1;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}