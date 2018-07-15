/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <ch.h>
#include <stdint.h>

#define STRINGIZE2(x)   #x
#define STRINGIZE(x)    STRINGIZE2(x)
#define MAKE_ASSERT_MSG() __FILE__ ":" STRINGIZE(__LINE__)

#if !CH_DBG_ENABLED
extern const char *dbg_panic_msg;
#endif

#define ASSERT(x) chDbgAssert((x), MAKE_ASSERT_MSG(), "")

#define ASSERT_ALWAYS(x)                                    \
    do {                                                    \
        if ((x) == 0) {                                     \
            dbg_panic_msg = MAKE_ASSERT_MSG();              \
            chSysHalt();                                    \
        }                                                   \
    } while (0)

uint64_t sysTimestampMicros(void);
