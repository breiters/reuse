//  Replace an original function with a custom function defined in the tool using
//  probes.  The replacement function has a different signature from that of the
//  original replaced function.

#include "ds.h"

extern void register_datastruct(datastruct_info &info);

// This is the replacement routine.
VOID *NewMalloc(FP_MALLOC orgFuncptr, UINT32 arg0, ADDRINT returnIp)
{
    datastruct_info info;

    // Call the relocated entry point of the original (replaced) routine.
    info.address = orgFuncptr(arg0);

    info.nbytes = arg0;
    info.is_active = true;
    info.allocator = "malloc";

    PIN_LockClient();
    LEVEL_PINCLIENT::PIN_GetSourceLocation(ADDRINT(returnIp), &info.col, &info.line, &info.file_name);
    PIN_UnlockClient();

    // TODO: case file_name length == 0
    if (info.file_name.length() > 0) {
        register_datastruct(info);
        // info.print();
    }

    return info.address;
}

// void *aligned_alloc(size_t alignment, size_t size);
typedef void *(*fp_aligned_alloc)(size_t, size_t);

VOID *NewAlignedAlloc(fp_aligned_alloc orgFuncptr, UINT64 arg0, UINT64 arg1, ADDRINT returnIp)
{
    datastruct_info info;

    // Call the relocated entry point of the original (replaced) routine.
    VOID *ret = orgFuncptr(arg0, arg1);
    info.address = ret;

    info.nbytes = arg1;
    info.is_active = true;
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

typedef int(*fp_posix_memalign)(void **, size_t, size_t);
// typedef int(*fp_posixmemalign)(void **, int, int);

// int NewPosixMemalign(fp_posixmemalign orgFuncptr, void **memptr, size_t arg0, size_t arg1, ADDRINT returnIp)
// INT32 NewPosixMemalign(fp_posixmemalign orgFuncptr, VOID **memptr, UINT32 arg0, UINT32 arg1, ADDRINT returnIp)
INT32 NewPosixMemalign(fp_posix_memalign orgFuncptr, VOID **memptr, UINT64 arg0, UINT64 arg1, ADDRINT returnIp)
{
    datastruct_info info;

    // printf("arg0: %lu arg1: %lu *memptr: %p\n", arg0, arg1, *memptr);
    // printf("arg0: %lu arg1: %lu *memptr: %p\n", arg0, arg1, *memptr);

    // Call the relocated entry point of the original (replaced) routine.
    int ret = orgFuncptr(memptr, arg0, arg1);
    info.address = *memptr;

    info.nbytes = arg1;
    info.is_active = true;
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

// Pin calls this function every time a new img is loaded.
// It is best to do probe replacement when the image is loaded,
// because only one thread knows about the image at this time.
//
VOID ImageLoad(IMG img, VOID *v)
{

    // See if malloc() is present in the image.  If so, replace it.
    //
    RTN rtn = RTN_FindByName(img, "malloc");


    if (RTN_Valid(rtn)) {
        cout << "Replacing malloc in " << IMG_Name(img) << endl;
        // Define a function prototype that describes the application routine
        // that will be replaced.
        //
        PROTO proto_malloc = PROTO_Allocate(PIN_PARG(void *),
                                            CALLINGSTD_DEFAULT,
                                            "malloc",
                                            PIN_PARG(size_t),
                                            // PIN_PARG(int),
                                            PIN_PARG_END());
        // Replace the application routine with the replacement function.
        // Additional arguments have been added to the replacement routine.
        //
        RTN_ReplaceSignature(rtn,
                             AFUNPTR(NewMalloc),
                             IARG_PROTOTYPE,
                             proto_malloc,
                             IARG_ORIG_FUNCPTR,
                             IARG_FUNCARG_ENTRYPOINT_VALUE,
                             0,
                             IARG_RETURN_IP,
                             IARG_END);
        // Free the function prototype.
        //
        PROTO_Free(proto_malloc);
    }

    rtn = RTN_FindByName(img, "posix_memalign");

    if (RTN_Valid(rtn)) {
        cout << "Replacing posix_memalign in " << IMG_Name(img) << endl;
        // Define a function prototype that describes the application routine
        // that will be replaced.
        //
        PROTO proto_malloc = PROTO_Allocate(PIN_PARG(int),
                                            CALLINGSTD_DEFAULT,
                                            "posix_memalign",
                                            PIN_PARG(void **),
                                            PIN_PARG(size_t),
                                            PIN_PARG(size_t),
                                            PIN_PARG_END());
        // Replace the application routine with the replacement function.
        // Additional arguments have been added to the replacement routine.
        //
        RTN_ReplaceSignature(rtn,
                             AFUNPTR(NewPosixMemalign),
                             IARG_PROTOTYPE,
                             proto_malloc,
                             IARG_ORIG_FUNCPTR,
                             IARG_FUNCARG_ENTRYPOINT_VALUE,
                             0,
                             IARG_FUNCARG_ENTRYPOINT_VALUE,
                             1,
                             IARG_FUNCARG_ENTRYPOINT_VALUE,
                             2,
                             IARG_RETURN_IP,
                             IARG_END);
        // Free the function prototype.
        //
        PROTO_Free(proto_malloc);
    }

    rtn = RTN_FindByName(img, "aligned_alloc");

    if (RTN_Valid(rtn)) {
        cout << "Replacing aligned_alloc in " << IMG_Name(img) << endl;
        // Define a function prototype that describes the application routine
        // that will be replaced.
        //
        PROTO proto_malloc = PROTO_Allocate(PIN_PARG(void *),
                                            CALLINGSTD_DEFAULT,
                                            "aligned_alloc",
                                            PIN_PARG(size_t),
                                            PIN_PARG(size_t),
                                            PIN_PARG_END());
        // Replace the application routine with the replacement function.
        // Additional arguments have been added to the replacement routine.
        //
        RTN_ReplaceSignature(rtn,
                             AFUNPTR(NewAlignedAlloc),
                             IARG_PROTOTYPE,
                             proto_malloc,
                             IARG_ORIG_FUNCPTR,
                             IARG_FUNCARG_ENTRYPOINT_VALUE,
                             0,
                             IARG_FUNCARG_ENTRYPOINT_VALUE,
                             1,
                             IARG_RETURN_IP,
                             IARG_END);
        // Free the function prototype.
        //
        PROTO_Free(proto_malloc);
    }
}
