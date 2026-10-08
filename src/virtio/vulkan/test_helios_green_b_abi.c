#include "vn_renderer_helios_green_b.h"
#include <assert.h>
#include <string.h>

int main(void)
{
   assert(sizeof(struct helios_e1_control) == 152);
   assert(sizeof(struct helios_e1_submit) == 112);
   assert(sizeof(struct helios_e1_target) == 32);
   assert(offsetof(struct helios_e1_control, request_id) == 24);
   assert(offsetof(struct helios_e1_control, response_version) == 96);
   assert(offsetof(struct helios_e1_control, response_id) == 112);
   assert(offsetof(struct helios_e1_submit, fence_id) == 40);
   assert(offsetof(struct helios_e1_submit, response_id) == 96);

   uint8_t id[16];
   memset(id, 0x45, sizeof(id));
   struct helios_e1_control request = helios_e1_control_query(id);
   assert(helios_e1_control_valid_request(&request, sizeof(request)));
   struct helios_e1_control response = request;
   response.response_version = 1;
   response.response_size = sizeof(response);
   response.response_operation = HELIOS_E1_CONTROL_QUERY;
   response.valid_marker = HELIOS_E1_CONTROL_VALID;
   memcpy(response.response_id, id, sizeof(id));
   response.accepted = 1;
   response.capabilities = 3;
   response.max_registrations = 256;
   assert(helios_e1_control_valid_response(&response, &request,
                                           sizeof(response)));
   response.response_id[0]++;
   assert(!helios_e1_control_valid_response(&response, &request,
                                            sizeof(response)));
   return 0;
}
