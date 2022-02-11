#pragma once

#include <string>
#include <vector>

enum DATASTRUCTS { RD_NO_DATASTRUCT = 0 };
typedef void *Addr;

class Datastruct {
public:
  Datastruct() : address{(void *)0x0}, nbytes{0UL}, col{0}, line{0} {};
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

  static std::vector<Datastruct> datastructs;
  static std::vector<std::vector<int>> indices_of;

  inline void print(void) {}

  static int datastruct_num(Addr addr);
  static void register_datastruct(Datastruct &ds);

private:
  static void combine(int ds_num, [[maybe_unused]] int level);
};
