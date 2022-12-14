/*==========================================================================================
** alist.cpp
** By Chris Winstead
   Based on original code by Radford Neal and David MacKay

** Description: 
   Defines struct and functions for processing "alist" files, which
   represent the locations of 1's in a large sparse binary matrix.
   The alist file is typically used to specify LDPC codes.

** Usage:
    ldpcsim <alist_fname> <stim_fname> <iterations> <clock_cycles> <vcd_fname>
==============================================================================================*/


#include "alist.h"
#include <stdio.h>
#include <fstream>
#include "r.h"
using namespace std;

alist_struct loadFile(const char * fileName)
{
  int i;

  #ifdef CPPSTYLE
  ifstream theFile(fileName,ios::in);
  //  FILE * theFile = fopen(fileName, "r");  
  alist_struct alist;
  
  theFile >> alist.N >> alist.M;
  theFile >> alist.biggest_num_n >> alist.biggest_num_m;

  //fscanf(theFile, "%d %d\n", &alist.N, &alist.M);
  //fscanf(theFile, "%d %d\n", &alist.biggest_num_n, &alist.biggest_num_m);

  alist.num_nlist = (int *) malloc((alist.N)*sizeof(int));
  alist.num_mlist = (int *) malloc((alist.M)*sizeof(int));

  for (int i=0; i<alist.N; i++)
    theFile >> alist.num_nlist[i];
  for (int j=0; j<alist.M; j++)
    theFile >> alist.num_mlist[j];

  //fread_ivector ( alist.num_nlist , 0 , alist.N-1 , theFile ) ; 
  //fread_ivector ( alist.num_mlist , 0 , alist.M-1  , theFile) ; 


  alist.nlist = (int **) malloc(alist.N*sizeof(int *));
  alist.mlist = (int **) malloc(alist.M*sizeof(int *));
  for (i=0; i<alist.N; i++)
    {
      alist.nlist[i] = (int *) malloc(alist.biggest_num_n*sizeof(int));
      for (int j=0; j<alist.num_nlist[i]; j++)
	theFile >> alist.nlist[i][j];
    } 
  for (i=0; i<alist.M; i++)
    {
      alist.mlist[i] = (int *) malloc(alist.biggest_num_m*sizeof(int));
      for (int j=0; j<alist.num_mlist[i]; j++)
	theFile >> alist.mlist[i][j];
    }
  /*

  fread_imatrix ( alist.nlist , 1 , alist.N , 1 , alist.biggest_num_n, theFile ) ; 
  fread_imatrix ( alist.mlist , 1 , alist.M , 1 , alist.biggest_num_m, theFile ) ; 

  */

  #else
  FILE * theFile = fopen(fileName, "r");
  alist_struct alist;

  fscanf(theFile, "%d %d\n", &alist.N, &alist.M);
  fscanf(theFile, "%d %d\n", &alist.biggest_num_n, &alist.biggest_num_m);

  alist.num_nlist = (int *) malloc((alist.N)*sizeof(int));
  alist.num_mlist = (int *) malloc((alist.M)*sizeof(int));
  fread_ivector ( alist.num_nlist , 0 , alist.N-1 , theFile ) ;
  fread_ivector ( alist.num_mlist , 0 , alist.M-1  , theFile) ;


  alist.nlist = (int **) malloc(alist.N*sizeof(int *));
  alist.mlist = (int **) malloc(alist.M*sizeof(int *));
  for (i=0; i<alist.N; i++)
    alist.nlist[i] = (int *) malloc(alist.biggest_num_n*sizeof(int));
  for (i=0; i<alist.M; i++)
    alist.mlist[i] = (int *) malloc(alist.biggest_num_m*sizeof(int));

  fread_imatrix ( alist.nlist , 1 , alist.N , 1 , alist.biggest_num_n, theFile ) ;
  fread_imatrix ( alist.mlist , 1 , alist.M , 1 , alist.biggest_num_m, theFile ) ;

  #endif
  return alist;
}


void printAlist(alist_struct alist)
{
  printf("%d %d\n", alist.N, alist.M);
  printf("%d %d\n", alist.biggest_num_n, alist.biggest_num_m);

  int i, j;

  for (i=0; i<alist.N; i++)
    printf("%d ", alist.num_nlist[i]);
  printf("\n");
  for (i=0; i<alist.M; i++)
    printf("%d ", alist.num_mlist[i]);
  printf("\n");

  for (i=0; i<alist.N; i++)
    {
      for (j=0; j<alist.biggest_num_n; j++)
	printf("%d ", alist.nlist[i][j]);
      printf("\n");
    }

}



void freeAlist(alist_struct alist)
{
  int i, j;
  for (i=0; i<alist.N; i++)
    free(alist.nlist[i]);
  for (i=0; i<alist.M; i++)
    free(alist.mlist[i]);

  free(alist.num_mlist);
  free(alist.num_nlist);
}
