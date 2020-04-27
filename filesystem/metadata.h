
/*
 *
 * Operating System Design / Diseño de Sistemas Operativos
 * (c) ARCOS.INF.UC3M.ES
 *
 * @file 	metadata.h
 * @brief 	Definition of the structures and data types of the file system.
 * @date	Last revision 01/04/2020
 *
 */

#define bitmap_getbit(bitmap_, i_) (bitmap_[i_ >> 3] & (1 << (i_ & 0x07)))
static inline void bitmap_setbit(char *bitmap_, int i_, int val_) {
  if (val_)
    bitmap_[(i_ >> 3)] |= (1 << (i_ & 0x07));
  else
    bitmap_[(i_ >> 3)] &= ~(1 << (i_ & 0x07));
}

#define MAGIC_NUMBER 12345
#define MAX_N_INODES 48
#define BLOCK_SIZE 2048
#define MAX_SIZE_FILE 2048
#define MAX_NAME_LENGTH 32
#define MIN_SIZE_SYS_FILES 460 * 1024
#define MAX_SIZE_SYS_FILES 600 * 1024

typedef struct{

  unsigned int magic_number;
  unsigned int num_Blocks_Map_Inodes;
  unsigned int num_Blocks_Map_Data;
  unsigned int num_inodes;
  unsigned int first;
  unsigned int size;
  unsigned int num_Blocks_Data;
  unsigned int num_Blocks;

}sb;

typedef struct{

  unsigned int size;
  unsigned int block;
  int state;
  int pos;
  char name[MAX_NAME_LENGTH];

}inode;
