/* SPDX-License-Identifier: MIT */
#ifndef VN_RENDERER_HELIOS_WAIT_RESULT_H
#define VN_RENDERER_HELIOS_WAIT_RESULT_H

#include <stdint.h>
#include <vulkan/vulkan_core.h>

#define HELIOS_VIRTIO_RESP_ERR_UNSPEC 0x1200u

enum helios_wire_state { HELIOS_WIRE_PENDING, HELIOS_WIRE_SUCCESS, HELIOS_WIRE_ERROR };
struct helios_wire_result {
   enum helios_wire_state state;
   uint32_t response_type;
};

static inline VkResult
helios_wire_vk_result(struct helios_wire_result result)
{
   if (result.state == HELIOS_WIRE_SUCCESS)
      return VK_SUCCESS;
   if (result.state == HELIOS_WIRE_PENDING)
      return VK_TIMEOUT;
   /* Only ERR_UNSPEC has a policy in this gate. Unmapped terminal errors
    * still fail closed; a future policy may choose a more specific VkResult. */
   if (result.response_type == HELIOS_VIRTIO_RESP_ERR_UNSPEC)
      return VK_ERROR_UNKNOWN;
   return VK_ERROR_UNKNOWN;
}

#endif
