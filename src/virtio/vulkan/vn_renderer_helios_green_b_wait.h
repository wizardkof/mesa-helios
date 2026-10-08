/* Shared production wait state machine and deterministic local test seam.
 * Callbacks arm kernel notifications and an interruptible aggregate event;
 * this loop never polls by interval and never treats an event as completion. */
#ifndef VN_RENDERER_HELIOS_GREEN_B_WAIT_H
#define VN_RENDERER_HELIOS_GREEN_B_WAIT_H
#include <stdint.h>

enum helios_e1_wait_state {
   HELIOS_E1_WAIT_PENDING,
   HELIOS_E1_WAIT_COMPLETE,
   HELIOS_E1_WAIT_ERROR,
   HELIOS_E1_WAIT_TIMEOUT,
};
enum helios_e1_wait_wake {
   HELIOS_E1_WAKE_EVENT,
   HELIOS_E1_WAKE_TIMEOUT,
   HELIOS_E1_WAKE_FAILED,
};
struct helios_e1_wait_ops {
   void *ctx;
   enum helios_e1_wait_state (*observe)(void *ctx);
   /* arm resets the private event, registers each pending target, arranges
    * notification for WDDM subset, and rechecks under the publisher's lock. */
   int (*arm)(void *ctx);
   /* disarm unregisters every token exactly once, including partial arms. */
   void (*disarm)(void *ctx);
   uint64_t (*now_ns)(void *ctx);
   /* Waits on <=2 aggregate events; UINT64_MAX means interruptible infinite. */
   enum helios_e1_wait_wake (*wait_event)(void *ctx, uint64_t remaining_ns);
};

static inline enum helios_e1_wait_state
helios_e1_wait_run(const struct helios_e1_wait_ops *ops, uint64_t timeout_ns)
{
   const uint64_t start = ops->now_ns(ops->ctx);
   for (;;) {
      enum helios_e1_wait_state state = ops->observe(ops->ctx);
      if (state != HELIOS_E1_WAIT_PENDING) return state;
      if (timeout_ns == 0) return HELIOS_E1_WAIT_TIMEOUT;

      const uint64_t now = ops->now_ns(ops->ctx);
      const uint64_t elapsed = now - start;
      if (timeout_ns != UINT64_MAX && elapsed >= timeout_ns) {
         state = ops->observe(ops->ctx); /* final consistent snapshot */
         return state == HELIOS_E1_WAIT_PENDING ? HELIOS_E1_WAIT_TIMEOUT : state;
      }
      if (!ops->arm(ops->ctx)) {
         ops->disarm(ops->ctx); /* also unwinds a partial arm */
         return HELIOS_E1_WAIT_ERROR;
      }
      state = ops->observe(ops->ctx);
      if (state != HELIOS_E1_WAIT_PENDING) {
         ops->disarm(ops->ctx);
         return state;
      }
      const uint64_t armed_now = ops->now_ns(ops->ctx);
      const uint64_t armed_elapsed = armed_now - start;
      const uint64_t remaining = timeout_ns == UINT64_MAX ? UINT64_MAX :
         armed_elapsed >= timeout_ns ? 0 : timeout_ns - armed_elapsed;
      const enum helios_e1_wait_wake wake = remaining == 0 ? HELIOS_E1_WAKE_TIMEOUT :
         ops->wait_event(ops->ctx, remaining);
      ops->disarm(ops->ctx);
      state = ops->observe(ops->ctx); /* event is a hint, never a result */
      if (state != HELIOS_E1_WAIT_PENDING) return state;
      if (wake == HELIOS_E1_WAKE_FAILED) return HELIOS_E1_WAIT_ERROR;
      if (wake == HELIOS_E1_WAKE_TIMEOUT) {
         const uint64_t checked_now = ops->now_ns(ops->ctx);
         if (timeout_ns != UINT64_MAX && checked_now - start >= timeout_ns) return HELIOS_E1_WAIT_TIMEOUT;
      }
   }
}
#endif
