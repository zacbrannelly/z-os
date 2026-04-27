#pragma once

#include <openlibm.h>
#include "../string.h"
#include "../memory.h"
#include "../kmalloc.h"
#include "../assert.h"

typedef unsigned char   stbtt_uint8;
typedef signed   char   stbtt_int8;
typedef unsigned short  stbtt_uint16;
typedef signed   short  stbtt_int16;
typedef unsigned int    stbtt_uint32;
typedef signed   int    stbtt_int32;

typedef char stbtt__check_size32[sizeof(stbtt_int32)==4 ? 1 : -1];
typedef char stbtt__check_size16[sizeof(stbtt_int16)==2 ? 1 : -1];

#define STBTT_ifloor(x)   ((int) floor(x))
#define STBTT_iceil(x)    ((int) ceil(x))

#define STBTT_sqrt(x)      sqrt(x)
#define STBTT_pow(x,y)     pow(x,y)

#define STBTT_fmod(x,y)    fmod(x,y)

#define STBTT_cos(x)       cos(x)
#define STBTT_acos(x)      acos(x)

#define STBTT_fabs(x)      fabs(x)

#define STBTT_malloc(x,u)  ((void)(u),kmalloc(x))
#define STBTT_free(x,u)    ((void)(u),kfree(x))

#define STBTT_assert(x)    assert(x)

#define STBTT_strlen(x)    strlen(x)
#define STBTT_memcpy       memory_copy
#define STBTT_memset       memory_set
