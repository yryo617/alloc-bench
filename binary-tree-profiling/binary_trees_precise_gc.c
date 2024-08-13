
#include <stdio.h>
#include <stdlib.h>

#ifdef MUTATOR_GC
#include <gc.h>
#define MYMALLOC(_sz) GC_MALLOC(_sz)
#define FREE(_ptr) 
#else 
#define MYMALLOC(_sz) malloc(_sz)
#define FREE(_ptr) free(_ptr)
#endif

#include "tree.h"

#define SEED 0x42
#define MAXSIZE 20
#define NUM_ALLOCS 2000



long int savebuffer[NUM_ALLOCS];

int main() {
  int i, size, *buffer;
  int check;
  treeNode *tempTree;

  check = 0;
  srand(SEED);

  for (i=0; i<NUM_ALLOCS; i++) {
    size = rand() % MAXSIZE;
    
    // printf("alloc buffer size %d\n", size);
    // buffer = (int *)MYMALLOC(size * sizeof(int));
    tempTree = BottomUpTree(size);
    check += ItemCheck(tempTree);

    /////
    // confuse conservative collector?
    savebuffer[i] = (long int)tempTree;
    printf("save %x at index %d , i = %d\n",
           (long int)buffer,
    	   i % (NUM_ALLOCS>>5),i); 
  }

  printf("check: %d, rand: %lld\n",
	 check,
	 savebuffer[rand()%NUM_ALLOCS]);
  return 0;
  
}
// tree code from
// CLBG at
// https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/binarytrees-gcc-1.html
treeNode* NewTreeNode(treeNode* left, treeNode* right)
{
    treeNode*    new;

    new = (treeNode*)MYMALLOC(sizeof(treeNode));

    new->left = left;
    new->right = right;

    return new;
} /* NewTreeNode() */


long ItemCheck(treeNode* tree)
{
    if (tree->left == NULL)
        return 1;
    else
        return 1 + ItemCheck(tree->left) + ItemCheck(tree->right);
} /* ItemCheck() */


treeNode* BottomUpTree(unsigned depth)
{
    if (depth > 0)
        return NewTreeNode
        (
            BottomUpTree(depth - 1),
            BottomUpTree(depth - 1)
        );
    else
        return NewTreeNode(NULL, NULL);
} /* BottomUpTree() */


void DeleteTree(treeNode* tree)
{
    if (tree->left != NULL)
    {
        DeleteTree(tree->left);
        DeleteTree(tree->right);
    }

    FREE(tree);
} /* DeleteTree() */
