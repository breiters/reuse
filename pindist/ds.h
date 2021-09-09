#pragma once

#include "pin.H"
#include <iostream>
#include <string>

#define DATASTRUCT_UNKNOWN -1

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::flush;
using std::hex;

struct datastruct_info {
  VOID *address;
  size_t nbytes;
  INT32 col;
  INT32 line;
  unsigned long access_count = 0;
  bool is_active;
  std::string allocator;
  std::string file_name;

  inline void print(void) {
    cout << "============================\n";
    cout << "bytes allocated by " << allocator << "(): " << nbytes << '\n';
    cout << "in: " << file_name << " line: ?" << line << "? col: " << col
         << '\n';
    cout << "located at address: " << hex << address << dec << '\n';
    cout << "============================\n";
  }
};

extern VOID ImageLoad(IMG img, VOID *v);
