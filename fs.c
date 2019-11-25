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

int fs_format()
{
	return 0;
}

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

int fs_mount()
{
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
