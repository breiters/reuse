#include "bucket.h"
#include "cachesim.h"
#include "datastructs.h"
#include "memoryblock.h"

#include <cassert>

#define CSV_FORMAT "%s,%p,%zu,%d,%lu,%s,%u,%lu,%lu,%lu\n"

extern std::list<MemoryBlock *> g_stack;
extern std::string g_application_name;

static FILE *csv_out;
static const char *csv_header =
    "region,datastruct,nbytes,line,ds_total_access_count,file_name,min,"
    "access_count,datastruct_access_count,datastruct_access_exclusive\n";
static bool is_print = false;

Bucket::Bucket(int m) {
  aCount = 0;
  min = m;
  marker = g_stack.end();
}

void Bucket::add_sub(const Bucket &add, const Bucket &sub) {
  aCount += add.aCount - sub.aCount;
  for (size_t ds = 0; ds < ds_aCount.size(); ds++) {
    ds_aCount[ds] = add.ds_aCount[ds] - sub.ds_aCount[ds];
    ds_aCount_excl[ds] = add.ds_aCount_excl[ds] - sub.ds_aCount_excl[ds];
  }
}

void Bucket::register_datastruct() {
  ds_aCount.push_back(0);
  ds_aCount_excl.push_back(0);

  assert(g_datastructs.size() > 0);

  // push back a dummy marker
  ds_markers.push_back(g_cachesims[g_datastructs.size() - 1].stack().end());
}

void Bucket::print_csv(const char *region) {

  if (!is_print) {
    constexpr size_t FILENAME_SIZE = 256;
    char csv_filename[FILENAME_SIZE];
    snprintf(csv_filename, FILENAME_SIZE, "pindist.%s.csv",
             g_application_name.c_str());
    csv_out = fopen(csv_filename, "w");
    is_print = true;
    fprintf(csv_out, "%s", csv_header);
  }

  fprintf(csv_out, CSV_FORMAT, region, (void *)0x0, (size_t)0, 0, 0UL, "main",
          min, aCount, 0UL, 0UL);

  int ds_num = 0;
  for (auto &ds : g_datastructs) {
    fprintf(csv_out, CSV_FORMAT, region, ds.address, ds.nbytes, ds.line,
            ds.access_count, ds.file_name.c_str(), min, aCount,
            ds_aCount[ds_num], ds_aCount_excl[ds_num]);
    ds_num++;
  }

  fflush(csv_out);
}
