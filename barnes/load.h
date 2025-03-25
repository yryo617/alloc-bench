#ifndef _LOAD_H_
#define _LOAD_H_
void maketree(unsigned ProcessId);
cellptr InitCell(cellptr parent, unsigned ProcessId);
leafptr InitLeaf(cellptr parent, unsigned ProcessId);
void printtree (nodeptr n);
nodeptr loadtree(bodyptr p, cellptr root, unsigned ProcessId);
int subindex(int x[NDIM], int l);
void hackcofm(int nc, unsigned ProcessId);
bool intcoord(int xp[NDIM], vector rp);
cellptr SubdivideLeaf \
  (leafptr le, cellptr parent, unsigned int l, unsigned int ProcessId);
cellptr makecell(unsigned ProcessId);
cellptr makecell(unsigned ProcessId);

#endif