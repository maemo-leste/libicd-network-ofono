#include <dbus/dbus.h>
#include <glib.h>

gboolean dbus_helper_append_property(DBusMessageIter *iter, const char *property, int type, void *value);
int dbus_helper_read_basic_dict_property(DBusMessageIter *iter, const char **property, DBusBasicValue *value);
int dbus_helper_read_basic_property(DBusMessageIter *iter, const char **property, DBusBasicValue *value);

#define dbus_helper_is_basic_type(t) \
  ( \
  (t) == DBUS_TYPE_STRING || (t) == DBUS_TYPE_BOOLEAN || \
  (t) == DBUS_TYPE_BYTE || (t) == DBUS_TYPE_INT16 || \
  (t) == DBUS_TYPE_UINT16 || (t) == DBUS_TYPE_INT32 || \
  (t) == DBUS_TYPE_UINT32 || (t) == DBUS_TYPE_INT64 || \
  (t) == DBUS_TYPE_UINT64 || (t) == DBUS_TYPE_DOUBLE || \
  (t) == DBUS_TYPE_OBJECT_PATH || (t) == DBUS_TYPE_SIGNATURE || \
  (t) == DBUS_TYPE_UNIX_FD \
  )
