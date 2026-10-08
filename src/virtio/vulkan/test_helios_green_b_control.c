#include "vn_renderer_helios_green_b_control.h"
#include <assert.h>
#include <string.h>

struct fixture { unsigned mode, calls; };
static int escape_call(void *ctx, void *buffer, uint32_t size)
{
   struct fixture *f = ctx;
   f->calls++;
   assert(size == sizeof(struct helios_e1_control));
   struct helios_e1_control *r = buffer;
   if (f->mode == 1) return 1; /* transport succeeded, bytes untouched */
   r->response_version = 1;
   r->response_size = sizeof(*r);
   r->response_operation = r->operation;
   r->valid_marker = HELIOS_E1_CONTROL_VALID;
   memcpy(r->response_id, r->request_id, 16);
   r->accepted = f->mode == 4 ? 0 : 1;
   r->refusal = f->mode == 4 ? 1 : 0;
   r->capabilities = 3;
   r->max_registrations = 256;
   if (r->operation == HELIOS_E1_CONTROL_REGISTER) r->response_token = 88;
   if (f->mode == 2) r->response_id[0]++;
   return f->mode != 3;
}
int main(void)
{
   uint8_t id[16];
   memset(id, 0x61, sizeof(id));
   struct helios_e1_control query = helios_e1_control_query(id);
   struct fixture f = {0};
   struct helios_e1_control out;
   assert(helios_e1_control_exchange(&query, escape_call, &f, &out));
   assert(f.calls == 1 && out.accepted == 1);
   for (unsigned mode = 1; mode <= 4; mode++) {
      f = (struct fixture) { .mode = mode };
      assert(!helios_e1_control_exchange(&query, escape_call, &f, &out));
      assert(f.calls == 1);
   }
   struct helios_e1_control reg = query;
   reg.operation = HELIOS_E1_CONTROL_REGISTER;
   reg.carrier_handle = 4;
   reg.event_handle = 8;
   reg.target_value = 17;
   memset(reg.carrier_id, 0x71, 16);
   reg.record_version = 2;
   f = (struct fixture) {0};
   assert(helios_e1_control_exchange(&reg, escape_call, &f, &out));
   assert(out.response_token == 88);
   return 0;
}
