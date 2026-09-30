#include "vn_renderer_helios_carrier_v2.h"
#include <assert.h>
#include <string.h>

static struct helios_carrier_v2_record
valid_record(void)
{
   struct helios_carrier_v2_record record = { 0 };
   record.magic = HELIOS_P06_SECTION_MAGIC;
   record.version = HELIOS_P06_PRODUCTION_SECTION_VERSION;
   record.size = sizeof(record);
   record.carrier_id[0] = 0x42;
   return record;
}

int
main(void)
{
   struct helios_carrier_v2_record record = valid_record();
   struct helios_carrier_v2_snapshot snapshot;
   assert(helios_carrier_has_magic(&record));
   assert(helios_carrier_classify(&record, &snapshot) == HELIOS_CARRIER_VALID);
   assert(helios_carrier_read_snapshot(&record, &snapshot));
   assert(snapshot.carrier_id[0] == 0x42);
   assert(helios_carrier_observe(&snapshot, 1) == HELIOS_CARRIER_PENDING);

   record.completed_value = 8;
   assert(helios_carrier_read_snapshot(&record, &snapshot));
   assert(helios_carrier_observe(&snapshot, 8) == HELIOS_CARRIER_COMPLETE);
   record.terminal_error_value = 7;
   record.terminal_response_type = 0x1200;
   assert(helios_carrier_read_snapshot(&record, &snapshot));
   assert(helios_carrier_observe(&snapshot, 7) == HELIOS_CARRIER_ERROR);

   record.sequence = 1;
   assert(!helios_carrier_read_snapshot(&record, &snapshot));
   record = valid_record();
   record.magic ^= 1;
   assert(!helios_carrier_has_magic(&record));
   assert(helios_carrier_classify(&record, &snapshot) == HELIOS_CARRIER_FOREIGN);
   assert(!helios_carrier_read_snapshot(&record, &snapshot));
   record = valid_record();
   record.version++;
   assert(helios_carrier_classify(&record, &snapshot) == HELIOS_CARRIER_INVALID);
   assert(!helios_carrier_read_snapshot(&record, &snapshot));
   record = valid_record();
   record.size--;
   assert(!helios_carrier_read_snapshot(&record, &snapshot));
   record = valid_record();
   memset(record.carrier_id, 0, sizeof(record.carrier_id));
   assert(!helios_carrier_read_snapshot(&record, &snapshot));
   return 0;
}
