#include <icd/support/icd_log.h>

#define OFONO_DEBUG(fmt, ...) ILOG_DEBUG(("[OFONO] "fmt), ##__VA_ARGS__)
#define OFONO_INFO(fmt, ...) ILOG_INFO(("[OFONO] " fmt), ##__VA_ARGS__)
#define OFONO_WARN(fmt, ...) ILOG_WARN(("[OFONO] " fmt), ##__VA_ARGS__)
#define OFONO_ERR(fmt, ...) ILOG_ERR(("[OFONO] " fmt), ##__VA_ARGS__)
#define OFONO_CRITICAL(fmt, ...) ILOG_CRITICAL(("[OFONO] " fmt), ##__VA_ARGS__)

#define OFONO_ENTER OFONO_DEBUG("> %s", __func__);
#define OFONO_EXIT OFONO_DEBUG("< %s", __func__);
