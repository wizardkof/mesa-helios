/* P06 E1 v2 read-only Section record. Mirror of helios_protocol at
 * 42d7e7341a8b2390d9a27695664693caffb8ca2b. */
#ifndef VN_RENDERER_HELIOS_CARRIER_V2_H
#define VN_RENDERER_HELIOS_CARRIER_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HELIOS_P06_SECTION_MAGIC UINT64_C(0x504636534543544e)
#define HELIOS_P06_PRODUCTION_SECTION_VERSION 2u

struct helios_carrier_v2_record {
   uint64_t magic;
   uint32_t version;
   uint32_t size;
   uint8_t carrier_id[16];
   uint64_t sequence;
   uint64_t completed_value;
   uint64_t terminal_error_value;
   uint32_t terminal_response_type;
   uint32_t reserved_tail;
};

_Static_assert(sizeof(struct helios_carrier_v2_record) == 64, "v2 record size");
_Static_assert(offsetof(struct helios_carrier_v2_record, carrier_id) == 16,
               "v2 identity offset");
_Static_assert(offsetof(struct helios_carrier_v2_record, sequence) == 32,
               "v2 sequence offset");
_Static_assert(offsetof(struct helios_carrier_v2_record, completed_value) == 40,
               "v2 completion offset");
_Static_assert(offsetof(struct helios_carrier_v2_record, terminal_error_value) == 48,
               "v2 error value offset");
_Static_assert(offsetof(struct helios_carrier_v2_record, terminal_response_type) == 56,
               "v2 error response offset");

struct helios_carrier_v2_snapshot {
   uint8_t carrier_id[16];
   uint64_t completed_value;
   uint64_t terminal_error_value;
   uint32_t terminal_response_type;
};

enum helios_carrier_observation {
   HELIOS_CARRIER_PENDING,
   HELIOS_CARRIER_COMPLETE,
   HELIOS_CARRIER_ERROR,
};

enum helios_carrier_classification {
   HELIOS_CARRIER_FOREIGN,
   HELIOS_CARRIER_VALID,
   HELIOS_CARRIER_INVALID,
};

/* Caller maps at least 64 bytes before reading. Signature detection fixes the
 * import route before validation, so malformed E1 never falls back to WDDM. */
static inline bool
helios_carrier_has_magic(const volatile struct helios_carrier_v2_record *record)
{
   return record && record->magic == HELIOS_P06_SECTION_MAGIC;
}

static inline bool
helios_carrier_read_snapshot(const volatile struct helios_carrier_v2_record *record,
                             struct helios_carrier_v2_snapshot *out)
{
   if (!record || !out)
      return false;
   for (unsigned attempt = 0; attempt < 8; attempt++) {
      const uint64_t begin = __atomic_load_n(&record->sequence, __ATOMIC_ACQUIRE);
      if (begin & 1)
         continue;
      __atomic_thread_fence(__ATOMIC_SEQ_CST);
      const uint64_t magic = record->magic;
      const uint32_t version = record->version;
      const uint32_t size = record->size;
      struct helios_carrier_v2_snapshot next;
      bool id_nonzero = false;
      for (unsigned i = 0; i < sizeof(next.carrier_id); i++) {
         next.carrier_id[i] = record->carrier_id[i];
         id_nonzero |= next.carrier_id[i] != 0;
      }
      next.completed_value = record->completed_value;
      next.terminal_error_value = record->terminal_error_value;
      next.terminal_response_type = record->terminal_response_type;
      const uint32_t reserved_tail = record->reserved_tail;
      __atomic_thread_fence(__ATOMIC_SEQ_CST);
      const uint64_t end = __atomic_load_n(&record->sequence, __ATOMIC_ACQUIRE);
      if (begin != end || (end & 1))
         continue;
      if (magic != HELIOS_P06_SECTION_MAGIC ||
          version != HELIOS_P06_PRODUCTION_SECTION_VERSION ||
          size != sizeof(*record) || !id_nonzero || reserved_tail ||
          ((next.terminal_response_type == 0) !=
           (next.terminal_error_value == 0)))
         return false;
      *out = next;
      return true;
   }
   return false;
}

static inline enum helios_carrier_classification
helios_carrier_classify(const volatile struct helios_carrier_v2_record *record,
                        struct helios_carrier_v2_snapshot *snapshot)
{
   if (!helios_carrier_has_magic(record))
      return HELIOS_CARRIER_FOREIGN;
   return helios_carrier_read_snapshot(record, snapshot) ?
      HELIOS_CARRIER_VALID : HELIOS_CARRIER_INVALID;
}

static inline enum helios_carrier_observation
helios_carrier_observe(const struct helios_carrier_v2_snapshot *snapshot,
                       uint64_t target)
{
   if (snapshot->terminal_response_type &&
       snapshot->terminal_error_value <= target)
      return HELIOS_CARRIER_ERROR;
   if (snapshot->completed_value >= target)
      return HELIOS_CARRIER_COMPLETE;
   return HELIOS_CARRIER_PENDING;
}

#endif
