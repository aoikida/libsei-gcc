#ifndef _ASCO_DEBUG_H_
#define _ASCO_DEBUG_H_

#if DEBUG >= 2
#define DLOG2(...) printf(__VA_ARGS__)
#else
#define DLOG2(...)
#endif

#if DEBUG >= 3
#define DLOG3(...) printf(__VA_ARGS__)
#else
#define DLOG3(...)
#endif

#endif /* _ASCO_DEBUG_H_ */
