/* P06/E1 Green B separate ABI. See GREEN_B_ABI_V1.md; existing 0x0001 is unchanged. */
#ifndef VN_RENDERER_HELIOS_GREEN_B_H
#define VN_RENDERER_HELIOS_GREEN_B_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(_MSC_VER)
#define HELIOS_E1_ALIGN8 __declspec(align(8))
#else
#define HELIOS_E1_ALIGN8 __attribute__((aligned(8)))
#endif

#define HELIOS_E1_MAGIC 0x48454c53u
#define HELIOS_E1_GLOBAL_VERSION 1u
#define HELIOS_E1_SCHEMA_VERSION 1u
#define HELIOS_E1_CONTROL 0x1bu
#define HELIOS_E1_SUBMIT 0x1cu
#define HELIOS_E1_CONTROL_QUERY 1u
#define HELIOS_E1_CONTROL_REGISTER 2u
#define HELIOS_E1_CONTROL_UNREGISTER 3u
#define HELIOS_E1_CONTROL_VALID 0x42314531u
#define HELIOS_E1_SUBMIT_VALID 0x42315331u
#define HELIOS_E1_MAX_TARGETS 16u

struct helios_e1_header {
   uint32_t magic, cmd_type, version, size;
};

struct HELIOS_E1_ALIGN8 helios_e1_control {
   struct helios_e1_header hdr;
   uint32_t schema_version, operation;
   uint8_t request_id[16];
   uint64_t carrier_handle, event_handle, target_value, registration_token;
   uint8_t carrier_id[16];
   uint32_t record_version, reserved;
   uint32_t response_version, response_size, response_operation, valid_marker;
   uint8_t response_id[16];
   uint64_t response_token;
   uint32_t accepted, refusal, capabilities, max_registrations;
};

struct HELIOS_E1_ALIGN8 helios_e1_target {
   uint64_t carrier_handle, target_value;
   uint8_t carrier_id[16];
};

struct HELIOS_E1_ALIGN8 helios_e1_submit {
   struct helios_e1_header hdr;
   uint32_t schema_version, flags;
   uint8_t request_id[16];
   uint64_t fence_id;
   uint32_t ctx_id, ring_idx, stream_size, target_count;
   uint64_t present_cookie;
   uint32_t present_value32, reserved;
   uint32_t response_version, response_size, valid_marker, response_status;
   uint8_t response_id[16];
};

_Static_assert(sizeof(struct helios_e1_control) == 152, "E1 control size");
_Static_assert(sizeof(struct helios_e1_submit) == 112, "E1 submit size");
_Static_assert(sizeof(struct helios_e1_target) == 32, "E1 target size");
_Static_assert(offsetof(struct helios_e1_control, request_id) == 24, "E1 id offset");
_Static_assert(offsetof(struct helios_e1_control, carrier_handle) == 40, "E1 handle offset");
_Static_assert(offsetof(struct helios_e1_control, response_version) == 96, "E1 response offset");
_Static_assert(offsetof(struct helios_e1_control, response_id) == 112, "E1 response id offset");
_Static_assert(offsetof(struct helios_e1_submit, fence_id) == 40, "E1 fence offset");
_Static_assert(offsetof(struct helios_e1_submit, response_id) == 96, "E1 submit id offset");

static inline int helios_e1_nonzero16(const uint8_t id[16])
{
   uint8_t bits = 0;
   for (unsigned i = 0; i < 16; i++) bits |= id[i];
   return bits != 0;
}

static inline int helios_e1_zero_bytes(const void *ptr, size_t len)
{
   const uint8_t *p = ptr;
   for (size_t i = 0; i < len; i++) if (p[i]) return 0;
   return 1;
}

static inline struct helios_e1_control
helios_e1_control_query(const uint8_t id[16])
{
   struct helios_e1_control req = {0};
   req.hdr.magic = HELIOS_E1_MAGIC;
   req.hdr.cmd_type = HELIOS_E1_CONTROL;
   req.hdr.version = HELIOS_E1_GLOBAL_VERSION;
   req.hdr.size = sizeof(req);
   req.schema_version = HELIOS_E1_SCHEMA_VERSION;
   req.operation = HELIOS_E1_CONTROL_QUERY;
   memcpy(req.request_id, id, 16);
   return req;
}

static inline int helios_e1_control_valid_request(
   const struct helios_e1_control *req, size_t actual)
{
   if (!req || actual != sizeof(*req) || req->hdr.magic != HELIOS_E1_MAGIC ||
       req->hdr.cmd_type != HELIOS_E1_CONTROL ||
       req->hdr.version != HELIOS_E1_GLOBAL_VERSION ||
       req->hdr.size != sizeof(*req) ||
       req->schema_version != HELIOS_E1_SCHEMA_VERSION ||
       !helios_e1_nonzero16(req->request_id) || req->reserved ||
       !helios_e1_zero_bytes(&req->response_version, 56)) return 0;
   switch (req->operation) {
   case HELIOS_E1_CONTROL_QUERY:
      return helios_e1_zero_bytes(&req->carrier_handle, 52);
   case HELIOS_E1_CONTROL_REGISTER:
      return req->carrier_handle && req->event_handle && req->target_value &&
             !req->registration_token && helios_e1_nonzero16(req->carrier_id) &&
             req->record_version == 2;
   case HELIOS_E1_CONTROL_UNREGISTER:
      return req->registration_token &&
             helios_e1_zero_bytes(&req->carrier_handle, 24) &&
             helios_e1_zero_bytes(req->carrier_id, 20);
   default:
      return 0;
   }
}

static inline int helios_e1_control_valid_response(
   const struct helios_e1_control *response,
   const struct helios_e1_control *request, size_t actual)
{
   if (!response || !helios_e1_control_valid_request(request, sizeof(*request)) ||
       actual != sizeof(*response) || memcmp(response, request, 96) ||
       response->response_version != HELIOS_E1_SCHEMA_VERSION ||
       response->response_size != sizeof(*response) ||
       response->response_operation != request->operation ||
       response->valid_marker != HELIOS_E1_CONTROL_VALID ||
       memcmp(response->response_id, request->request_id, 16) ||
       response->accepted > 1 ||
       ((response->accepted == 1) == (response->refusal != 0))) return 0;
   if (!response->accepted) return !response->response_token;
   switch (response->operation) {
   case HELIOS_E1_CONTROL_QUERY:
      return !response->response_token && (response->capabilities & 3) == 3 &&
             response->max_registrations > 0;
   case HELIOS_E1_CONTROL_REGISTER:
      return response->response_token != 0;
   case HELIOS_E1_CONTROL_UNREGISTER:
      return response->response_token == 0;
   default:
      return 0;
   }
}

#endif
