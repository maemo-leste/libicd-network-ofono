#include <libofono/notifier.h>

enum ofono_mce_device_mode
{
  OFONO_MCE_DEVICE_MODE_INVALID,
  OFONO_MCE_DEVICE_MODE_NORMAL,
  OFONO_MCE_DEVICE_MODE_OFFLINE
};

enum ofono_mce_device_mode ofono_mce_get_device_mode(void);
gboolean ofono_mce_device_mode_register(ofono_notify_fn cb, gpointer user_data);
void ofono_mce_device_mode_close(ofono_notify_fn cb, gpointer user_data);
