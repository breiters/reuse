#pragma once

#include <string>
#include <vector>

enum DATASTRUCTS { RD_NO_DATASTRUCT = -1 };

class DatastructInfo {
public:
  void *address;
  size_t nbytes;
  int col;
  int line;
  unsigned long access_count = 0;
  // bool is_active;
  std::string allocator;
  std::string file_name;

  /*
    inline void print(void) {
      cout << "============================\n";
      cout << "bytes allocated by " << allocator << "(): " << nbytes << '\n';
      cout << "in: " << file_name << " line: ?" << line << "? col: " << col
           << '\n';
      cout << "located at address: " << hex << address << dec << '\n';
      cout << "============================\n";
    }
  */

  inline void print(void) {}

private:
};

typedef void *Addr;
extern void register_datastruct(DatastructInfo &info);
extern int datastruct_num(Addr addr);
extern std::vector<int> datstruct_nums(Addr addr);
extern std::vector<DatastructInfo> g_datastructs;
extern std::vector<std::vector<int>> g_csindices_of_ds;