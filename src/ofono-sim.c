#include <glib.h>

#include <string.h>

#include "ofono-sim.h"
#include "log.h"
#include "dbus-helpers.h"
#include "modem.h"

static GHashTable *sims = NULL;

static void
ofono_sim_notifier_notify(const char *path, gpointer data)
{
  GSList *notifiers;

  g_assert(sims !=NULL);

  notifiers = g_hash_table_lookup(sims, path);

  if (notifiers)
    notifier_notify(notifiers, data);
}

#if 0
static void
ofono_sim_set_property_cb(DBusPendingCall *pending, void *user_data)
{
  DBusMessage *reply;

  OFONO_ENTER

  if (pending)
  {
    reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);
  }

  if (pending && reply)
  {
    if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR)
    {
      OFONO_WARN("SetProperty returned '%s'",
                 dbus_message_get_error_name(reply));
    }

    dbus_message_unref(reply);
  }

  OFONO_EXIT
}

static gboolean
ofono_sim_set_property(const char *path, const char *property, int type,
                       void *value)
{
  DBusMessage *message;
  gboolean rv = FALSE;

  OFONO_ENTER

  message = dbus_message_new_method_call(OFONO_SERVICE, path,
                                         OFONO_SIM_MANAGER_INTERFACE,
                                         "SetProperty");

  if (message)
  {
    DBusMessageIter iter;

    dbus_message_iter_init_append(message, &iter);

    g_assert(dbus_helper_append_property(&iter, property, type, value) == TRUE);

    if (icd_dbus_send_system_mcall(message, -1,
                                   ofono_sim_set_property_cb, NULL))
    {
      rv = TRUE;
    }
    else
      OFONO_ERR("could not send 'SetProperty' method call");

    dbus_message_unref(message);
  }
  else
    OFONO_ERR("could not create 'SetProperty' method call");

  OFONO_EXIT

  return rv;
}
#endif
static gboolean
ofono_sim_property_changed(const char *path, DBusMessageIter *iter)
{
  property_changed pc;
  gboolean rv = FALSE;

  OFONO_ENTER

  if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING)
  {
    int type;
    dbus_message_iter_get_basic(iter, &pc.property);
    type = dbus_helper_read_basic_property(iter, &pc.property, &pc.val);

    if (type != DBUS_TYPE_INVALID)
    {
      if (dbus_helper_is_basic_type(type))
        ofono_sim_notifier_notify(path, &pc);

      rv = TRUE;
    }
  }

  OFONO_EXIT

  return rv;
}

static void
ofono_sim_get_properties_cb(DBusPendingCall *pending, void *user_data)
{
  DBusMessage *reply;
  gchar *path = user_data;

  OFONO_ENTER

  if (pending)
  {
    reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);
  }

  if (pending && reply)
  {
    if (dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_ERROR)
    {
      DBusMessageIter iter;

      dbus_message_iter_init(reply, &iter);

      if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY)
      {
        DBusMessageIter array_iter;

        dbus_message_iter_recurse(&iter, &array_iter);

        while (dbus_message_iter_get_arg_type(&array_iter) ==
               DBUS_TYPE_DICT_ENTRY)
        {
          DBusMessageIter dict_iter;

          dbus_message_iter_recurse(&array_iter, &dict_iter);
          ofono_sim_property_changed(path, &dict_iter);

          dbus_message_iter_next(&array_iter);
        }
      }
    }
    else
    {
      OFONO_WARN("GetProperties returned '%s'",
                 dbus_message_get_error_name(reply));
    }

    dbus_message_unref(reply);
  }

  g_free(path);

  OFONO_EXIT
}

static gboolean
ofono_sim_init(const char *path)
{
  DBusMessage *message;
  gboolean rv = FALSE;
  OFONO_ENTER

  message = dbus_message_new_method_call(OFONO_SERVICE, path,
                                         OFONO_SIM_MANAGER_INTERFACE,
                                         "GetProperties");

  if (message)
  {
    gchar *_path = g_strdup(path);

    if (icd_dbus_send_system_mcall(message, -1, ofono_sim_get_properties_cb,
                                   _path))
    {
      rv = TRUE;
    }
    else
    {
      g_free(_path);
      OFONO_ERR("could not send 'GetProperties' message");
    }

    dbus_message_unref(message);
  }
  else
    OFONO_ERR("could not create 'GetProperties' method call");

  OFONO_EXIT

  return rv;
}

static DBusHandlerResult
ofono_sim_filter(DBusConnection *connection, DBusMessage *message,
                 void *user_data)
{
  OFONO_ENTER

  if (dbus_message_is_signal(message, OFONO_SIM_MANAGER_INTERFACE,
                             "PropertyChanged"))
  {
    DBusMessageIter iter;

    if (dbus_message_iter_init(message, &iter))
      ofono_sim_property_changed(dbus_message_get_path(message), &iter);
    else
      OFONO_WARN("Invalid arguments for PropertyChanged signal");
  }

  OFONO_EXIT

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
ofono_sim_add_dbus_filter()
{
  return icd_dbus_connect_system_bcast_signal(
        OFONO_SIM_MANAGER_INTERFACE, ofono_sim_filter, NULL, NULL);
}

static void
ofono_sim_remove_dbus_filter()
{
  icd_dbus_disconnect_system_bcast_signal(
    OFONO_SIM_MANAGER_INTERFACE, ofono_sim_filter, NULL, NULL);
}

gboolean
ofono_sim_register(const char *path, notify_fn cb, gpointer user_data)
{
  gboolean rv = TRUE;
  GSList *notifiers;

  if (!sims)
    sims = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  notifiers = g_hash_table_lookup(sims, path);

  if (!notifiers)
  {
    if ((rv = ofono_sim_init(path)))
        rv = ofono_sim_add_dbus_filter();
  }

  if (rv)
  {
    notifier_register(&notifiers, cb, user_data);
    g_hash_table_insert(sims, g_strdup(path), notifiers);
  }

  return rv;
}

void
ofono_sim_close(const char *path, notify_fn cb, gpointer user_data)
{
  GSList *notifiers;

  g_assert(sims !=NULL);

  notifiers = g_hash_table_lookup(sims, path);

  if (notifiers)
  {
    notifier_close(&notifiers, cb, user_data);

    if (!notifiers)
    {
      g_hash_table_remove(sims, path);

      if (!g_hash_table_size(sims))
      {
        ofono_sim_remove_dbus_filter();
        g_hash_table_unref(sims);
        sims = NULL;
      }
    }
    else
      g_hash_table_insert(sims, g_strdup(path), notifiers);
  }
}
