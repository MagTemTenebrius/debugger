#include "string_util.h"

#include <stdlib.h>


WCHAR *GetStringFromHandle(HANDLE handle) {
    WCHAR *wname = malloc (sizeof(WCHAR) * 200);

    GetFinalPathNameByHandle(handle, (LPSTR) wname, 200, FILE_NAME_NORMALIZED);
    return wname;
}