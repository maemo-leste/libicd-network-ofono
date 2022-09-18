#ifndef _LOG_H_
#define _LOG_H_

#include <icd/support/icd_log.h>

#define OFONO_DEBUG(fmt, ...) ILOG_DEBUG(("[OFONO] "fmt), ##__VA_ARGS__)
#define OFONO_INFO(fmt, ...) ILOG_INFO(("[OFONO] " fmt), ##__VA_ARGS__)
#define OFONO_WARN(fmt, ...) ILOG_WARN(("[OFONO] %s.%d:" fmt), __func__, __LINE__, ##__VA_ARGS__)
#define OFONO_ERR(fmt, ...) ILOG_ERR(("[OFONO] %s.%d:" fmt), __func__, __LINE__, ##__VA_ARGS__)
#define OFONO_CRIT(fmt, ...) ILOG_CRIT(("[OFONO] %s.%d:" fmt), __func__, __LINE__, ##__VA_ARGS__)

#define OFONO_ENTER OFONO_DEBUG("> %s", __func__);
#define OFONO_EXIT OFONO_DEBUG("< %s", __func__);
#endif /* _LOG_H */
