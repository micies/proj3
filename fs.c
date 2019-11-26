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
	//return fail if already mounted
	
	if(!fs_mount()){
		printf("disk is already mounted\n");
		return 0;
	}
	int nblocks = disk_size();
	
	//clear all the data existed in blocks

	// initialize super block
	char data[DISK_BLOCK_SIZE];
	//set nblocks
	data[4] = nblocks;
	//set ninode block
	int inodes = (int)(nblocks*0.1) + ((nblocks%10 == 0) ? 0 : 1) + 1;
	data[8] = inodes;
	disk_write(0, data);

	//set aside ten percent blocks as inode block
	// bit map should obey the rule that the first block is for super block
	//and the first 10% blocks are used for inodes
	for(int i = 0; i < inodes - 1; i++){
		char temp_data[DISK_BLOCK_SIZE];
		temp_data[0] = (int)0;
		temp_data[4] = (int)8;
		disk_write(i+1, temp_data);
	}

	//block.super.ninodeblocks
	return 1;
}

//Scan a mounted filesystem and report on how the inodes and blocks are organized
void fs_debug()
{
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	if (block.super.magic == FS_MAGIC){
		printf("    magic number is valid\n");
	}
	else {
		printf("    magic number is not valid\n");
	}
	printf("    %d blocks on disk\n",block.super.nblocks);
	printf("    %d blocks for inodes\n",block.super.ninodeblocks);
	printf("    %d inodes toal\n",block.super.ninodes);

	int ninodeblocks = block.super.ninodeblocks;
	if (ninodeblocks < 0){return;}
	for (int i = 0; i< ninodeblocks; i++){ // each inode block
		disk_read(i+1,block.data);
		for (int j = 0; j < INODES_PER_BLOCK; j++){ // each inode
			struct fs_inode inode = block.inode[j];
			if (!inode.isvalid){
				//printf("inode.isvalid %d\n", inode.isvalid);
				continue;
			}
			printf("inode %d:\n", j+i*INODES_PER_BLOCK);
			printf("    size: %d bytes\n",fs_getsize(j+i*INODES_PER_BLOCK));
			printf("    direct blocks: ");
			for (int k = 0;k<POINTERS_PER_INODE; k++){
				int pointedblock = inode.direct[k];
				if (pointedblock != 0){
					printf("%d ", pointedblock);
				}
			}
			printf("\n");

			if (!inode.indirect){continue;}
			printf("    indirect block: %d\n", inode.indirect);
			printf("    indirect data blocks: "); 
			union fs_block indirectblock;
			disk_read(inode.indirect, indirectblock.data);
			for (int l = 0; l < POINTERS_PER_BLOCK; l++){
                        	if (indirectblock.pointers[l]!=0){
					printf("%d ",indirectblock.pointers[l]);
				}
			
			}
			printf("\n");	
		}
	}
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
	int blocknum = inumber/INODES_PER_BLOCK;
	int inodenum = inumber%INODES_PER_BLOCK;
	union fs_block block;

	disk_read(0,block.data);
	if (inumber>block.super.ninodes){
		printf("Please enter a number within 1 ~ ninodes");
		return -1;
	}
	else if (inumber==0){
		printf("Please enter a number within 1 ~ ninodes");
                return -1;
	}
	
	// read a inode
	disk_read(blocknum+1, block.data);
	struct fs_inode inode = block.inode[inodenum];	
	return inode.size;

	//return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
