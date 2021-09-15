#pragma once

#include <string>
#include <vector>

enum DATASTRUCTS { RD_NO_DATASTRUCT = -1 };
typedef void *Addr;

class Datastruct {
public:
  void *address;
  size_t nbytes;
  int col;
  int line;
  unsigned long access_count = 0;
  // bool is_active;
  std::string allocator;
  std::string file_name;
  // std::vector<int> contained_in;

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

  static std::vector<Datastruct> datastructs;
  static std::vector<std::vector<int>> contained_indices;

  inline void print(void) {}

  static int datastruct_num(Addr addr);
  static void register_datastruct(Datastruct &ds);

private:
  static void combine(int ds_num, [[maybe_unused]] int level);
};
