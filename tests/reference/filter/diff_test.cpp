#include "ResonantFilter.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
extern "C" {
typedef struct { float f,g,q,s1,s2,a,b,zi,drive; } CFilt;
void SVF_reset(CFilt*); void SVF_init(CFilt*); void SVF_setReso(CFilt*,float); void SVF_setDrive(CFilt*,uint8_t);
void SVF_directSetFilterValue(CFilt*,float); void SVF_recalcFreq(CFilt*); void SVF_calcBlockZDF(CFilt*,const uint8_t,int16_t*,const uint8_t);
}
static uint32_t rs=12345; static uint32_t rnd(){ rs=rs*1664525u+1013904223u; return rs>>8; }
int main(){
  static_assert(sizeof(lxr::ResonantFilter)==sizeof(CFilt),"layout");
  long bad=0, total=0;
  for(int it=0; it<20000; ++it){
    CFilt c; lxr::ResonantFilter q; memset(&c,0,sizeof c); memset(&q,0,sizeof q);
    SVF_reset(&c); q.reset();
    float fv=(rnd()%1001)/1000.f, reso=(rnd()%1001)/1000.f; uint8_t drv=rnd()%128;
    SVF_directSetFilterValue(&c,fv); q.directSetFilterValue(fv);
    SVF_setReso(&c,reso); q.setReso(reso); SVF_setDrive(&c,drv); q.setDrive(drv);
    if(it%7==0){ SVF_recalcFreq(&c); q.recalcFreq(); }
    uint8_t type=1+rnd()%9;               // 1..9 incl. invalid 9 and off 8
    for(int blk=0; blk<4; ++blk){
      uint8_t size=(blk==0)?32:(1+rnd()%40);
      int16_t a[64],b[64];
      int mode=rnd()%3;
      for(int i=0;i<size;++i){ int16_t v = mode==0? (int16_t)(rnd()%65536-32768) : mode==1? (int16_t)((rnd()%2)?32767:-32768) : (int16_t)(rnd()%2001-1000); a[i]=b[i]=v; }
      SVF_calcBlockZDF(&c,type,a,size); q.calcBlockZDF(type,b,size);
      ++total;
      if(memcmp(a,b,size*2)!=0 || memcmp(&c,&q,sizeof c)!=0){ if(bad<5) printf("MISMATCH it=%d blk=%d type=%d\n",it,blk,type); ++bad; }
    }
  }
  printf("differential: %ld blocks, %ld mismatches (bitwise on buffers and all state)\n",total,bad);
  return bad?1:0;
}
