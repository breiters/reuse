#pragma once

#include "pin.H"
#include <iostream>
using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::flush;
using std::hex;

typedef VOID *(*FP_MALLOC)(size_t);

struct datastruct_info {
    VOID *address;
    size_t nbytes;
    bool is_active;
    INT32 col;
    INT32 line;
    std::string allocator;
    std::string file_name;
    void print(void)
    {
        cout << "============================\n";
        cout << "bytes allocated by " << allocator << "(): " << nbytes << '\n';
        cout << "in: " << file_name << " line: " << line << " col: " << col << '\n';
        cout << "located at address: " << hex << address << dec << '\n';
        cout << "============================\n";
    }
};