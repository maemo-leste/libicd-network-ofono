#include <glib.h>
#include <dbus/dbus.h>

#include "dbus-helpers.h"

gboolean
dbus_helper_append_property(DBusMessageIter *iter, const char *property,
                            int type, void *value)
{
  DBusMessageIter variant;
  char *type_str;

  switch(type)
  {
    case DBUS_TYPE_BOOLEAN:
    {
      type_str = DBUS_TYPE_BOOLEAN_AS_STRING;
      break;
    }
    case DBUS_TYPE_BYTE:
    {
      type_str = DBUS_TYPE_BYTE_AS_STRING;
      break;
    }
    case DBUS_TYPE_STRING:
    {
      type_str = DBUS_TYPE_STRING_AS_STRING;
      break;
    }
    default:
      return FALSE;
  }

  dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &property);
  dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, type_str, &variant);
  dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, value);
  dbus_message_iter_close_container(iter, &variant);

  return TRUE;
}

int
dbus_helper_read_basic_dict_property(DBusMessageIter *iter,
                                     const char **property,
                                     DBusBasicValue *value)
{
  DBusMessageIter dict;
  int rv = DBUS_TYPE_INVALID;

  dbus_message_iter_recurse(iter, &dict);

  if (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_STRING)
  {
    dbus_message_iter_get_basic(&dict, property);
    dbus_message_iter_next(&dict);

    if (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_VARIANT)
    {
      DBusMessageIter variant;

      dbus_message_iter_recurse(&dict, &variant);
      rv = dbus_message_iter_get_arg_type(&variant);

      /* extend to what is needed */
      if (rv == DBUS_TYPE_STRING || rv == DBUS_TYPE_BOOLEAN ||
          rv == DBUS_TYPE_BYTE || rv == DBUS_TYPE_INT16 ||
          rv == DBUS_TYPE_UINT16 || rv == DBUS_TYPE_INT32 ||
          rv == DBUS_TYPE_UINT32 || rv == DBUS_TYPE_INT64 ||
          rv == DBUS_TYPE_UINT64 || rv == DBUS_TYPE_DOUBLE ||
          rv == DBUS_TYPE_OBJECT_PATH || rv == DBUS_TYPE_SIGNATURE ||
          rv == DBUS_TYPE_UNIX_FD)
      {
        dbus_message_iter_get_basic(&variant, value);
      }
    }
  }

  return rv;
}

int
dbus_helper_read_basic_property(DBusMessageIter *iter, const char **property,
                                DBusBasicValue *value)
{
  int rv = DBUS_TYPE_INVALID;

  if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING)
  {
    dbus_message_iter_get_basic(iter, property);
    dbus_message_iter_next(iter);

    if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_VARIANT)
    {
      DBusMessageIter variant;

      dbus_message_iter_recurse(iter, &variant);
      rv = dbus_message_iter_get_arg_type(&variant);

      /* extend to what is needed */
      if (dbus_helper_is_basic_type(rv))
        dbus_message_iter_get_basic(&variant, value);
    }
  }

  return rv;
}
