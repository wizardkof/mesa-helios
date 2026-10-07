#include "vn_renderer_helios_attest_transport.h"
#include <assert.h>
int main(void) {
 uint8_t id[16]={1},carrier[16]={2};
 for(uint32_t op=0x19;op<=0x1a;op++) {
  struct helios_attest_transport q=helios_attest_request(op,id,42,carrier,2);
  assert(!helios_attest_response_valid(&q,&q,sizeof(q)));
  for(uint32_t c=0;c<8;c++) {
   struct helios_attest_transport r=q;
   r.response_version=1;r.response_size=120;r.response_operation=op;r.valid_marker=0x41545431;
   memcpy(r.response_id,id,16);r.accepted=op==0x1a && c==0;r.refusal_class=op==0x19?0:c;
   r.capabilities=1;r.supported_record_version=2;
   assert(helios_attest_response_valid(&r,&q,120));
   assert(!helios_attest_response_valid(&r,&q,119));
   for(unsigned i=0;i<120;i++) {
    struct helios_attest_transport b=r;((uint8_t*)&b)[i]^=0x80;
    assert(!helios_attest_response_valid(&b,&q,120));
   }
   struct helios_attest_transport other=q;other.request_id[0]++;
   assert(!helios_attest_response_valid(&r,&other,120));
  }
 }
 return 0;
}
