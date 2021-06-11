//  Replace an original function with a custom function defined in the tool using
//  probes.  The replacement function has a different signature from that of the
//  original replaced function.

#include "ds.h"

// std::vector<datastruct_info> g_datastructs;
// extern BucketContainer g_bucket_container;

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
        /*g_bucket_container.*/register_datastruct(info);
        info.print();
    }

    return info.address;
}

#if 0
VOID *NewPosixMemalign(FP_MALLOC orgFuncptr, UINT32 arg0, ADDRINT returnIp)
{
    datastruct_info info;

    // Call the relocated entry point of the original (replaced) routine.
    info.address = orgFuncptr(arg0);

    info.nbytes = arg0;
    info.allocator = "posix memalign";

    PIN_LockClient();
    LEVEL_PINCLIENT::PIN_GetSourceLocation(ADDRINT(returnIp), &info.col, &info.line, &info.file_name);
    PIN_UnlockClient();

    if (info.file_name.length() > 0) {
        g_datastructs.push_back(info);
        g_datastructs.back().print();
    }

    return info.address;
}
#endif

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
                                            PIN_PARG(int),
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
}
