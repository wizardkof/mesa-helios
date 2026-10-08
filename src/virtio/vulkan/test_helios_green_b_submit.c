#include "vn_renderer_helios_green_b_submit.h"
#include <assert.h>
#include <string.h>

struct fixture { unsigned calls, mode; };
static int escape_call(void *ctx, void *buffer, uint32_t size)
{
   struct fixture *f = ctx;
   f->calls++;
   struct helios_e1_submit *header = buffer;
   assert(header->hdr.cmd_type == HELIOS_E1_SUBMIT);
   assert(header->hdr.size + header->stream_size == size);
   if (f->mode == 1) return 1; /* untouched sentinel */
   header->fence_id = 77;
   header->response_version = 1;
   header->response_size = sizeof(*header);
   header->valid_marker = HELIOS_E1_SUBMIT_VALID;
   memcpy(header->response_id, header->request_id, 16);
   if (f->mode == 2) header->response_id[0]++;
   return f->mode != 3;
}
int main(void)
{
   uint8_t id[16] = {1};
   uint8_t stream[4] = {1,2,3,4};
   struct helios_e1_target target = { .carrier_handle = 4,
      .target_value = 17, .carrier_id = {2} };
   union { uint64_t align; uint8_t bytes[112 + 32 + 4]; } storage;
   uint8_t *buffer = storage.bytes;
   uint64_t fence = 0;
   struct fixture f = {0};
   assert(helios_e1_submit_exchange(&f, escape_call, id, 7, 1, 0, 0,
                                   &target, 1, stream, sizeof(stream),
                                   buffer, sizeof(storage.bytes), &fence));
   assert(f.calls == 1 && fence == 77);
   for (unsigned mode = 1; mode <= 3; mode++) {
      f = (struct fixture) { .mode = mode };
      fence = 0;
      assert(!helios_e1_submit_exchange(&f, escape_call, id, 7, 1, 0, 0,
                                        &target, 1, stream, sizeof(stream),
                                        buffer, sizeof(storage.bytes), &fence));
      assert(f.calls == 1 && fence == 0);
   }
   f = (struct fixture) {0};
   assert(!helios_e1_submit_exchange(&f, escape_call, id, 7, 1, 0, 0,
                                     &target, 1, stream, sizeof(stream),
                                     buffer, sizeof(storage.bytes)-1, &fence));
   assert(f.calls == 0);
   f = (struct fixture) {0};
   const uint32_t invalid_rings[] = {0, 256, UINT32_MAX};
   for (unsigned i = 0; i < 3; i++) {
      assert(!helios_e1_submit_exchange(&f, escape_call, id, 7, invalid_rings[i], 0, 0,
         &target, 1, stream, sizeof(stream), buffer, sizeof(storage.bytes), &fence));
      assert(f.calls == 0);
   }
   target.carrier_handle = 0;
   assert(!helios_e1_submit_exchange(&f, escape_call, id, 7, 1, 0, 0,
                                     &target, 1, stream, sizeof(stream),
                                     buffer, sizeof(storage.bytes), &fence));
   assert(f.calls == 0);
   return 0;
}
