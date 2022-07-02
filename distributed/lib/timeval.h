#ifndef _LIB_TIMEVAL_H_
#define _LIB_TIMEVAL_H_

#include <sys/time.h>
#include <time.h>               /* strftime */
#include <stdlib.h>             /* malloc */
#include <stdio.h>              /* sprintf */
#include <string.h>

static inline struct timeval
timeval_sub(struct timeval a, struct timeval b) {
    struct timeval result;
        
    if (a.tv_usec < b.tv_usec) {
        result.tv_sec = a.tv_sec - b.tv_sec - 1;
        result.tv_usec = a.tv_usec + 1000000 - b.tv_usec;
    } else {
        result.tv_sec = a.tv_sec - b.tv_sec;
        result.tv_usec = a.tv_usec - b.tv_usec;
    }
    return result;
};

static inline bool
timeval_lessthan(struct timeval a, struct timeval b) {
    return ((a.tv_sec < b.tv_sec) ||
            ((a.tv_sec == b.tv_sec) &&
             (a.tv_usec < b.tv_usec)));
};

static inline struct timeval
Timeval_FromSecs(double secs)
{
    struct timeval res;
    res.tv_sec = (time_t)secs;
    res.tv_usec = (time_t) (secs - (long long)secs) * 1000000;
    return res;
}

#define FMT_TIMEVAL_ABS "%s"
#define XVA_TIMEVAL_ABS(t) Message_DFree(Timeval_FmtAbs(t))

static inline char *
Timeval_FmtAbs(struct timeval tv)
{
    static const int LEN = 32;
    char *buf = (char *)malloc(LEN);
    if (!buf)
        return NULL;
    strftime(buf, LEN, "%H:%M:%S", localtime(&tv.tv_sec));
    sprintf(buf + strlen(buf), ":%06ld", tv.tv_usec);
    return buf;
}

#define FMT_TIMEVAL_DIFF "%ld.%06ld"
#define VA_TIMEVAL_DIFF(t) t.tv_sec, t.tv_usec

#endif
