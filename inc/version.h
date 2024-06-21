#define MAJOR_VERSION 0
#define MINOR_VERSION 1
#define PATCH_VERSION 0

typedef struct
{
   unsigned short int Major, Minor, Patch;
   unsigned int  GitTag;
   char GitCI[41], BuildDate[18], Name[50];
} SWV;
