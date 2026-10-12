#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static unsigned char* base;
static int hook_init(unsigned flags) {
 fprintf(stderr,"[VEXA][DIAG] SDL_Init begin flags=%x\n",flags);
 int r=((int(*)(unsigned))(base+0x1883ad6))(flags);
 fprintf(stderr,"[VEXA][DIAG] SDL_Init end flags=%x result=%d error=%s video=%p\n",flags,r,((char*(*)(void))(base+0x1884466))(),*(void**)(base+0x1fb5138));
 return r;
}
int main(int argc,char**argv) {
 void* p=dlopen("./UnityPlayer.so",RTLD_NOW|RTLD_GLOBAL);
 if(!p){fprintf(stderr,"%s\n",dlerror());return 1;}
 int(*entry)(int,char**)=dlsym(p,"_Z10PlayerMainiPPc");
 Dl_info di; dladdr(entry,&di); base=di.dli_fbase;
 const unsigned char id[]={0xb4,0x4d,0x37,0x66,0x01,0xaf,0x15,0xf4,0x48,0x66,0xad,0x62,0x23,0x20,0xc9,0xa8,0x4d,0xfe,0xa7,0x87};
 if(memcmp(base+0x2b8,id,20))return 3;
 unsigned char* thunk=MAP_FAILED;
 for(int i=1;i<100&&thunk==MAP_FAILED;i++)thunk=mmap(base-i*0x10000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|0x100000,-1,0);
 if(thunk==MAP_FAILED)return 4;
 thunk[0]=0x48;thunk[1]=0xb8;*(uintptr_t*)(thunk+2)=(uintptr_t)hook_init;thunk[10]=0xff;thunk[11]=0xe0;
 mprotect(thunk,4096,PROT_READ|PROT_EXEC);
 unsigned char* site=base+0x1883e01;
 mprotect((void*)((uintptr_t)site&-4096UL),4096,PROT_READ|PROT_WRITE|PROT_EXEC);
 site[0]=0xe9;*(int32_t*)(site+1)=thunk-(site+5);
 mprotect((void*)((uintptr_t)site&-4096UL),4096,PROT_READ|PROT_EXEC);
 return entry(argc,argv);
}
