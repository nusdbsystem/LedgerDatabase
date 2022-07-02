#ifndef _LIB_VIEWSTAMP_H_
#define _LIB_VIEWSTAMP_H_

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

typedef uint64_t view_t;
typedef uint64_t opnum_t;

struct viewstamp_t
{
    view_t view;
    opnum_t opnum;

    viewstamp_t() : view(0), opnum(0) {}
    viewstamp_t(view_t view, opnum_t opnum) : view(view), opnum(opnum) {}
};

#define FMT_VIEW "%" PRIu64
#define FMT_OPNUM "%" PRIu64

#define FMT_VIEWSTAMP "<" FMT_VIEW "," FMT_OPNUM ">"
#define VA_VIEWSTAMP(x) x.view, x.opnum

static inline int
Viewstamp_Compare(viewstamp_t a, viewstamp_t b)
{
    if (a.view < b.view) return -1;
    if (a.view > b.view) return 1;
    if (a.opnum < b.opnum) return -1;
    if (a.opnum > b.opnum) return 1;
    return 0;
}

inline bool operator==(const viewstamp_t& lhs, const viewstamp_t& rhs){ return Viewstamp_Compare(lhs,rhs) == 0; }
inline bool operator!=(const viewstamp_t& lhs, const viewstamp_t& rhs){return !operator==(lhs,rhs);}
inline bool operator< (const viewstamp_t& lhs, const viewstamp_t& rhs){ return Viewstamp_Compare(lhs,rhs) < 0; }
inline bool operator> (const viewstamp_t& lhs, const viewstamp_t& rhs){return  operator< (rhs,lhs);}
inline bool operator<=(const viewstamp_t& lhs, const viewstamp_t& rhs){return !operator> (lhs,rhs);}
inline bool operator>=(const viewstamp_t& lhs, const viewstamp_t& rhs){return !operator< (lhs,rhs);}

#endif  /* _LIB_VIEWSTAMP_H_ */
