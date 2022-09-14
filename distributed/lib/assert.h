#ifndef _LIB_ASSERT_H_
#define _LIB_ASSERT_H_

/*
 * Assertion macros.
 *
 * Currently these mostly just wrap the standard C assert but
 * eventually they should tie in better with the logging framework.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "distributed/lib/message.h"

#define ASSERT(x) Assert(x)

// XXX These should output the expected and actual values in addition
// to failing.
#define ASSERT_EQ(x, y) Assert(x == y)
#define ASSERT_LT(x, y) Assert(x < y)
#define ASSERT_GT(x, y) Assert(x > y)
#define ASSERT_LE(x, y) Assert(x <= y)
#define ASSERT_GE(x, y) Assert(x >= y)

#define NOT_REACHABLE() do {                                            \
        fprintf(stderr, "NOT_REACHABLE point reached: %s, line %d\n",   \
                __FILE__, __LINE__);                                    \
        abort();                                                        \
    } while (0)

#define NOT_IMPLEMENTED() do {                                          \
        fprintf(stderr, "NOT_IMPLEMENTED point reached: %s, line %d\n", \
                __FILE__, __LINE__);                                    \
        abort();                                                        \
} while (0)


#endif  /* _LIB_ASSERT_H */
