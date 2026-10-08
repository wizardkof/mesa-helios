#!/usr/bin/env python3
"""Compile actual Mesa wait callbacks/wrapper; Windows calls are deterministic seams.
SOURCE_EXTRACTION/CPU_SIMULATION only, not native renderer/GPU qualification.
"""
from pathlib import Path
import subprocess,sys,os
root=Path(__file__).parent
source=Path(os.environ.get('P06_SOURCE', root/'vn_renderer_helios.c')).read_text()
assert 'helios_e1_wait_blocking(helios, wait)' in source[source.index('\nhelios_wait('):]
start=source.index('struct helios_e1_wait_context {')
end=source.index('\nstatic VkResult\nhelios_wait(',start)
actual=source[start:end]
unit=r"""
#include "vn_renderer_helios_green_b_control.h"
#include "vn_renderer_helios_green_b_wait.h"
#include "vn_renderer_helios_carrier_v2.h"
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <wchar.h>
typedef int VkResult;
typedef void *HANDLE;
typedef uint32_t DWORD;
#define VK_SUCCESS 0
#define VK_TIMEOUT 2
#define VK_ERROR_UNKNOWN -13
#define VK_ERROR_INVALID_EXTERNAL_HANDLE -1000072003
#define VK_ERROR_FEATURE_NOT_PRESENT -8
#define VK_ERROR_OUT_OF_HOST_MEMORY -1
#define HELIOS_SYNC_BACKING_E1_V2 2
#define INFINITE UINT32_MAX
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define FALSE 0
struct helios {int unused;};
struct helios_sync {int backing;HANDLE carrier_handle_owned;
  volatile struct helios_carrier_v2_record *carrier_view_readonly;uint8_t carrier_id[16];};
struct vn_renderer_wait {unsigned sync_count;struct helios_sync **syncs;
  uint64_t *sync_values;bool wait_any;uint64_t timeout;};
static uint64_t clock_ns, setup_cost;
static unsigned registers, unregisters, waits, queries, next_token, closed;
static int stage, fail_at_register, fail_unregister, error_wake;
static struct helios_carrier_v2_record record, complete_record;
static int signaled;
static int64_t os_time_get_nano(void){return (int64_t)clock_ns;}
static bool helios_e1_fresh_request_id(uint8_t id[16]){memset(id,1,16);return true;}
static HANDLE CreateEventW(void*a,int b,int c,const wchar_t*d){(void)a;(void)b;(void)c;(void)d;return (HANDLE)1;}
static bool ResetEvent(HANDLE e){(void)e;signaled=0;return true;}
static bool CloseHandle(HANDLE e){(void)e;closed++;return true;}
static void publish(void){record.sequence+=2;if(error_wake){record.terminal_error_value=9;record.terminal_response_type=0x1200;}else record.completed_value=9;signaled=1;}
static DWORD WaitForSingleObject(HANDLE e,DWORD timeout){(void)e;waits++;assert(waits<4);
 if(stage==3){publish();return WAIT_OBJECT_0;}if(signaled)return WAIT_OBJECT_0;
 clock_ns+=(uint64_t)timeout*1000000;return WAIT_TIMEOUT;}
static int helios_e1_escape_callback(void*data,void*buffer,uint32_t size){(void)data;assert(size==152);
 struct helios_e1_control*q=buffer;q->response_version=1;q->response_size=152;
 q->response_operation=q->operation;q->valid_marker=HELIOS_E1_CONTROL_VALID;
 memcpy(q->response_id,q->request_id,16);q->accepted=1;q->capabilities=3;q->max_registrations=256;
 if(q->operation==HELIOS_E1_CONTROL_QUERY){queries++;clock_ns+=setup_cost;}
 if(q->operation==HELIOS_E1_CONTROL_REGISTER){registers++;
  if(fail_at_register && registers==(unsigned)fail_at_register)return false;
  q->response_token=++next_token;if(stage==2)publish();
  const struct helios_carrier_v2_record *selected=q->carrier_handle==6?&complete_record:&record;
  if(selected->completed_value>=q->target_value)signaled=1;}
 if(q->operation==HELIOS_E1_CONTROL_UNREGISTER){unregisters++;if(fail_unregister)return false;}
 return true;}
"""
unit+='\n'+actual
unit+=r"""
static void reset(void){clock_ns=0;setup_cost=0;registers=unregisters=waits=queries=next_token=closed=0;
 stage=fail_at_register=fail_unregister=error_wake=signaled=0;
 memset(&record,0,sizeof(record));record.magic=HELIOS_P06_SECTION_MAGIC;record.version=2;record.size=64;memset(record.carrier_id,1,16);}
int main(void){
 struct helios h={0};struct helios_sync sync={.backing=2,.carrier_handle_owned=(HANDLE)5,
  .carrier_view_readonly=&record,.carrier_id={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}};
 struct helios_sync*syncs[]={&sync,&sync};uint64_t values[]={9,9};
 struct vn_renderer_wait w={.sync_count=1,.syncs=syncs,.sync_values=values,.timeout=10000000};
 reset();w.timeout=0;assert(helios_e1_wait_blocking(&h,&w)==VK_TIMEOUT);assert(!queries&&!registers&&!waits);
 w.timeout=10000000;
 reset();publish();assert(helios_e1_wait_blocking(&h,&w)==VK_SUCCESS);assert(!queries&&!registers&&!waits);
 reset();stage=2;assert(helios_e1_wait_blocking(&h,&w)==VK_SUCCESS);assert(registers==1&&unregisters==1&&!waits&&closed==1);
 reset();stage=3;assert(helios_e1_wait_blocking(&h,&w)==VK_SUCCESS);assert(registers==1&&unregisters==1&&waits==1);
 reset();stage=3;error_wake=1;assert(helios_e1_wait_blocking(&h,&w)==VK_ERROR_UNKNOWN);assert(registers==unregisters);
 reset();assert(helios_e1_wait_blocking(&h,&w)==VK_TIMEOUT);assert(clock_ns==10000000&&registers==unregisters);
 reset();setup_cost=10000000;assert(helios_e1_wait_blocking(&h,&w)==VK_TIMEOUT);assert(!registers&&!waits);
 reset();stage=2;fail_unregister=1;assert(helios_e1_wait_blocking(&h,&w)==VK_ERROR_UNKNOWN);assert(closed==1);
 reset();w.sync_count=2;fail_at_register=2;assert(helios_e1_wait_blocking(&h,&w)==VK_ERROR_UNKNOWN);assert(registers==2&&unregisters==1&&closed==1);
 reset();complete_record=record;complete_record.completed_value=9;memset(complete_record.carrier_id,2,16);
 struct helios_sync completed=sync;completed.carrier_handle_owned=(HANDLE)6;memset(completed.carrier_id,2,16);completed.carrier_view_readonly=&complete_record;
 syncs[0]=&completed;syncs[1]=&sync;w.sync_count=2;w.wait_any=false;
 assert(helios_e1_wait_blocking(&h,&w)==VK_TIMEOUT);
 assert(registers==1&&unregisters==1&&waits==1); /* completed subset must not self-wake wait-all */
 syncs[0]=&sync;
 reset();sync.backing=0;assert(helios_e1_wait_blocking(&h,&w)==VK_ERROR_FEATURE_NOT_PRESENT);assert(!queries);
 return 0;
}
"""
out=Path(sys.argv[1]) if len(sys.argv)>1 else Path('/tmp/helios-e1-wait-production')
out.with_suffix('.c').write_text(unit)
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I',str(root),str(out.with_suffix('.c')),'-o',str(out)],check=True)
if os.environ.get('P06_COMPILE_ONLY')!='1':subprocess.run([str(out)],check=True)
