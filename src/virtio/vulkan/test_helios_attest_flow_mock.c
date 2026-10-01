#include "vn_renderer_helios_attest_transport.h"
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
typedef int32_t NTSTATUS;typedef int64_t LONG64;typedef void* HANDLE;typedef void* HMODULE;typedef uint32_t ULONG;typedef unsigned char* PUCHAR;
#define WINAPI
#define DECLSPEC_ALIGN(x) _Alignas(x)
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x800
static LONG64 InterlockedIncrement64(volatile LONG64*p){return __atomic_add_fetch(p,1,__ATOMIC_SEQ_CST);}
static uint32_t GetCurrentProcessId(void){return 42;}
struct helios {int device;};
static int mode,calls;static atomic_uint entropy;static uint8_t last_id[16];
static NTSTATUS random_mock(void* alg,PUCHAR out,ULONG n,ULONG flags){assert(!alg&&n==16&&flags==2);if(mode==9)return -1;memset(out,0,n);unsigned v=atomic_fetch_add(&entropy,1)+1;memcpy(out,&v,4);return 0;}
static HMODULE LoadLibraryExW(const void*n,void*h,unsigned flags){(void)n;assert(!h&&flags==0x800);return mode==10?NULL:(void*)1;}
static void* GetProcAddress(HMODULE h,const char*n){assert(h&&strcmp(n,"BCryptGenRandom")==0);return mode==11?NULL:(void*)(uintptr_t)&random_mock;}
static void FreeLibrary(HMODULE h){assert(h);}
static NTSTATUS helios_escape_ex_status(struct helios*h,void* data,size_t n,bool hardware){
 assert(h->device==123&&n==120&&!hardware);calls++;
 struct helios_attest_transport*r=data;memcpy(last_id,r->request_id,16);
 if(mode==1)return -1;
 if(mode==2)return 0;
 r->response_version=1;r->response_size=120;r->response_operation=r->operation;r->valid_marker=0x41545431;
 memcpy(r->response_id,r->request_id,16);r->capabilities=1;r->supported_record_version=2;
 r->accepted=r->operation==HELIOS_ATTEST_CALL;r->refusal_class=0;
 if(mode==3 && r->operation==HELIOS_ATTEST_QUERY)r->valid_marker=0;
 if(mode==4 && r->operation==HELIOS_ATTEST_CALL){r->accepted=0;r->refusal_class=6;}
 if(mode==5)return -1;
 if(mode==6)r->response_id[0]^=1;
 if(mode==7){r->accepted=1;r->refusal_class=6;}
 if(mode==8){r->accepted=0;r->refusal_class=0;}
 return 0;
}
/* FUNCTIONS */
int main(void){
 struct helios h={123};uint8_t carrier[16]={2};
 mode=0;assert(helios_carrier_attest_negotiated(&h,(HANDLE)42,carrier));assert(calls==2);
 uint8_t first[16];memcpy(first,last_id,16);calls=0;
 assert(second_negotiated(&h,(HANDLE)42,carrier));assert(calls==2);
 assert(memcmp(first,last_id,16)!=0); /* independent DLL instances */
 for(mode=1;mode<=11;mode++){
  calls=0;assert(!helios_carrier_attest_negotiated(&h,(HANDLE)42,carrier));
  assert(calls==(mode>=9?0:(mode==4||mode==8?2:1)));
 }
 puts("ATTEST_FLOW=PASS INSTANCES=2 FAILURE_CASES=11");return 0;
}
