
/*
 *
 * Operating System Design / Diseño de Sistemas Operativos
 * (c) ARCOS.INF.UC3M.ES
 *
 * @file 	auxiliary.h
 * @brief 	Headers for the auxiliary functions required by filesystem.c.
 * @date	Last revision 01/04/2020
 *
 */

int syncFS(void);
int ialloc(void);
int balloc(void);
int namei(char *fileName);
int ifree(int i);
int bfree(int i);