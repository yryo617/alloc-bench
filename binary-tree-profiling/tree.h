// tree code from
// CLBG at
// https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/binarytrees-gcc-1.html

typedef struct tn {
    struct tn*    left;
    struct tn*    right;
} treeNode;


treeNode* NewTreeNode(treeNode *, treeNode * );
long ItemCheck(treeNode *);
treeNode* BottomUpTree(unsigned);
void DeleteTree(treeNode *);
