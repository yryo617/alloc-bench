#ifndef _GETPARAM_H_
#define _GETPARAM_H_

void initparam(string *argv, string *defv);
  
string getparam(string name);

int getiparam(string name);
long getlparam(string name);
bool getbparam(string name);
double getdparam(string name);
int scanbind(string bvec[], string name);
bool matchname(string bind, string name);
string extrvalue(string arg);

bool matchname(string bind, string name);


#endif