/* Production Green B control envelope exchange; OS escape is a test seam. */
#ifndef VN_RENDERER_HELIOS_GREEN_B_CONTROL_H
#define VN_RENDERER_HELIOS_GREEN_B_CONTROL_H
#include "vn_renderer_helios_green_b.h"

typedef int (*helios_e1_escape_fn)(void *ctx, void *buffer, uint32_t size);

static inline int helios_e1_control_exchange(
   const struct helios_e1_control *request,
   helios_e1_escape_fn escape, void *ctx,
   struct helios_e1_control *response)
{
   if (!request || !escape || !response ||
       !helios_e1_control_valid_request(request, sizeof(*request))) return 0;
   *response = *request;
   const int transport_ok = escape(ctx, response, sizeof(*response));
   return transport_ok &&
          helios_e1_control_valid_response(response, request, sizeof(*response)) &&
          response->accepted == 1 && response->refusal == 0;
}
#endif
