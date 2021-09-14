/**
 *
 */

#include "imgload.h"
#include "bucket.h"
#include "datastructs.h"
#include "pin.H"
#include "region.h"

#include <iostream>
#include <string>
#include <vector>

std::string g_application_name;

//  Replace an original function with a custom function defined in the tool
//  using probes.  The replacement function has a different signature from that
//  of the original replaced function.

typedef VOID *(*FP_MALLOC)(size_t);
// This is the replacement routine.
VOID *NewMalloc(FP_MALLOC orgFuncptr, UINT32 arg0, ADDRINT returnIp) {
  DatastructInfo info;

  // Call the relocated entry point of the original (replaced) routine.
  info.address = orgFuncptr(arg0);

  info.nbytes = arg0;
  // info.is_active = true;
  info.allocator = "malloc";

  PIN_LockClient();
  LEVEL_PINCLIENT::PIN_GetSourceLocation(ADDRINT(returnIp), &info.col, &info.line, &info.file_name);
  PIN_UnlockClient();

  // TODO: case file_name length == 0 ?
  if (info.file_name.length() > 0) {
    register_datastruct(info);
    // info.print();
  }

  return info.address;
}

typedef void *(*fp_calloc)(size_t, size_t);
VOID *NewCalloc(fp_calloc orgFuncptr, UINT64 arg0, UINT64 arg1, ADDRINT returnIp) {
  DatastructInfo info;

  // Call the relocated entry point of the original (replaced) routine.
  info.address = orgFuncptr(arg0, arg1);

  info.nbytes = arg0 * arg1;
  // info.is_active = true;
  info.allocator = "calloc";

  PIN_LockClient();
  LEVEL_PINCLIENT::PIN_GetSourceLocation(ADDRINT(returnIp), &info.col, &info.line, &info.file_name);
  PIN_UnlockClient();

  if (info.file_name.length() > 0) {
    register_datastruct(info);
  }

  return info.address;
}

// void *aligned_alloc(size_t alignment, size_t size);
typedef void *(*fp_aligned_alloc)(size_t, size_t);

VOID *NewAlignedAlloc(fp_aligned_alloc orgFuncptr, UINT64 arg0, UINT64 arg1, ADDRINT returnIp) {
  DatastructInfo info;

  // Call the relocated entry point of the original (replaced) routine.
  VOID *ret = orgFuncptr(arg0, arg1);
  info.address = ret;

  info.nbytes = arg1;
  // info.is_active = true;
  info.allocator = "aligned_alloc";

  PIN_LockClient();
  LEVEL_PINCLIENT::PIN_GetSourceLocation(ADDRINT(returnIp), &info.col, &info.line, &info.file_name);
  PIN_UnlockClient();

  if (info.file_name.length() > 0) {
    register_datastruct(info);
    // info.print();
  }

  return ret;
}

typedef int (*fp_posix_memalign)(void **, size_t, size_t);

INT32 NewPosixMemalign(fp_posix_memalign orgFuncptr, VOID **memptr, UINT64 arg0, UINT64 arg1,
                       ADDRINT returnIp) {
  DatastructInfo info;

  // printf("arg0: %lu arg1: %lu *memptr: %p\n", arg0, arg1, *memptr);
  // printf("arg0: %lu arg1: %lu *memptr: %p\n", arg0, arg1, *memptr);

  // Call the relocated entry point of the original (replaced) routine.
  int ret = orgFuncptr(memptr, arg0, arg1);
  info.address = *memptr;

  info.nbytes = arg1;
  // info.is_active = true;
  info.allocator = "posix_memalign";

  PIN_LockClient();
  LEVEL_PINCLIENT::PIN_GetSourceLocation(ADDRINT(returnIp), &info.col, &info.line, &info.file_name);
  PIN_UnlockClient();

  if (info.file_name.length() > 0) {
    register_datastruct(info);
    // info.print();
  }

  return ret;
}

/**
 *   Replacement functions for region markers
 */
typedef void (*fp_pindist_start_stop)(char *);
VOID PINDIST_start_region_(char *region) {
  auto reg = g_regions.find(region);

  if (reg == g_regions.end()) {
    g_regions[region] = new Region(region);
    g_regions[region]->on_region_entry();
  } else {
    reg->second->on_region_entry();
  }
}

VOID New_PINDIST_start_region(fp_pindist_start_stop orgFuncptr, char *region, ADDRINT returnIp) {
  PINDIST_start_region_(region);
}

VOID PINDIST_stop_region_(char *region) {
  auto reg = g_regions.find(region);
  assert(reg != g_regions.end());
  if (reg != g_regions.end()) {
    reg->second->on_region_exit();
  }
}

VOID New_PINDIST_stop_region(fp_pindist_start_stop orgFuncptr, char *region, ADDRINT returnIp) {
  PINDIST_stop_region_(region);
}

// Pin calls this function every time a new img is loaded.
// It is best to do probe replacement when the image is loaded,
// because only one thread knows about the image at this time.
//
VOID ImageLoad(IMG img, VOID *v) {

  std::cout << IMG_Name(img) << std::endl;

  if (IMG_IsMainExecutable(img)) {
    std::size_t found = IMG_Name(img).rfind('/');
    if (found != std::string::npos) {
      g_application_name = IMG_Name(img).substr(found + 1);
    } else {
      g_application_name = IMG_Name(img);
    }
    std::cout << g_application_name << std::endl;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
      if (SEC_TYPE_EXEC != SEC_Type(sec))
        continue; // LEVEL_CORE::SEC_TYPE
      // printf("sec type: %d\n", (int)SEC_Type(sec));
      for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
        RTN_Open(rtn);
        // do not insert call for functions that begin with __ or that contain @
        // or ...
        printf("function: %s\n", RTN_Name(rtn).c_str());
        if ((RTN_Name(rtn).rfind('_', 0) == 0 && RTN_Name(rtn).rfind('Z', 1) != 1) // not mangled
            || RTN_Name(rtn).rfind("omp_", 0) == 0 ||
            RTN_Name(rtn).rfind("static_initialization_and_destruction") != std::string::npos ||
            RTN_Name(rtn).rfind('.', 0) == 0 || RTN_Name(rtn).rfind('@') != std::string::npos ||
            RTN_Name(rtn).compare("main") == 0 ||
            RTN_Name(rtn).compare("register_tm_clones") == 0 ||
            RTN_Name(rtn).compare("deregister_tm_clones") == 0 ||
            RTN_Name(rtn).compare("frame_dummy") == 0 ||
            RTN_Name(rtn).rfind("PINDIST_start_region") != std::string::npos ||
            RTN_Name(rtn).rfind("PINDIST_stop_region") != std::string::npos) {
          RTN_Close(rtn);
          continue;
        }
        // application functions to not instrument: // TODO: remove in
        // production
        if (RTN_Name(rtn).rfind("timer_") != std::string::npos ||
            RTN_Name(rtn).rfind("wtime") != std::string::npos ||
            RTN_Name(rtn).rfind("c_print_results") != std::string::npos ||
            RTN_Name(rtn).rfind("alloc_") != std::string::npos ||
            RTN_Name(rtn).rfind("verify") != std::string::npos) {
          RTN_Close(rtn);
          continue;
        }
        printf("Instrumenting function: %s\n", RTN_Name(rtn).c_str());
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)PINDIST_start_region_, IARG_PTR,
                       RTN_Name(rtn).c_str(), IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)PINDIST_stop_region_, IARG_PTR,
                       RTN_Name(rtn).c_str(), IARG_END);
        RTN_Close(rtn);
      }
    }
  }

  // See if malloc() is present in the image.  If so, replace it.
  //
  RTN rtn = RTN_FindByName(img, "malloc");

  if (RTN_Valid(rtn)) {
    std::cout << "Replacing malloc in " << IMG_Name(img) << std::endl;
    // Define a function prototype that describes the application routine
    // that will be replaced.
    //
    PROTO proto_malloc =
        PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT, "malloc", PIN_PARG(size_t),
                       // PIN_PARG(int),
                       PIN_PARG_END());
    // Replace the application routine with the replacement function.
    // Additional arguments have been added to the replacement routine.
    //
    RTN_ReplaceSignature(rtn, AFUNPTR(NewMalloc), IARG_PROTOTYPE, proto_malloc, IARG_ORIG_FUNCPTR,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_RETURN_IP, IARG_END);
    // Free the function prototype.
    //
    PROTO_Free(proto_malloc);
  }

  rtn = RTN_FindByName(img, "calloc");

  if (RTN_Valid(rtn)) {
    std::cout << "Replacing calloc in " << IMG_Name(img) << std::endl;

    PROTO proto_malloc = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT, "calloc",
                                        PIN_PARG(size_t), PIN_PARG(size_t), PIN_PARG_END());

    RTN_ReplaceSignature(rtn, AFUNPTR(NewCalloc), IARG_PROTOTYPE, proto_malloc, IARG_ORIG_FUNCPTR,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                         IARG_RETURN_IP, IARG_END);

    PROTO_Free(proto_malloc);
  }

  rtn = RTN_FindByName(img, "posix_memalign");

  if (RTN_Valid(rtn)) {
    std::cout << "Replacing posix_memalign in " << IMG_Name(img) << std::endl;

    PROTO proto_malloc =
        PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "posix_memalign", PIN_PARG(void **),
                       PIN_PARG(size_t), PIN_PARG(size_t), PIN_PARG_END());

    RTN_ReplaceSignature(rtn, AFUNPTR(NewPosixMemalign), IARG_PROTOTYPE, proto_malloc,
                         IARG_ORIG_FUNCPTR, IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                         IARG_RETURN_IP, IARG_END);

    PROTO_Free(proto_malloc);
  }

  rtn = RTN_FindByName(img, "aligned_alloc");

  if (RTN_Valid(rtn)) {
    std::cout << "Replacing aligned_alloc in " << IMG_Name(img) << std::endl;
    PROTO proto_malloc = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT, "aligned_alloc",
                                        PIN_PARG(size_t), PIN_PARG(size_t), PIN_PARG_END());

    RTN_ReplaceSignature(rtn, AFUNPTR(NewAlignedAlloc), IARG_PROTOTYPE, proto_malloc,
                         IARG_ORIG_FUNCPTR, IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_RETURN_IP, IARG_END);

    PROTO_Free(proto_malloc);
  }

  rtn = RTN_FindByName(img, "PINDIST_start_region");

  if (RTN_Valid(rtn)) {
    std::cout << "Replacing PINDIST_start_region in " << IMG_Name(img) << std::endl;

    PROTO proto_malloc = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT,
                                        "PINDIST_start_region", PIN_PARG(char *), PIN_PARG_END());

    RTN_ReplaceSignature(rtn, AFUNPTR(New_PINDIST_start_region), IARG_PROTOTYPE, proto_malloc,
                         IARG_ORIG_FUNCPTR, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_RETURN_IP,
                         IARG_END);

    PROTO_Free(proto_malloc);
  }

  rtn = RTN_FindByName(img, "PINDIST_stop_region");

  if (RTN_Valid(rtn)) {
    std::cout << "Replacing PINDIST_stop_region in " << IMG_Name(img) << std::endl;

    PROTO proto_malloc = PROTO_Allocate(PIN_PARG(void *), CALLINGSTD_DEFAULT, "PINDIST_stop_region",
                                        PIN_PARG(char *), PIN_PARG_END());

    RTN_ReplaceSignature(rtn, AFUNPTR(New_PINDIST_stop_region), IARG_PROTOTYPE, proto_malloc,
                         IARG_ORIG_FUNCPTR, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_RETURN_IP,
                         IARG_END);

    PROTO_Free(proto_malloc);
  }
}
