#ifndef _LIB_HASH_H_
#define _LIB_HASH_H_

#include <stdint.h>     /* defines uint32_t etc */
#include <unistd.h>     /* size_t */

#ifdef __APPLE__
/* OS X: defines __LITTLE_ENDIAN__ or __BIG_ENDIAN__ */
#include <libkern/OSByteOrder.h> 
#else
#include <endian.h>     /* attempt to define endianness */
#endif


#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/*
 * My best guess at if you are big-endian or little-endian.  This may
 * need adjustment.
 */
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
     __BYTE_ORDER == __LITTLE_ENDIAN) || \
    (defined(i386) || defined(__i386__) || defined(__i486__) || \
     defined(__i586__) || defined(__i686__) || defined(vax) || \
     defined(MIPSEL) || defined(__LITTLE_ENDIAN__))
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
       __BYTE_ORDER == __BIG_ENDIAN) || \
      (defined(sparc) || defined(POWERPC) || defined(mc68000) || \
       defined(sel) || defined(__LITTLE_ENDIAN__))
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 0
#endif

uint32_t hash(const void *key, size_t length, uint32_t initval);

#endif // _LIB_HASH_H_
 
