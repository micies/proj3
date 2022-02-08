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
#define BLOCK_SIZE 4096
#define FREE 0
#define TAKEN 1



int * bitmap = NULL; //initialized when mount

int built = 0;
int copysize;

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


//an attempt to format an already-mounted disk should do nothing and return failure
int fs_format()
{
	//return fail if already mounted
	
	if(bitmap != NULL){
		printf("disk is already mounted\n");
		return 0;
	}
	int nblocks = disk_size();
	
	//clear all the data existed in blocks

	// initialize super block
	union fs_block data;
	//set nblocks
	data.super.nblocks = nblocks;
	//set ninode block
	int inodesblocks = (int)(nblocks*0.1) + ((nblocks%10 == 0) ? 0 : 1);
	data.super.ninodeblocks = inodesblocks;
	data.super.ninodes = inodesblocks * INODES_PER_BLOCK;
	data.super.magic = FS_MAGIC;
	//printf("in format: ninodesblocks: %d ninodes: %d\n",data.super.ninodeblocks, data.super.ninodes);
	disk_write(0, data.data);

	//set aside ten percent blocks as inode block
	// bit map should obey the rule that the first block is for super block
	//and the first 10% blocks are used for inodes
	for(int i = 1; i < inodesblocks; i++){
		union fs_block block;
		for(int j = 0; j < INODES_PER_BLOCK; j++){
			block.inode[j].isvalid = 0;
			block.inode[j].size = 0;
			// clear all the pointer(direct & indrect)
			memset(block.inode[j].direct, 0, POINTERS_PER_INODE * 4);
			block.inode[j].indirect = 0;
		}
		disk_write(i, block.data);
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
	printf("    %d inodes total\n",block.super.ninodes);

	int ninodeblocks = block.super.ninodeblocks;
	if (ninodeblocks < 0){return;}
	for (int i = 1; i< ninodeblocks; i++){ // each inode block
		disk_read(i,block.data);
		for (int j = 0; j < INODES_PER_BLOCK; j++){ // each inode
			struct fs_inode inode = block.inode[j];
			if (!inode.isvalid){
				//printf("inode.isvalid %d\n", inode.isvalid);
				continue;
			}
			printf("inode %d:\n", j+i*INODES_PER_BLOCK + 1);
			printf("    size: %d bytes\n",fs_getsize(j+i*INODES_PER_BLOCK + 1));
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
	if(bitmap != NULL){
		printf("It has already been mounted!\n");
		return 0;
	}
	union fs_block block;
	disk_read(0,block.data);
	int ninodeblocks = block.super.ninodeblocks;
	int nblocks = block.super.nblocks;
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
				if(inode.size % BLOCK_SIZE){
					fileblocks++;
				}
				for(k = 0; k < POINTERS_PER_INODE; k++){
					bitmap[inode.direct[k]] = TAKEN;
				}
				if(fileblocks > POINTERS_PER_INODE){
					disk_read(inode.indirect,datablock.data);
					for(k = 0; k < (fileblocks - POINTERS_PER_INODE); k++){
						bitmap[datablock.pointers[k]] = TAKEN;
					}
				}
			}
		}
	}
	return 1;
}

int fs_create()
{
	if(bitmap == NULL){
		printf("The disk haven't been mounted!\n");
		return -1;
	}
	// search through the top 10% blocks finding the first avalible inode
	union fs_block block;
	disk_read(0, block.data);
	for(int i = 0; i < block.super.ninodeblocks; i++){
		union fs_block tempblock;
		disk_read(i+1, tempblock.data);
		for(int j = 0; j < INODES_PER_BLOCK; j++){
			if(tempblock.inode[j].isvalid == 0){
				tempblock.inode[j].isvalid = 1;
				tempblock.inode[j].size = 0;
				disk_write(i+1, tempblock.data);
				disk_write(0, block.data);
				printf("create with an inumber of : %d", i*INODES_PER_BLOCK + j + 1);
				return i * INODES_PER_BLOCK + j + 1;
			}
		}
	}
	return -1;
}

int fs_delete(int inumber)
{
	if(bitmap == NULL){
		printf("The disk haven't been mounted!\n");
		return 0;
	}
	//check if the input inumber if valid
	union fs_block superblock;
	disk_read(0, superblock.data);	
	if(inumber > superblock.super.ninodes || inumber == 0){
		printf("The inumber is invalid!\n");
		return 0;
	}

	int blocknum = (inumber - 1) /INODES_PER_BLOCK + 1;
	int inodenum = (inumber - 1) %INODES_PER_BLOCK;
	union fs_block block;	
	disk_read(blocknum, block.data);
	struct fs_inode inode = block.inode[inodenum];

	if(inode.isvalid){
		int fileblocks = inode.size/BLOCK_SIZE + ((inode.size%BLOCK_SIZE == 0)?0:1);
		for(int i = 0; i < POINTERS_PER_INODE; i++){
			if(inode.direct[i] == 0)
				continue;
			bitmap[inode.direct[i]] = FREE;
		}
		if(fileblocks > POINTERS_PER_INODE){
			union fs_block datablock;
			disk_read(inode.indirect, datablock.data);
			for(int k = 0; k < (fileblocks - POINTERS_PER_INODE); k++){
				if(inode.direct[k] == 0)
					continue;
				bitmap[datablock.pointers[k]] = FREE;
			}
		}
		block.inode[inodenum].isvalid = 0;
		block.inode[inodenum].size = 0;
		memset(block.inode[inodenum].direct, 0, POINTERS_PER_INODE * 4);
		block.inode[inodenum].indirect = 0;
		disk_write(blocknum, block.data);
		disk_write(0, superblock.data);
	}
	return 1;
}


int fs_getsize( int inumber )
{
	union fs_block superblock;
	disk_read(0, superblock.data);	
	if(inumber > superblock.super.ninodes || inumber == 0){
		printf("The inumber is invalid!\n");
		return 0;
	}

	int blocknum = (inumber - 1) /INODES_PER_BLOCK + 1;
	int inodenum = (inumber - 1) %INODES_PER_BLOCK;
	union fs_block block;
	disk_read(blocknum, block.data);
	struct fs_inode inode = block.inode[inodenum];
	if(inode.isvalid == 0){
		printf("inumber is not valid. Not create yet.\n");
		return -1;
	}	
	//printf("%d\n", inode.size);
	return inode.size;

}


int fs_read( int inumber, char *data, int length, int offset )
{	
	if(bitmap == NULL){
		printf("The disk haven't been mounted!\n");
		return -1;
	}
	union fs_block superblock;
	disk_read(0, superblock.data);	
	if(inumber > superblock.super.ninodes || inumber == 0){
		printf("The inumber is invalid!\n");
		return 0;
	}
	int blocknum = (inumber - 1) /INODES_PER_BLOCK + 1;
	int inodenum = (inumber - 1) %INODES_PER_BLOCK;
	union fs_block block;



	disk_read(0,block.data);
	if(block.super.magic == FS_MAGIC){
		union fs_block block;
		disk_read(blocknum, block.data);
		struct fs_inode inode = block.inode[inodenum];
		if(inode.isvalid){
			//check if input is valid
			if(inode.size < offset) 
				return 0;
			int copysize = (inode.size - offset  < length) ? inode.size - offset : length;
			if(copysize > 0){
				union fs_block indirect;
				disk_read(inode.indirect, indirect.data);
				int blockbegin = offset / BLOCK_SIZE;
				int blockoffset = offset % BLOCK_SIZE;
				int datablocknum = (copysize > (BLOCK_SIZE - offset)) ? (copysize - (BLOCK_SIZE - blockoffset)) / BLOCK_SIZE : 0;
				int first_length = (copysize > BLOCK_SIZE - blockoffset) ? BLOCK_SIZE - blockoffset : copysize;	
				int last = (copysize > first_length)?(copysize - first_length) % BLOCK_SIZE : 0;

				//copy the first number
				//printf("length %d \t offset %d \t copysize %d, \tfirst length %d \t last %d\n", length, offset, copysize, first_length, last);
				union fs_block datablock;
				if(blockbegin < POINTERS_PER_INODE){
					disk_read(inode.direct[blockbegin], datablock.data);
				}
				else{
					int tempblocknum = indirect.pointers[blockbegin - POINTERS_PER_INODE];
					disk_read(tempblocknum, datablock.data);
				}
				memcpy(data, datablock.data, first_length);
			
				//printf("first block complete, datablock number:%d\n", datablocknum);
				//copy rest blocks
				for(int i = 1; i <= datablocknum; i++){
					union fs_block tempblock;
					if(blockbegin + i < POINTERS_PER_INODE){
						disk_read(inode.direct[blockbegin + i], tempblock.data);
					}else{
						int tempblocknum = indirect.pointers[blockbegin + i - POINTERS_PER_INODE];
						disk_read(tempblocknum, tempblock.data);
					}
					memcpy(data + first_length + BLOCK_SIZE * (i-1), tempblock.data, BLOCK_SIZE);
				}

				if(last != 0){
					//printf("copy last block\n");
					union fs_block tempblock;
					if(blockbegin + datablocknum + 1 < POINTERS_PER_INODE){
						disk_read(inode.direct[blockbegin + datablocknum + 1], tempblock.data);
					}else{
						int tempblocknum = indirect.pointers[blockbegin + datablocknum + 1 - POINTERS_PER_INODE];
						disk_read(tempblocknum, tempblock.data);
					}
					memcpy(data + first_length + BLOCK_SIZE * datablocknum, tempblock.data, BLOCK_SIZE);
					//printf("last block copy complete\n");
				}
			}
			return copysize;
		}
	}
	return 0;
}

int findFree(){
	if(bitmap == NULL){
		printf("The disk haven't been mounted!\n");
		return -1;
	}
	union fs_block block;
	disk_read(0, block.data);
	int nblocks = block.super.nblocks;
	int inodesblocks = block.super.ninodeblocks;
	for(int i = inodesblocks; i < nblocks; i++){
		if(bitmap[i] == FREE){
			return i;
		}
	}
	return -1;
}


int fs_write( int inumber, const char *data, int length, int offset )
{
	printf("enter write\n");
	printf("length %d, /t offset %d\n", length, offset);
	if(bitmap == NULL){
		printf("The disk haven't been mounted!\n");
		return -1;
	}
	union fs_block superblock;
	disk_read(0, superblock.data);	
	if(inumber > superblock.super.ninodes || inumber == 0){
		printf("The inumber is invalid!\n");
		return 0;
	}

	int blocknum = (inumber - 1) /INODES_PER_BLOCK + 1;
	int inodenum = (inumber - 1) %INODES_PER_BLOCK;
	union fs_block block;
	int ret = 0;

	disk_read(0,block.data);
	if(block.super.magic == FS_MAGIC){
		union fs_block block;
		disk_read(blocknum, block.data);
		struct fs_inode inode = block.inode[inodenum];
		if(inode.isvalid){
			//check the input
			if(inode.size < offset)
				return ret;
			int blockbegin = offset / BLOCK_SIZE;
			int blockoffset = offset % BLOCK_SIZE;
			int datablocknum = (length > (BLOCK_SIZE - offset))?(length - (BLOCK_SIZE - blockoffset)) / BLOCK_SIZE : 0;
			int first_length = (length > BLOCK_SIZE - blockoffset) ? BLOCK_SIZE - blockoffset : length;	
			int last = (first_length < length)?(length - first_length) % BLOCK_SIZE : 0;
	
			int directblocknum = datablocknum + blockbegin + 1;
			for(int i = 0; i < directblocknum; i++){
				if(inode.direct[i] != 0)
					continue;
				int freeblock = findFree();
				if(freeblock != -1){
					bitmap[freeblock] = TAKEN;
					inode.direct[i] = freeblock;
				}
			}
			
			int extrablock = datablocknum + blockbegin + 1 - POINTERS_PER_INODE;
			// allocate indirect pointer
			if(extrablock > 0 && inode.indirect == 0){
				int freeblock = findFree();
				if(freeblock != -1){
					bitmap[freeblock] = TAKEN;
					inode.indirect = freeblock;
				}
			}
			if(extrablock > 0){
				union fs_block indirect;
				disk_read(inode.indirect, indirect.data);
				// allocate space for data
				for(int k = 0; k < extrablock; k++){
					if(indirect.pointers[k] != 0)
						continue;
					int tempfree = findFree();
					if(tempfree != -1){
						bitmap[tempfree] = TAKEN;
						indirect.pointers[k] = tempfree;
					}else{
						indirect.pointers[k] = 0;
					}
				}
				disk_write(inode.indirect, indirect.data);
			}
			
			block.inode[inodenum] = inode;
			disk_write(blocknum, block.data);

			//write the first block
			union fs_block indirect;
			disk_read(inode.indirect, indirect.data);
			union fs_block datablock;
			int tempblocknum;
			if(blockbegin > POINTERS_PER_INODE){
				tempblocknum = indirect.pointers[blockbegin - POINTERS_PER_INODE];
			}else{
				tempblocknum = inode.direct[blockbegin];
			}

			if(tempblocknum == 0){
				block.inode[inodenum].size += ret;
				disk_write(blocknum, block.data);
				return ret;
			}
			
			disk_read(tempblocknum, datablock.data);
			memcpy(datablock.data + blockoffset, data, first_length);
			disk_write(inode.direct[blockbegin], datablock.data);
			ret += first_length;

			//write the rest block
			for(int i = 1; i <= datablocknum; i++){
				if(blockbegin + i < POINTERS_PER_INODE){
					if(inode.direct[blockbegin + i] == 0){
						block.inode[inodenum].size += ret;
						disk_write(blocknum, block.data);
						return ret;
					}
					disk_write(inode.direct[blockbegin + i], data+first_length + BLOCK_SIZE*(i-1));
				}else{
					int tempblocknum = indirect.pointers[blockbegin + i - POINTERS_PER_INODE];
					if(tempblocknum == 0){
						block.inode[inodenum].size += ret;
						disk_write(blocknum, block.data);
						return ret;
					}
					disk_write(tempblocknum, data + first_length + BLOCK_SIZE*(i-1));
				}
				ret += BLOCK_SIZE;
			}
			
			if(last != 0){
				union fs_block tempblock;
				
				if(blockbegin + datablocknum < POINTERS_PER_INODE){
					if(inode.direct[blockbegin + datablocknum] == 0){
						block.inode[inodenum].size += ret;
						disk_write(blocknum, block.data);
						return ret;
					}
					disk_read(inode.direct[blockbegin+datablocknum], tempblock.data);
					memcpy(tempblock.data, data + first_length + BLOCK_SIZE * datablocknum, last);
					disk_write(inode.direct[blockbegin + datablocknum], tempblock.data);
				}else{
					int tempblocknum = indirect.pointers[blockbegin + datablocknum - POINTERS_PER_INODE];
					if(tempblocknum == 0){
						block.inode[inodenum].size += ret;
						disk_write(blocknum, block.data);
						return ret;
					}
					memcpy(tempblock.data, data + first_length + BLOCK_SIZE * datablocknum, last);
					disk_write(tempblocknum, tempblock.data);
				}
				ret += last;
				
			}
			block.inode[inodenum].size += ret;
			disk_write(blocknum, block.data);

		}
	}
	return ret;
}
