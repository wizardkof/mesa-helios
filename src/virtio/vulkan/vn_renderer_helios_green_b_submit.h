/* One-escape E1 submit builder. No post-submit association or legacy fallback. */
#ifndef VN_RENDERER_HELIOS_GREEN_B_SUBMIT_H
#define VN_RENDERER_HELIOS_GREEN_B_SUBMIT_H
#include "vn_renderer_helios_green_b_control.h"
#include <limits.h>

static inline int helios_e1_target_valid(const struct helios_e1_target *target)
{
   return target->carrier_handle && target->target_value &&
          helios_e1_nonzero16(target->carrier_id);
}

static inline int helios_e1_submit_response_valid(
   const struct helios_e1_submit *response,
   const struct helios_e1_submit *request)
{
   return memcmp(response, request, 40) == 0 &&
          memcmp((const uint8_t *)response + 48,
                 (const uint8_t *)request + 48, 32) == 0 &&
          response->fence_id != 0 &&
          response->response_version == HELIOS_E1_SCHEMA_VERSION &&
          response->response_size == sizeof(*response) &&
          response->valid_marker == HELIOS_E1_SUBMIT_VALID &&
          response->response_status == 0 &&
          memcmp(response->response_id, request->request_id, 16) == 0;
}

static inline int helios_e1_submit_exchange(
   void *ctx, helios_e1_escape_fn escape, const uint8_t id[16],
   uint32_t ctx_id, uint32_t ring_idx,
   uint64_t present_cookie, uint32_t present_value32,
   const struct helios_e1_target *targets, uint32_t target_count,
   const void *stream, uint32_t stream_size,
   void *buffer, size_t capacity, uint64_t *out_fence)
{
   if (out_fence) *out_fence = 0;
   if (!escape || !id || !helios_e1_nonzero16(id) || !ctx_id ||
       !ring_idx || ring_idx > UINT8_MAX ||
       !targets || !target_count || target_count > HELIOS_E1_MAX_TARGETS ||
       !stream || !stream_size || !buffer || !out_fence ||
       ((uintptr_t)buffer & 7) ||
       ((present_cookie == 0) != (present_value32 == 0))) return 0;
   const size_t header_size = sizeof(struct helios_e1_submit) +
                              (size_t)target_count * sizeof(*targets);
   const size_t total = header_size + stream_size;
   if (total > capacity || total > UINT32_MAX) return 0;
   for (uint32_t i = 0; i < target_count; i++)
      if (!helios_e1_target_valid(&targets[i])) return 0;

   struct helios_e1_submit request = {0};
   request.hdr.magic = HELIOS_E1_MAGIC;
   request.hdr.cmd_type = HELIOS_E1_SUBMIT;
   request.hdr.version = HELIOS_E1_GLOBAL_VERSION;
   request.hdr.size = (uint32_t)header_size;
   request.schema_version = HELIOS_E1_SCHEMA_VERSION;
   memcpy(request.request_id, id, 16);
   request.ctx_id = ctx_id;
   request.ring_idx = ring_idx;
   request.stream_size = stream_size;
   request.target_count = target_count;
   request.present_cookie = present_cookie;
   request.present_value32 = present_value32;
   memcpy(buffer, &request, sizeof(request));
   memcpy((uint8_t *)buffer + sizeof(request), targets,
          (size_t)target_count * sizeof(*targets));
   memcpy((uint8_t *)buffer + header_size, stream, stream_size);
   const int status = escape(ctx, buffer, (uint32_t)total);
   struct helios_e1_submit response;
   memcpy(&response, buffer, sizeof(response));
   if (!status || !helios_e1_submit_response_valid(&response, &request) ||
       memcmp((const uint8_t *)buffer + sizeof(request), targets,
              (size_t)target_count * sizeof(*targets)) ||
       memcmp((const uint8_t *)buffer + header_size, stream, stream_size)) return 0;
   *out_fence = response.fence_id;
   return 1;
}
#endif
