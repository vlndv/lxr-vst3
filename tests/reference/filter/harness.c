#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ResonantFilter.h"
float fastTan(float x); float tanhXdX(float x); float softClipTwo(float in);
static void mkin(int16_t*b){for(int i=0;i<32;i++)b[i]=(int16_t)(((i*7919+13)%2001-1000)*20);}
typedef struct{const char*name;float fv,reso;uint8_t drv;}Cfg;
int main(void){
  printf("fastTan(0.5)=%.9g\nfastTan(1.0)=%.9g\ntanhXdX(1.0)=%.9g\nsoftClipTwo(1.0)=%.9g\nsoftClipTwo(-2.5)=%.9g\n",fastTan(0.5f),fastTan(1.0f),tanhXdX(1.0f),softClipTwo(1.0f),softClipTwo(-2.5f));
  ResonantFilter f; memset(&f,0,sizeof f);
  SVF_setDrive(&f,0);printf("drive(0)=%.9g\n",f.drive);SVF_setDrive(&f,64);printf("drive(64)=%.9g\n",f.drive);SVF_setDrive(&f,127);printf("drive(127)=%.9g\n",f.drive);
  SVF_setReso(&f,0.f);printf("q(reso0)=%.9g\n",f.q);SVF_setReso(&f,0.5f);printf("q(reso0.5)=%.9g\n",f.q);SVF_setReso(&f,0.95f);printf("q(reso0.95)=%.9g\n",f.q);
  SVF_directSetFilterValue(&f,0.5f);printf("direct(0.5): f=%.9g g=%.9g\n",f.f,f.g);
  SVF_directSetFilterValue(&f,1.0f);printf("direct(1.0): f=%.9g g=%.9g\n",f.f,f.g);
  SVF_init(&f);printf("init: f=%.9g g=%.9g q=%.9g drive=%.9g s1=%g s2=%g\n",f.f,f.g,f.q,f.drive,f.s1,f.s2);
  Cfg cfgs[]={{"C1",0.5f,0.5f,64},{"C2",0.9f,0.95f,127},{"C3",1.0f,0.5f,64}};
  for(int c=0;c<3;c++){
    for(int type=1;type<=8;type++){
      ResonantFilter fl; memset(&fl,0,sizeof fl); SVF_reset(&fl);
      SVF_directSetFilterValue(&fl,cfgs[c].fv);SVF_setReso(&fl,cfgs[c].reso);SVF_setDrive(&fl,cfgs[c].drv);
      int16_t in[32],b1[32],b2[32]; mkin(in); memcpy(b1,in,64);memcpy(b2,in,64);
      SVF_calcBlockZDF(&fl,type,b1,32); 
      float s1a=fl.s1,s2a=fl.s2,zia=fl.zi,aa=fl.a,ba=fl.b;
      SVF_calcBlockZDF(&fl,type,b2,32);
      long sum=0;for(int i=0;i<32;i++)sum+=b2[i];
      printf("%s type%d blk2 out[0]=%d out[1]=%d out[15]=%d out[31]=%d sum=%ld | blk1 out[0]=%d | unchanged=%d\n",cfgs[c].name,type,b2[0],b2[1],b2[15],b2[31],sum,b1[0],(int)(memcmp(b2,in,64)==0));
      if(type==1||type==7) printf("%s type%d state after blk1: s1=%.9g s2=%.9g zi=%.9g a=%.9g b=%.9g | after blk2: s1=%.9g s2=%.9g zi=%.9g a=%.9g b=%.9g\n",cfgs[c].name,type,s1a,s2a,zia,aa,ba,fl.s1,fl.s2,fl.zi,fl.a,fl.b);
      if(type==8) printf("%s type8 state after blk2: s1=%.9g s2=%.9g zi=%.9g\n",cfgs[c].name,fl.s1,fl.s2,fl.zi);
    }
  }
  {int ok=1;for(int type=1;type<=8;type++){ResonantFilter fl;memset(&fl,0,sizeof fl);SVF_reset(&fl);SVF_directSetFilterValue(&fl,0.5f);SVF_setReso(&fl,0.5f);SVF_setDrive(&fl,64);int16_t z[32];memset(z,0,64);SVF_calcBlockZDF(&fl,type,z,32);SVF_calcBlockZDF(&fl,type,z,32);for(int i=0;i<32;i++)if(z[i])ok=0;}printf("zero_input_all_zero=%d\n",ok);}
  return 0;}
