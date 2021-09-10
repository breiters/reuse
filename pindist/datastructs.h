#pragma once

#include <string>
#include <vector>
// #include "pin.H"

#define DATASTRUCT_UNKNOWN -1

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

  inline void print(void) { }

private:
};

extern void register_datastruct(DatastructInfo &info);
extern int datstruct_num(Addr addr);

extern std::vector<DatastructInfo> g_datastructs;