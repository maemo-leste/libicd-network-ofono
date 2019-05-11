#include <ofono/dbus.h>
#include "notifier.h"

gboolean ofono_modem_register(const char *path, notify_fn cb, gpointer user_data);
void ofono_modem_close(const char *path, notify_fn cb, gpointer user_data);

typedef void (*ofono_modem_set_fn)(gboolean success, gpointer user_data);

gboolean ofono_modem_set_power(const gchar *path, dbus_bool_t on, ofono_modem_set_fn cb, gpointer user_data);
gboolean ofono_modem_set_online(const char *path, dbus_bool_t on, ofono_modem_set_fn cb, gpointer user_data);
