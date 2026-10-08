#include "vn_renderer_helios_green_b_wait.h"
#include <assert.h>
#include <stdint.h>

struct fixture {
   uint64_t now;
   unsigned observed, armed, disarmed, waited;
   unsigned complete_at_observe, error_at_observe;
   uint64_t wait_advance;
   uint64_t first_budget, second_budget;
};
static enum helios_e1_wait_state observe(void *ctx)
{
   struct fixture *f = ctx;
   f->observed++;
   if (f->error_at_observe && f->observed >= f->error_at_observe)
      return HELIOS_E1_WAIT_ERROR;
   if (f->complete_at_observe && f->observed >= f->complete_at_observe)
      return HELIOS_E1_WAIT_COMPLETE;
   return HELIOS_E1_WAIT_PENDING;
}
static int arm(void *ctx)
{
   ((struct fixture *)ctx)->armed++;
   return 1;
}
static void disarm(void *ctx) { ((struct fixture *)ctx)->disarmed++; }
static uint64_t now(void *ctx) { return ((struct fixture *)ctx)->now; }
static enum helios_e1_wait_wake wait_event(void *ctx, uint64_t budget)
{
   struct fixture *f = ctx;
   f->waited++;
   if (f->waited == 1) f->first_budget = budget;
   if (f->waited == 2) f->second_budget = budget;
   f->now += f->wait_advance;
   return HELIOS_E1_WAKE_EVENT;
}
static struct helios_e1_wait_ops ops(struct fixture *f)
{
   return (struct helios_e1_wait_ops) {
      .ctx = f, .observe = observe, .arm = arm, .disarm = disarm,
      .now_ns = now, .wait_event = wait_event,
   };
}
int main(void)
{
   struct fixture f = { .complete_at_observe = 1 };
   struct helios_e1_wait_ops call = ops(&f);
   assert(helios_e1_wait_run(&call, 100) == HELIOS_E1_WAIT_COMPLETE);
   assert(f.armed == 0 && f.waited == 0);

   f = (struct fixture) { .complete_at_observe = 2 };
   call = ops(&f);
   assert(helios_e1_wait_run(&call, 100) == HELIOS_E1_WAIT_COMPLETE);
   assert(f.armed == 1 && f.disarmed == 1 && f.waited == 0);

   f = (struct fixture) { .complete_at_observe = 4, .wait_advance = 30 };
   call = ops(&f);
   assert(helios_e1_wait_run(&call, 100) == HELIOS_E1_WAIT_COMPLETE);
   assert(f.first_budget == 100 && f.armed == f.disarmed);

   f = (struct fixture) { .complete_at_observe = 6, .wait_advance = 30 };
   call = ops(&f);
   assert(helios_e1_wait_run(&call, 100) == HELIOS_E1_WAIT_COMPLETE);
   assert(f.first_budget == 100 && f.second_budget == 70);

   f = (struct fixture) { .wait_advance = 100 };
   call = ops(&f);
   assert(helios_e1_wait_run(&call, 100) == HELIOS_E1_WAIT_TIMEOUT);
   assert(f.observed >= 3 && f.armed == f.disarmed);

   f = (struct fixture) { .error_at_observe = 3, .wait_advance = 10 };
   call = ops(&f);
   assert(helios_e1_wait_run(&call, 100) == HELIOS_E1_WAIT_ERROR);
   assert(f.armed == f.disarmed);

   f = (struct fixture) { .wait_advance = 40, .error_at_observe = 4 };
   call = ops(&f);
   assert(helios_e1_wait_run(&call, UINT64_MAX) == HELIOS_E1_WAIT_ERROR);
   assert(f.first_budget == UINT64_MAX);
   f = (struct fixture) { .now = UINT64_MAX - 50, .wait_advance = 70, .complete_at_observe = 100 };
   call = ops(&f);
   assert(helios_e1_wait_run(&call, 100) == HELIOS_E1_WAIT_TIMEOUT);
   assert(f.second_budget == 30); /* elapsed includes wrap, no renewed budget */
   return 0;
}
