#ifndef _GRAV_H_
#define _GRAV_H_
void hackgrav(bodyptr p,unsigned ProcessId);
// void gravsub(register nodeptr p, unsigned ProcessId, int level);
void gravsub(register nodeptr p, unsigned ProcessId);
void walksub(nodeptr n, real dsq, unsigned ProcessId);
void hackwalk(proced sub, unsigned ProcessId);
bool subdivp(register nodeptr p, real dsq, unsigned ProcessId);

#endif