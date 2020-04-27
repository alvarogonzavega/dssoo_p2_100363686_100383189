
/*
 *
 * Operating System Design / Diseño de Sistemas Operativos
 * (c) ARCOS.INF.UC3M.ES
 *
 * @file 	filesystem.c
 * @brief 	Implementation of the core file system funcionalities and auxiliary functions.
 * @date	Last revision 01/04/2020
 *
 */


#include "filesystem/filesystem.h" // Headers for the core functionality
#include "filesystem/auxiliary.h"  // Headers for auxiliary functions
#include "filesystem/metadata.h"   // Type and structure declaration of the file system
#include <stdlib.h>
#include <string.h>

char i_map[MAX_N_INODES]; //Inode map
char b_map[MAX_SIZE_SYS_FILES / BLOCK_SIZE]; //Block map
sb sbk[1]; //Superblock
inode *inodo; //Inode structure
char *disk = "disk.dat";

/*
 * @brief 	Generates the proper file system structure in a storage device, as designed by the student.
 * @return 	0 if success, -1 otherwise.
 */
int mkFS(long deviceSize)
{

	//We see if the size is between the allowed values
	if(deviceSize > MAX_SIZE_SYS_FILES || deviceSize < MIN_SIZE_SYS_FILES) return -1;
	//We stablish the magic number
	sbk[0].magic_number = MAGIC_NUMBER;
	int check = MAX_N_INODES * sizeof(inode) / BLOCK_SIZE;
	//If the number of blocks of inodes is less than 1, we put it to 1
	if(check < 1) sbk[0].num_Blocks_Map_Inodes = 1;
	//If is bigger than allowed we have an error
	else if(check > MAX_N_INODES) return -1;
	else sbk[0].num_Blocks_Map_Inodes = check;
	//We stablish the number of blocks
	sbk[0].num_Blocks = deviceSize / BLOCK_SIZE;
	check = sbk[0].num_Blocks * sizeof(int) / BLOCK_SIZE;
	//If the number of blocks of data is less than 1, we put it to 1
	if(check < 1) sbk[0].num_Blocks_Map_Data = 1;
	//If is bigger we have an error
	else if(check > MAX_N_INODES) return -1;
	else sbk[0].num_Blocks_Map_Data = check;
	//We stablish the number of inodes, the first inode (0), the size and the number of blocks of data
	sbk[0].num_inodes = MAX_N_INODES;
	sbk[0].first = 0;
	sbk[0].size = deviceSize;
	sbk[0].num_Blocks_Data = sbk[0].num_Blocks - sbk[0].num_Blocks_Map_Data - sbk[0].num_Blocks_Map_Inodes;
	//We need allocate the memory with malloc
	inodo = (inode *) malloc(sbk[0].num_inodes*sizeof(inode));
	//With bitmap_setbit we can inilizate the maps
	for(int i=0; i<sbk[0].num_inodes; i++){ bitmap_setbit(i_map, i, 0); }
	for(int i=0; i<sbk[0].num_Blocks_Data; i++){ bitmap_setbit(b_map, i, 0); }
	//We write in the disk
	if(syncFS() != 0) return -1;
	return 0;

}

/*
 * @brief 	Mounts a file system in the simulated device.
 * @return 	0 if success, -1 otherwise.
 */
int mountFS(void)
{
	//We set the bytes to 0 in the inode structure
	for(int i=0; i<sbk[0].num_inodes; i++){ memset(&(inodo[i]), 0, sizeof(inode)); }
	//We read the superblock
	if(bread(disk, 1, (char*)&(sbk[0])) != 0) return -1;
	//Now the same with the maps of blocks (inodes and data)
	for(int i=0; i<sbk[0].num_Blocks_Map_Inodes; i++){

		if(bread(disk, 2+i, (char*)&(i_map[i])) != 0) return -1;

	}

	for(int i=0; i<sbk[0].num_Blocks_Map_Data; i++){

		if(bread(disk, 2+i+sbk[0].num_Blocks_Map_Inodes, (char*)&(b_map[i])) != 0) return -1;

	}

	//Now we read the inodes 
	for(int i=0; i<sbk[0].num_Blocks_Map_Inodes; i++){

		if(bread(disk, i+sbk[0].first, (char*)&(inodo[i])) != 0) return -1;

	}

	return 0;
	
}

/*
 * @brief 	Unmounts the file system from the simulated device.
 * @return 	0 if success, -1 otherwise.
 */
int unmountFS(void)
{ 
	//We check if there is any inode open
	for(int i=0; i<sbk[0].num_inodes; i++){
	
		if(inodo[i].state !=0) return -1;

	}

	//We write to the disk
	if(syncFS() != 0) return -1;
	return 0;

}

/*
 * @brief	Creates a new file, provided it it doesn't exist in the file system.
 * @return	0 if success, -1 if the file already exists, -2 in case of error.
 */
int createFile(char *fileName)
{
	
	//We check the length of the file
	if(strlen(fileName)> MAX_NAME_LENGTH) return -2;
	//We check if we have reached the limit of inodes
	int c=0;
	for(int i=0; i<MAX_N_INODES; i++){
		 if(strcmp(inodo[i].name, "")){
			  
			  c++; 
			  //if we already have that file we return -1
			  if(strcmp(inodo[i].name, fileName) == 0) return -1;

			}
		
		}

		//If we have more than 48 elements we can't add another one
		if(c>MAX_N_INODES) return -2;
		int bid = balloc();
		int inodeid = ialloc();
		//If there is no free inodes or blocks
		if(bid == -1 || inodeid == -1) return -2;
		//We add the information
		inodo[inodeid].size = 0;
		inodo[inodeid].block = bid;
		strcpy(inodo[inodeid].name, fileName);
		return 0;

}

/*
 * @brief	Deletes a file, provided it exists in the file system.
 * @return	0 if success, -1 if the file does not exist, -2 in case of error..
 */
int removeFile(char *fileName)
{

	//We check if the file exists
	int i = namei(fileName);
	if(i==-1) return -1;
	//We check if the inode is open
	if(inodo[i].state != 0) return -2;
	//We free the block and the inode
	if(bfree(i)!=0 || ifree(i)!=0) return -2;
	return 0;

}

/*
 * @brief	Opens an existing file.
 * @return	The file descriptor if possible, -1 if file does not exist, -2 in case of error..
 */
int openFile(char *fileName)
{

	//We check if the file exists
	int i=namei(fileName);
	if(i==-1) return -1;
	//We check if it's already open
	if(inodo[i].state!=0) return -2;
	inodo[i].state = 1; //We change the status to open
	return i;

}

/*
 * @brief	Closes a file.
 * @return	0 if success, -1 otherwise.
 */
int closeFile(int fileDescriptor)
{
	//We check if the descriptor is valid
	if(fileDescriptor <0 || fileDescriptor > MAX_N_INODES) return -1;
	//We close the file
	inodo[fileDescriptor].state = 0;
	return 0;

}

/*
 * @brief	Reads a number of bytes from a file and stores them in a buffer.
 * @return	Number of bytes properly read, -1 in case of error.
 */
int readFile(int fileDescriptor, void *buffer, int numBytes)
{

	//We check if the descriptor is valid
	if(fileDescriptor <0 || fileDescriptor > MAX_N_INODES) return -1;
	//We need to check if the file is open
	if(inodo[fileDescriptor].state == 0) return -1;
	char rbf[BLOCK_SIZE]; //Char were we will put the buffer
	int start = inodo[fileDescriptor].pos;
	int end = start + numBytes - 1;
	//If the starting point is at the end or there is no bytes to read, we return 0
	if(start > inodo[fileDescriptor].size || numBytes == 0) return 0;
	//If the buffer wants to read over the size of the file we need to put the end to size
	if(end>inodo[fileDescriptor].size) end = inodo[fileDescriptor].size-1;
	//Total of bytes to read
	int total = end - start;
	bread(disk, fileDescriptor, rbf); //We read the total bytes specified
	memmove(buffer, rbf+inodo[fileDescriptor].pos, total); //We move the pointer
	inodo[fileDescriptor].pos += end; //We stablish the new position
	return total;

}

/*
 * @brief	Writes a number of bytes from a buffer and into a file.
 * @return	Number of bytes properly written, -1 in case of error.
 */
int writeFile(int fileDescriptor, void *buffer, int numBytes)
{

	//We check if the descriptor is valid
	if(fileDescriptor <0 || fileDescriptor > MAX_N_INODES) return -1;
	//We need to check if the file is open
	if(inodo[fileDescriptor].state == 0) return -1;
	char wbf[BLOCK_SIZE]; //Char were we will put the buffer
	int start = inodo[fileDescriptor].pos;
	int end = start + numBytes - 1;
	//If the starting point is over the maximum file size or there is no bytes to write, we return 0
	if(start > MAX_SIZE_FILE || numBytes == 0) return 0;
	//If the buffer wants to write over the maximum size of the file we need to put a limit
	if(end>MAX_SIZE_FILE) end = MAX_SIZE_FILE-1;
	//Total bytes to write
	int total = end - start;
	bread(disk, fileDescriptor, wbf); //We read the total bytes specified
	memmove(buffer, wbf+inodo[fileDescriptor].pos, total); //We move the pointer
	bwrite(disk, fileDescriptor, wbf); //We write on the file
	inodo[fileDescriptor].size += total; //We update the size of the file
	inodo[fileDescriptor].pos += end; //We stablish the new position
	return total;

}

/*
 * @brief	Modifies the position of the seek pointer of a file.
 * @return	0 if succes, -1 otherwise.
 */
int lseekFile(int fileDescriptor, long offset, int whence)
{

	//We need to see if the offset is on an allowed range
	if(offset > MAX_SIZE_FILE || offset < 0) return -1;
	//The inode must be open
	if(inodo[fileDescriptor].state==0) return -1;
	switch(whence){
	
		case FS_SEEK_CUR: //We add the offset to the actual position

			//Case where the offset and the actual position is greater than the maximum size allowed
			if(inodo[fileDescriptor].pos + offset > MAX_SIZE_FILE) return -1;
			inodo[fileDescriptor].pos += offset;
			break;

		case FS_SEEK_END: //We put the pointer to the end

			inodo[fileDescriptor].pos = inodo[fileDescriptor].size;
			break;

		case FS_SEEK_BEGIN: //We put the pointer to the start

			inodo[fileDescriptor].pos = 0;
			break;

	}

	return 0;

}

/*
 * @brief	Checks the integrity of the file.
 * @return	0 if success, -1 if the file is corrupted, -2 in case of error.
 */

int checkFile (char * fileName)
{
    return -2;
}

/*
 * @brief	Include integrity on a file.
 * @return	0 if success, -1 if the file does not exists, -2 in case of error.
 */

int includeIntegrity (char * fileName)
{
    return -2;
}

/*
 * @brief	Opens an existing file and checks its integrity
 * @return	The file descriptor if possible, -1 if file does not exist, -2 if the file is corrupted, -3 in case of error
 */
int openFileIntegrity(char *fileName)
{

    return -2;
}

/*
 * @brief	Closes a file and updates its integrity.
 * @return	0 if success, -1 otherwise.
 */
int closeFileIntegrity(int fileDescriptor)
{
    return -1;
}

/*
 * @brief	Creates a symbolic link to an existing file in the file system.
 * @return	0 if success, -1 if file does not exist, -2 in case of error.
 */
int createLn(char *fileName, char *linkName)
{
    return -1;
}

/*
 * @brief 	Deletes an existing symbolic link
 * @return 	0 if the file is correct, -1 if the symbolic link does not exist, -2 in case of error.
 */
int removeLn(char *linkName)
{
    return -2;
}

/*
 * @brief 	Writes data on disk
 * @return 	0 if it's written correctly, -1 if there is case of error
 */
int syncFS(void){

	//We write the superblock
	if(bwrite(disk, 1, (char *)&(sbk[0])) != 0) return -1;
	//Now the same for blocks of maps (data and inodes)
	int k=0;
	for(int i=0; i<sbk[0].num_Blocks_Map_Inodes; i++){

		if(bwrite(disk, 2+k, (char *)&(i_map[i])) != 0) return -1;
		k++;

	}

	for(int i=0; i<sbk[0].num_Blocks_Map_Data; i++){

		if(bwrite(disk, 2+k, (char *)&(b_map[i])) != 0) return -1;
		k++;

	}

	//Now we write the inodes into the disk
	for(int i=0; i<sbk[0].num_Blocks_Map_Inodes; i++){

		if(bwrite(disk, 1+sbk[0].first, (char *)&(inodo[i])) != 0) return -1;

	} 

	return 0;

}

/*
 * @brief 	Search for a free inode
 * @return 	i if we found a free inode, -1 if there isn't free inodes
 */
int ialloc(void){

	for(int i=0; i<MAX_N_INODES; i++){

		//We search for a free inode
		if(bitmap_getbit(i_map, i)==0){

			//Inode changes his status to OCUPIED
			bitmap_setbit(i_map, i, 1);
			memset(&(inodo[i]), 0, sizeof(inode));
			return i;

		}


	}

	return -1;

}

/*
 * @brief 	Search for a free blocks
 * @return 	i if we found a free block, -1 if there isn't free blocks
 */
int balloc(void){

	char bk[BLOCK_SIZE];
	for(int i=0; i<sbk[0].num_Blocks_Data; i++){

		//We search for a free block
		if(bitmap_getbit(b_map, i)==0){

			//Block changes his status to OCUPIED
			bitmap_setbit(b_map, i, 1);
			memset(&(bk[i]), 0, BLOCK_SIZE);
			return i;

		}

	}

	return -1;

}

/*
 * @brief	Searches for a inode with the fileName provided
 * @return	i if we find the inode, -1 in case there is no file with that name
 */
int namei(char *fileName)
{

	for(int i=0; i<MAX_N_INODES; i++){

		if(strcmp(inodo[i].name, fileName)) return i;

	}

	return -1;

}

/*
 * @brief	frees an inode
 * @return	0 if it works, -1 in case of error
 */
int ifree(int i){

	if(i<0 || i>MAX_N_INODES) return -1;
	memset(&(inodo[i]), 0, sizeof(inode));
	bitmap_setbit(i_map, i, 0);
	return 0;


}

/*
 * @brief	frees a block
 * @return	0 if it works, -1 in case of error
 */
int bfree(int i){

	if(i<0 || i>MAX_N_INODES) return -1;
	bitmap_setbit(b_map, i, 0);
	return 0;


}
