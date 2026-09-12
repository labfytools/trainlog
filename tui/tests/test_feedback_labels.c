#include "trainlog/timeutil.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d\n",__LINE__); return 1; } } while (0)
static int label(const char *end,const char *at,bool exercise,const char *expected){char out[64];CHECK(trainlog_feedback_relative_label(end,at,exercise,out,sizeof(out))==TRAINLOG_STATUS_OK);CHECK(strcmp(out,expected)==0);CHECK(strstr(out,"H+-")==NULL);return 0;}
int main(void){const char *end="2026-09-01T10:00:00+02:00";
 CHECK(label(end,"2026-09-01T09:20:00+02:00",true,"Pendant la séance")==0);
 CHECK(label(end,"2026-09-01T09:59:59+02:00",true,"Pendant la séance")==0);
 CHECK(label(end,end,true,"H+0")==0);CHECK(label(end,"2026-09-01T18:00:00+02:00",true,"H+8")==0);
 CHECK(label(NULL,"2026-09-01T09:20:00+02:00",true,"Ressenti")==0);
 CHECK(label(NULL,"2026-09-01T18:00:00+02:00",false,"H+?")==0);
 CHECK(label(end,"2026-09-01T09:00:00+02:00",false,"H+?")==0);return 0;}
