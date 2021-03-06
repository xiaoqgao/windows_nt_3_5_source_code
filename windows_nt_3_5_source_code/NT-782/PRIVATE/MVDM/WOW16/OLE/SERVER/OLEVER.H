#include <ver.h>

#ifndef DEBUG
#define OLEVER_DEBUG        0
#else
#define OLEVER_DEBUG        1
#endif


#ifndef OFFICIAL
#define OLEVER_PRIVATEBUILD 1
#else
#define OLEVER_PRIVATEBUILD 0
#endif


#ifndef FINAL
#define OLEVER_PRERELEASE   1
#else
#define OLEVER_PRERELEASE   0
#endif

#define OLEVER_FILEFLAGS    (OLEVER_PRIVATEBUILD|OLEVER_PRERELEASE|OLEVER_DEBUG)

#ifdef NT
#define OLEVER_FILEOS           VOS_WINDOWS32
#define OLEVER_FILEVERSION      1,03, 001
#define OLEVER_PRODUCTVERSION   3,10,0,043
#elseif PWIN
#define OLEVER_FILEOS           VOS_DOS_WINDOWS16
#define OLEVER_FILEVERSION      1,10
#define OLEVER_PRODUCTVERSION   3,10,0,043
#else
#define OLEVER_FILEOS           VOS_DOS_WINDOWS16
#define OLEVER_FILEVERSION      1,03, 001
#define OLEVER_PRODUCTVERSION   3,10,0,043
#endif

