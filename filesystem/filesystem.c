
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
#include <stdio.h>

char i_map[MAX_N_INODES]; //Inode map
char b_map[MAX_SIZE_SYS_FILES / BLOCK_SIZE]; //Block map
sb sbk[1]; //Superblock
inode inodo[MAX_N_INODES]; //Inode structure
char *disk = "disk.dat";

/*
 * @brief 	Generates the proper file system structure in a storage device, as designed by the student.
 * @return 	0 if success, -1 otherwise.
 */
int mkFS(long deviceSize)
{

	//We see if the size is between the allowed values
	if(deviceSize > MAX_SIZE_SYS_FILES || deviceSize < MIN_SIZE_SYS_FILES) return -1;
	//We stablish the number of blocks, inodes, the size and the number of blocks of data
	sbk[0].num_Blocks = deviceSize / BLOCK_SIZE;
	sbk[0].num_inodes = MAX_N_INODES;
	sbk[0].size = deviceSize;
	sbk[0].num_Blocks_Data = sbk[0].num_Blocks - 3;
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
	//Security copy
	char buffer[BLOCK_SIZE];
	memcpy(buffer, &(sbk[0]), sizeof(sbk[0]));
	if(bread(disk, 1, (char *)&(sbk[0])) != 0) return -1;
	//Now the same with the maps of blocks (inodes and data)
	if(bread(disk, 1, (char*)&(i_map)) != 0) return -1;
	if(bread(disk, 1, (char*)&(b_map)) != 0) return -1;
	int k=0;
	//Now we read the inodes
	for(int i=0; i<MAX_N_INODES; i++){

		if(i>31) k++;
		if(bread(disk, 2+k, (char*)&(inodo[i])) != 0) return -1;

	}

	for(int i=0; i<MAX_N_INODES; i++){ inodo[i].state = 0; }
	//We put the copy back
	memcpy(&(sbk[0]), buffer, sizeof(sbk[0]));
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
	//We check if we have the same file
	if(namei(fileName)!=-1) return -1;
	int bid = balloc();
	int inodeid = ialloc();
	//If there is no free inodes or blocks
	if(bid == -1 || inodeid == -1) return -2;
	//We add the information
	inodo[inodeid].size = 0;
	inodo[inodeid].block[0] = bid;
	inodo[inodeid].pos = 0;
	inodo[inodeid].hasIntegrity = 0;
	inodo[inodeid].crc = 0;
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
	int blockActual=inodo[fileDescriptor].pos/BLOCK_SIZE;
	int start = inodo[fileDescriptor].pos;
	int end = start + numBytes;
	int blockF = end/BLOCK_SIZE; //The final block of lecture
	while(blockF>5){blockF--;}
	int total=0;
	if(blockActual == blockF){ //Only one block to read

		//If the starting point is at the end or there is no bytes to read, we return 0
		if(start == inodo[fileDescriptor].size || numBytes == 0) return 0;
		//If the buffer wants to read over the size of the file we need to put the end to size
		if(end>inodo[fileDescriptor].size) end = inodo[fileDescriptor].size;
		//Total of bytes to read
		total = end - start;
		bread(disk, inodo[fileDescriptor].block[blockActual], rbf); //We read the total bytes specified
		memmove(buffer, rbf+inodo[fileDescriptor].pos, total); //We move the pointer
		inodo[fileDescriptor].pos += end; //We stablish the new position
		return total;

	}else{

		//More than one block to read
		int p=end-start;
		while(p>BLOCK_SIZE){ p -= BLOCK_SIZE; } //We reduce until we reach
		for(int i=blockActual; i<=blockF; i++){

			if(i==blockF){

				bread(disk, inodo[fileDescriptor].block[i], rbf);
				memmove(buffer+total, rbf, p);
				total += p;

			}else{

				bread(disk, inodo[fileDescriptor].block[i], rbf);
				memmove(buffer+total, rbf, BLOCK_SIZE);
				total += BLOCK_SIZE;

			}




		}

	  inodo[fileDescriptor].pos += end;
		return total;


	}

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
	int end = start + numBytes;
		//If the buffer wants to write over the maximum size of the file we need to put a limit
	if(end>MAX_SIZE_FILE) end = MAX_SIZE_FILE;
	int blockI = start/BLOCK_SIZE;
	int blockF = end/BLOCK_SIZE;
	while(blockF>5){blockF--;}
	//If the starting point is over the maximum file size or there is no bytes to write, we return 0
	if(start > MAX_SIZE_FILE || numBytes == 0) return 0;
	//Total bytes to write
	if(blockI == blockF){

		int total = end - start;
		bread(disk, inodo[fileDescriptor].block[blockI], wbf); //We read the total bytes specified
		memmove(buffer, wbf+inodo[fileDescriptor].pos, total); //We move the pointer
		bwrite(disk, fileDescriptor, wbf); //We write on the file
		inodo[fileDescriptor].size += total; //We update the size of the file
		inodo[fileDescriptor].pos += end; //We stablish the new position
		return total;

	}else{

		int total=0;
		int p=end-start; while(p>BLOCK_SIZE){ p-=BLOCK_SIZE; }
		for(int i=blockI; i<=blockF; i++){

			if(i>blockI){

				int b=balloc();
				if(b==-1) return -1;
				inodo[fileDescriptor].block[i]= b;

			}

			if(i==blockF){

				bread(disk, inodo[fileDescriptor].block[i], wbf); //We read the total bytes specified
				memmove(buffer+total, wbf+inodo[fileDescriptor].pos, p); //We move the pointer
				bwrite(disk, inodo[fileDescriptor].block[i], wbf);
				total += p;

			}else{

				bread(disk, inodo[fileDescriptor].block[i], wbf); //We read the total bytes specified
				memmove(buffer+total, wbf+inodo[fileDescriptor].pos, BLOCK_SIZE); //We move the pointer
				bwrite(disk, inodo[fileDescriptor].block[i], wbf);
				total += BLOCK_SIZE;


			}


		}

		inodo[fileDescriptor].size += total; //We update the size of the file
		inodo[fileDescriptor].pos += end; //We stablish the new position
		return total;

	}

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
	// TODO if file is open and modified, it will always detect corruption.
	// TODO check first if the file has integrity
	// To get the contents of the file, we choosed to use readFile

	// If the file isnt opened, it opens it and closes it at the end
	int of_result;
	of_result = openFile(fileName);
	int file_inode_n = namei(fileName);

	if (of_result == -1 || namei == -1) return -2; // File doesnt exist

	// Put the position on the start of the file and then put it back
	inode* file_inode = &inodo[file_inode_n];
	unsigned int orig_inode_pos = file_inode->pos;
	unsigned int file_length = file_inode->size;
	file_inode->pos = 0;

	// Read the entire file
	unsigned char* file_data;
	readFile(file_inode_n, file_data, file_length);

	// Calculate and compare the CRC
	uint32_t new_crc = CRC32(file_data, file_length);
	uint32_t stored_crc = file_inode->crc;

	int result = 0;
	if ( new_crc != stored_crc ) result = -1;	// File is corrupted

	// Put back the original inode position
	file_inode->pos = orig_inode_pos;

	if (of_result >= 0) closeFile(of_result); // If file wasnt originaly open, it closes it again

    return result;
}

/*
 * @brief	Include integrity on a file.
 * @return	0 if success, -1 if the file does not exists, -2 in case of error.
 */

int includeIntegrity (char * fileName)
{
	// To get the contents of the file, we choosed to use readFile

	// If the file isnt opened, it opens it and closes it at the end
	int of_result;
	of_result = openFile(fileName);
	int file_inode_n = namei(fileName);

	if (of_result == -1 || namei == -1) return -1; // File doesnt exist

	// Put the position on the start of the file and then put it back
	inode* file_inode = &inodo[file_inode_n];
	unsigned int orig_inode_pos = file_inode->pos;
	unsigned int file_length = file_inode->size;
	file_inode->pos = 0;

	// Read the entire file
	unsigned char* file_data;
	readFile(file_inode_n, file_data, file_length);

	// Calculate and store the CRC
	uint32_t new_crc = CRC32(file_data, file_length);
	file_inode->crc = new_crc;
	file_inode->hasIntegrity = 1;

	// Put back the original inode position
	file_inode->pos = orig_inode_pos;

	if (of_result >= 0) closeFile(of_result); // If file wasnt originaly open, it closes it again

    return 0;
}

/*
 * @brief	Opens an existing file and checks its integrity
 * @return	The file descriptor if possible, -1 if file does not exist, -2 if the file is corrupted, -3 in case of error
 */
int openFileIntegrity(char *fileName)
{
	// The file must have integrity first
	if ( inodo[namei(fileName)].hasIntegrity == 0 ) return -3;

	int cf_result = checkFile(fileName);
	if ( cf_result == -1 ) return -2; // File is corrupted
	else if ( cf_result == -2 ) return -3; // Other check error

	int of_result = openFile(fileName);
	if ( of_result == -1 ) return -1; // File doesnt exist
	else if (of_result == -2 ) return -3; // Other open error (file is already open)
	
    return of_result;
}

/*
 * @brief	Closes a file and updates its integrity.
 * @return	0 if success, -1 otherwise.
 */
int closeFileIntegrity(int fileDescriptor)
{
	// The file must have integrity first
	if ( inodo[fileDescriptor].hasIntegrity == 0 ) return -1;

	if (includeIntegrity(inodo[fileDescriptor].name) != 0) return -1;
	if (closeFile(fileDescriptor) != 0) return -1;

    return 0;
}
/*
 * @brief	Creates a symbolic link to an existing file in the file system.
 * @return	0 if success, -1 if file does not exist, -2 in case of error.
 */
int createLn(char *fileName, char *linkName)
{
	// The symbolic links will be stored in a specific file named "symlinkFile.sys"
	
	// Check if the file exists first
	if ( namei(fileName) == -1 ) return -1;
    
	int of_result = openFile(SYMLINK_FILE);
	
	if (of_result == -2) ; // The file is probably open already (do nothing)
	else if (of_result == -1) // If SYMLINK file doesnt exist, create it
	{
		if ( createFile(SYMLINK_FILE) == -2 ) return -2;
		of_result = openFile(SYMLINK_FILE);
	}

	char* links_buffer[MAX_FILE_SIZE];
	if (readFile(of_result, links_buffer, MAX_FILE_SIZE) == -1) return -2;

	int end_file_pointer = strlen(links_buffer);

	// If it doesnt fit in the file, return error
	if ( (end_file_pointer + strlen(linkName) + 2) >= MAX_SIZE_FILE ) { 
		return -2;
	}

	// Concat the new symbolic link in the buffer (with specific format)
	strncat(links_buffer, (char*)28, 1);
	strncat(links_buffer, fileName, strlen(fileName));
	strncat(links_buffer, (char*)29, 1);
	strncat(links_buffer, linkName, strlen(linkName));
	
	// Write the buffer in the file and close it
	lseekFile(of_result, 0, FS_SEEK_BEGIN);
	if (writeFile(of_result, links_buffer, strlen(links_buffer)) == -1) return -2;

	closeFile(of_result);

	return 0;
}

/*
 * @brief 	Deletes an existing symbolic link
 * @return 	0 if the file is correct, -1 if the symbolic link does not exist, -2 in case of error.
 */
int removeLn(char *linkName)
{
	// The symbolic links are stored in a specific file named "symlinkFile.sys"

	int link_length = strlen(linkName);

	int of_result = openFile(SYMLINK_FILE);
	
	if (of_result == -2) ;
	else if (of_result == -1) // If SYMLINK file doesnt exist, the link doesnt exist
	{
		return -1;
	}

	char* links_buffer[MAX_FILE_SIZE];
	if (readFile(of_result, links_buffer, MAX_FILE_SIZE) == -1) return -2;

	int end_file_pointer = strlen(links_buffer);
	int buffer_pointer = 0;
	int pointer_to_entry = 1;
	int pointer_to_linkname = -1;
	int found = 0;
	
	// Search through the file to find the link
	while (buffer_pointer < end_file_pointer && !found)
	{
		// Find next character delimitator 29 (start of linkname)
		while(links_buffer[buffer_pointer] != 29 && buffer_pointer < end_file_pointer)
		{
			++buffer_pointer;
		}
		if (buffer_pointer == end_file_pointer) break;
		++buffer_pointer;
		pointer_to_linkname = buffer_pointer; // Point to the linkname


		// Match the name or go to the next delimitator character
		found = 1;
		for(int local=0; local < link_length && links_buffer[buffer_pointer] != 28 && found; local++)
		{
			if (links_buffer[buffer_pointer] != linkName[local]) found = 0;
			++buffer_pointer;
		}

		// If not found, continue until the next character delimitator 28 (start of entry) or end of file
		if (!found)
		{
			while(links_buffer[buffer_pointer] != 28 && buffer_pointer < end_file_pointer)
			{
				++buffer_pointer;
			}
			++buffer_pointer;
			pointer_to_entry = buffer_pointer;
		}
	}

	if (found == 0) return -1;

	// Cut the buffer
	int begin = pointer_to_entry-1;
	int len = (pointer_to_linkname - (pointer_to_entry-1)) + link_length;
	int l = strlen(links_buffer);

	if (begin + len > l) len = l - begin;
	memmove(links_buffer + begin, links_buffer + begin + len, l - len + 1);


	// Write the buffer in the file and close it
	lseekFile(of_result, 0, FS_SEEK_BEGIN);
	if (writeFile(of_result, links_buffer, strlen(links_buffer)) == -1) return -2;

	closeFile(of_result);

	return 0;
}

/*
 * @brief 	Writes data on disk
 * @return 	0 if it's written correctly, -1 if there is case of error
 */
int syncFS(void){

	char a[BLOCK_SIZE];
	char b[BLOCK_SIZE];
	char c[BLOCK_SIZE];
	//We write the superblock
	char buffer[BLOCK_SIZE];
	memcpy(buffer, &(sbk[0]), sizeof(sbk[0]));
	memmove(&(sbk[0]), a, sizeof(sb));
	memcpy(&(sbk[0]), buffer, sizeof(sbk[0]));
	memmove(&(i_map), a, sizeof(sb));
	memmove(&(b_map), a, sizeof(sb));
	if(bwrite(disk, 1, a) != 0) return -1;
	//Now the same for blocks of maps (data and inodes)
	memmove(&(inodo[0]), b, 31*sizeof(inode));
	memmove(&(inodo[31]), c, 17*sizeof(inode));
	//Now we write the inodes into the disk
	if(bwrite(disk, 2, b)) return -1;
	if(bwrite(disk, 3, c)) return -1;
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

		if(strncmp(inodo[i].name, fileName, strlen(fileName)) == 0) return i;

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

for(int j=0; j<5; j++){

	if(inodo[i].block[j]>sbk[0].num_Blocks_Data) return -1;
		bitmap_setbit(b_map, inodo[i].block[j], 0);

	}

	return 0;

}
