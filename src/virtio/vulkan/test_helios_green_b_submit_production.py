#!/usr/bin/env python3
"""Compile the exact production Green B submit bodies with a fake escape seam."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parent
source = Path(os.environ.get("P06_SOURCE", root / "vn_renderer_helios.c")).read_text()


def body(name):
    start = source.index("\n" + name + "(") + 1
    begin = source.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


unit = r'''
#include "vn_renderer_helios_green_b_submit.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
typedef int NTSTATUS;
typedef unsigned char *PUCHAR;
typedef unsigned long ULONG;
typedef void *HMODULE;
typedef struct { long long QuadPart; } LARGE_INTEGER;
#define WINAPI
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0
#define VK_SUCCESS 0
#define VK_ERROR_DEVICE_LOST -4
#define VK_ERROR_INVALID_EXTERNAL_HANDLE -1000072003
#define VK_ERROR_FEATURE_NOT_PRESENT -8
#define VK_ERROR_OUT_OF_HOST_MEMORY -1
#define HELIOS_SYNC_BACKING_E1_V2 2
typedef int VkResult;
typedef struct vn_renderer { int unused; } vn_renderer;
struct helios_sync { int backing; void *carrier_handle_owned; unsigned char carrier_id[16]; int wddm_local; };
struct vn_renderer_submit_batch {
  unsigned sync_count; struct helios_sync **syncs; unsigned long long *sync_values;
  const void *cs_data; size_t cs_size; unsigned ring_idx;
  unsigned long long present_cookie; unsigned present_value32;
};
struct vn_renderer_submit { unsigned batch_count; struct vn_renderer_submit_batch *batches; };
struct helios { struct vn_renderer base; int dev_mutex; struct { bool enabled;
  unsigned long long submit_mutex_ticks,submit_calls,submit_batches,submit_syncs,
  submit_cs_bytes,submit_empty_batches,submit_escape_ticks,submit_sync_ticks; } perf;
  unsigned ctx_id; unsigned char *submit_buf; size_t submit_buf_capacity; };
static unsigned calls_query, calls_e1, calls_legacy, calls_append;
static int fail_e1, fail_query;
static int fake_random(void *a, PUCHAR p, ULONG n, ULONG flags) {
  (void)a; (void)flags; for (ULONG i=0;i<n;i++) p[i]=(unsigned char)(i+1); return 0;
}
static HMODULE LoadLibraryExW(const wchar_t *a, void *b, unsigned c) {
  (void)a;(void)b;(void)c; return (HMODULE)1;
}
static void *GetProcAddress(HMODULE h,const char *n) {
  (void)h;(void)n; return (void *)fake_random;
}
static void FreeLibrary(HMODULE h) { (void)h; }
static bool helios_escape(struct helios *h,void *buffer,unsigned size) {
  (void)h; struct helios_e1_header *hdr=buffer;
  if (hdr->cmd_type==HELIOS_E1_CONTROL) {
    assert(size==152); calls_query++;
    if (fail_query) return false;
    struct helios_e1_control *q=buffer;
    q->response_version=1; q->response_size=152;
    q->response_operation=q->operation; q->valid_marker=HELIOS_E1_CONTROL_VALID;
    memcpy(q->response_id,q->request_id,16); q->accepted=1;
    q->capabilities=3; q->max_registrations=256; return true;
  }
  assert(hdr->cmd_type==HELIOS_E1_SUBMIT); calls_e1++;
  if (fail_e1) return false;
  struct helios_e1_submit *s=buffer;
  assert(size==s->hdr.size+s->stream_size);
  assert(s->target_count==1 && s->ctx_id==7 && s->ring_idx==1);
  struct helios_e1_target *target=(void *)((unsigned char *)buffer+sizeof(*s));
  assert(target->carrier_handle==4 && target->target_value==17 &&
         target->carrier_id[0]==2);
  assert(memcmp((unsigned char *)buffer+s->hdr.size,"\1\2\3",3)==0);
  s->fence_id=19; s->response_version=1; s->response_size=112;
  s->valid_marker=HELIOS_E1_SUBMIT_VALID;
  memcpy(s->response_id,s->request_id,16); return true;
}
static bool helios_ioctl_submit_cs(struct helios *h,const void *data,size_t size,
  unsigned ring,uint64_t cookie,unsigned value,uint64_t *fence) {
  (void)h;(void)data;(void)size;(void)ring;(void)cookie;(void)value;
  calls_legacy++; *fence=20; return true;
}
static bool helios_sync_append_locked(struct vn_renderer *r,struct helios_sync *s,
  unsigned long long value,unsigned long long fence) {
  (void)r;(void)s;(void)value;(void)fence; calls_append++; return true;
}
static bool helios_retire_enqueue_locked(struct helios *h,struct helios_sync *s,
  unsigned long long fence) { (void)h;(void)s;(void)fence; return true; }
static void helios_perf_write(struct helios *h,bool x) {(void)h;(void)x;}
static void mtx_lock(int *p) {(void)p;}
static void mtx_unlock(int *p) {(void)p;}
static void QueryPerformanceCounter(LARGE_INTEGER *p) {p->QuadPart=0;}
'''
for ret, name in [("bool", "helios_p06_request_id"),
                  ("bool", "helios_e1_fresh_request_id"),
                  ("int", "helios_e1_escape_callback"),
                  ("bool", "helios_e1_submit_batch"),
                  ("VkResult", "helios_submit")]:
    unit += "\nstatic " + ret + "\n" + body(name) + "\n"
unit += r'''
int main(void) {
  struct helios h={0}; h.ctx_id=7;
  struct helios_sync e={.backing=HELIOS_SYNC_BACKING_E1_V2,
    .carrier_handle_owned=(void *)4,.carrier_id={2}};
  struct helios_sync w={0}; struct helios_sync *syncs[]={&e};
  unsigned long long values[]={17}; unsigned char stream[]={1,2,3};
  struct vn_renderer_submit_batch batch={.sync_count=1,.syncs=syncs,
    .sync_values=values,.cs_data=stream,.cs_size=sizeof(stream),.ring_idx=1};
  struct vn_renderer_submit submit={.batch_count=1,.batches=&batch};
  assert(helios_submit(&h.base,&submit)==VK_SUCCESS);
  assert(calls_query==1 && calls_e1==1 && calls_legacy==0 && calls_append==1);
  fail_e1=1; assert(helios_submit(&h.base,&submit)==VK_ERROR_DEVICE_LOST);
  assert(calls_e1==2 && calls_legacy==0 && calls_append==1);
  fail_e1=0; batch.cs_size=0;
  assert(helios_submit(&h.base,&submit)==VK_ERROR_FEATURE_NOT_PRESENT);
  assert(calls_e1==2 && calls_query==2 && calls_append==1);
  batch.cs_size=sizeof(stream); syncs[0]=&w;
  assert(helios_submit(&h.base,&submit)==VK_SUCCESS);
  assert(calls_legacy==1 && calls_e1==2);
  syncs[0]=&e; fail_query=1;
  assert(helios_submit(&h.base,&submit)==VK_ERROR_DEVICE_LOST);
  assert(calls_query==3 && calls_e1==2 && calls_legacy==1);
  free(h.submit_buf); return 0;
}
'''
out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/p06-e1-submit-production")
compiler = os.environ.get("CC", "cc")
generated = out.with_suffix(".c")
generated.write_text(unit)
subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root),
                str(generated), "-o", str(out)], check=True)
if os.environ.get("P06_COMPILE_ONLY") != "1":
    subprocess.run([str(out)], check=True)
