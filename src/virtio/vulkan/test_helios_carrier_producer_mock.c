/* Test-only seams. The Python runner inserts exact production C bodies. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
typedef LONG NTSTATUS;
#else
typedef uint32_t ULONG, DWORD, ACCESS_MASK;
typedef int32_t NTSTATUS;
typedef uint16_t WCHAR, USHORT;
typedef void *HANDLE, *HMODULE, *PVOID;
typedef void (*FARPROC)(void);
#define NTAPI
#define WINAPI
#define FALSE 0
#define FILE_MAP_READ 4
#define SECTION_MAP_READ 4
#define SECTION_QUERY 1
#endif
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define NT_SUCCESS(s) ((s) >= 0)
#define VK_SUCCESS 0
#define VK_ERROR_INVALID_EXTERNAL_HANDLE (-1000072003)
typedef int VkResult;
#define HELIOS_P06_PRODUCTION_CREATE 1
#define HELIOS_P06_PRODUCTION_RELEASE 5
#define HELIOS_P06_SECTION_LEASE_KERNEL_HANDLE_RETAINED 1
#define HELIOS_SYNC_BACKING_NONE 0
#define HELIOS_SYNC_BACKING_E1_V2 2
#include "vn_renderer_helios_carrier_v2.h"
/* PRODUCTION_STRUCTS */
_Static_assert(sizeof(struct helios_escape_p06_production_carrier)==600,"wire ABI");
_Static_assert(sizeof(struct _OBJECT_ATTRIBUTES)==(sizeof(void*)==8?48:24),"OA ABI");
_Static_assert(sizeof(struct helios_unicode_string)==(sizeof(void*)==8?16:8),"US ABI");
struct vn_renderer { int unused; };
struct helios { struct vn_renderer base; };
struct helios_sync {
 unsigned backing, wddm_local; HANDLE nt_named_handle;
 bool carrier_lease_live; uint64_t carrier_generation; uint32_t carrier_slot;
 uint8_t carrier_id[16]; uint64_t val;
 HANDLE carrier_handle_owned; const volatile struct helios_carrier_v2_record *carrier_view_readonly;
};
static unsigned opens, maps, closes, unmaps, creates, releases, leases, mask;
static bool fail_open, fail_map, fail_create, no_api, no_module;
static struct helios_escape_p06_production_carrier reply;
static struct helios_carrier_v2_record record;
static const WCHAR expected_native[]=L"\\BaseNamedObjects\\HeliosP06Carrier_0123456789abcdef0123456789abcdef";
static NTSTATUS helios_carrier_escape(struct helios *h, struct helios_escape_p06_production_carrier *r) {
 (void)h;
 if(r->op==HELIOS_P06_PRODUCTION_CREATE){creates++;if(fail_create)return -1;*r=reply;leases++;return 0;}
 assert(r->op==HELIOS_P06_PRODUCTION_RELEASE);assert(leases==1);leases--;releases++;return 0;
}
static void helios_diag(const char *fmt, ...) {(void)fmt;}
static DWORD mock_error(void){return 5;}
static HANDLE WINAPI mock_old_open(DWORD access, int inherit, const WCHAR *name){
 (void)inherit;(void)name;assert(leases==1);opens++;mask=access;return fail_open?NULL:(HANDLE)(uintptr_t)42;
}
static NTSTATUS NTAPI mock_nt_open(HANDLE *out, ACCESS_MASK access, struct _OBJECT_ATTRIBUTES *oa){
 assert(leases==1);opens++;mask=access;
 assert(access==(SECTION_MAP_READ|SECTION_QUERY));
 assert(oa->Length==sizeof(*oa)&&oa->RootDirectory==NULL&&oa->SecurityDescriptor==NULL&&oa->SecurityQualityOfService==NULL);
 assert((oa->Attributes & 0x200)==0); /* no OBJ_KERNEL_HANDLE */
 struct helios_unicode_string *name=oa->ObjectName;
 assert(name->Length==sizeof(expected_native)-sizeof(WCHAR));
 assert(name->MaximumLength>=name->Length);
 assert(memcmp(name->Buffer,expected_native,name->Length)==0);
 *out=fail_open?NULL:(HANDLE)(uintptr_t)42;return fail_open?-1:0;
}
static HMODULE WINAPI mock_module(const WCHAR *name){(void)name;return no_module?NULL:(HMODULE)(uintptr_t)1;}
static FARPROC WINAPI mock_proc(HMODULE mod,const char *name){assert(mod);assert(strcmp(name,"NtOpenSection")==0);return no_api?NULL:(FARPROC)mock_nt_open;}
static void *WINAPI mock_map(HANDLE h,DWORD access,DWORD hi,DWORD lo,size_t size){
 assert(leases==1&&h==(HANDLE)(uintptr_t)42&&access==FILE_MAP_READ&&hi==0&&lo==0&&size==64);
 maps++;return fail_map?NULL:&record;
}
static int WINAPI mock_close(HANDLE h){assert(h==(HANDLE)(uintptr_t)42);closes++;return 1;}
static int WINAPI mock_unmap(const void *p){assert(p==&record);unmaps++;return 1;}
#define OpenFileMappingW mock_old_open
#define GetModuleHandleW mock_module
#define GetProcAddress mock_proc
#define MapViewOfFile mock_map
#define CloseHandle mock_close
#define UnmapViewOfFile mock_unmap
#define GetLastError mock_error
/* PRODUCTION_FUNCTIONS */
static void reset(void){
 opens=maps=closes=unmaps=creates=releases=leases=mask=0;
 fail_open=fail_map=fail_create=no_api=no_module=false;
 memset(&reply,0,sizeof(reply));memset(&record,0,sizeof(record));
 const uint8_t id[16]={1,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,1,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
 memcpy(reply.carrier_id,id,16);reply.generation=9;reply.slot_index=2;reply.lease_flags=1;
 const WCHAR win[]=L"Global\\HeliosP06Carrier_0123456789abcdef0123456789abcdef";
 memcpy(reply.object_name,win,sizeof(win));memcpy(reply.native_name,expected_native,sizeof(expected_native));
 record.magic=HELIOS_P06_SECTION_MAGIC;record.version=2;record.size=64;memcpy(record.carrier_id,id,16);
}
static void refused(struct helios *h,unsigned expected_opens,unsigned expected_maps,unsigned expected_closes,unsigned expected_unmaps){
 struct helios_sync s={0};
 assert(helios_carrier_create_producer(&h->base,&s)==VK_ERROR_INVALID_EXTERNAL_HANDLE);
 assert(opens==expected_opens&&maps==expected_maps&&closes==expected_closes&&unmaps==expected_unmaps);
 assert(leases==0&&releases==1&&!s.carrier_lease_live&&!s.carrier_handle_owned&&!s.carrier_view_readonly);
}
int main(void){
 struct helios h={0};reset();struct helios_sync s={0};
 assert(helios_carrier_create_producer(&h.base,&s)==0);
 if(mask!=5){fprintf(stderr,"RED: producer open access=%u expected native read|query=5\n",mask);return 1;}
 assert(opens==1&&maps==1&&leases==1&&s.backing==HELIOS_SYNC_BACKING_E1_V2);
 assert(helios_carrier_create_producer(&h.base,&s)==0&&opens==1&&creates==1);
 helios_carrier_drop_owned(&h,&s);assert(leases==0&&closes==1&&unmaps==1&&releases==1);
 unsigned cases=2;
 for(unsigned i=0;i<9;i++){
  reset();size_t len=ARRAY_SIZE(expected_native)-1;
  switch(i){
   case 0:reply.native_name[0]='X';break;
   case 1:reply.native_name[len-1]='0';break;
   case 2:reply.native_name[len]='x';break;
   case 3:for(unsigned j=0;j<128;j++)reply.native_name[j]='a';break;
   case 4:reply.native_name[8]=0;break;
   case 5:reply.native_name[len-1]='F';break;
   case 6:memset(reply.carrier_id,0,16);break;
   case 7:reply.object_name[0]='x';break;
   case 8:reply.lease_flags=0;break;
  }
  refused(&h,0,0,0,0);cases++;
 }
 reset();fail_open=true;refused(&h,1,0,0,0);cases++;
 reset();no_module=true;refused(&h,0,0,0,0);cases++;
 reset();no_api=true;refused(&h,0,0,0,0);cases++;
 reset();fail_map=true;refused(&h,1,1,1,0);cases++;
 reset();record.version=1;refused(&h,1,1,1,1);cases++;
 reset();record.carrier_id[0]^=1;refused(&h,1,1,1,1);cases++;
 reset();fail_create=true;s=(struct helios_sync){0};assert(helios_carrier_create_producer(&h.base,&s)==VK_ERROR_INVALID_EXTERNAL_HANDLE);assert(creates==1&&!opens&&!leases&&!releases);cases++;
 for(unsigned i=0;i<2;i++){reset();s=(struct helios_sync){0};if(i)s.nt_named_handle=(HANDLE)(uintptr_t)3;else s.wddm_local=3;assert(helios_carrier_create_producer(&h.base,&s)==VK_ERROR_INVALID_EXTERNAL_HANDLE);assert(!creates&&!opens&&!closes);cases++;}
 printf("PRODUCTION_PRODUCER_TESTS=PASS CASES=%u WIRE=%zu OA=%zu US=%zu\n",cases,sizeof(reply),sizeof(struct _OBJECT_ATTRIBUTES),sizeof(struct helios_unicode_string));
 return 0;
}
