#include <ofono/dbus.h>

#include <string.h>

#include "dbus-helpers.h"
#include "ofono-private.h"
#include "ofono-manager.h"
#include "ofono-modem.h"
#include "ofono-sim.h"
#include "ofono-net.h"
#include "log.h"

static GHashTable *modems = NULL;
static GSList *notifiers = NULL;

static void
ofono_sim_property_change_cb(gpointer data, gpointer user_data)
{
  property_changed *pc = data;
  modem *m = user_data;
  const char *property = pc->property;
  gboolean notify = TRUE;

  OFONO_ENTER

  OFONO_DEBUG("SIM property changed %s", property);

  if (!strcmp(property, "Present"))
    m->sim.present = pc->val.bool_val;
  else if (!strcmp(property, "SubscriberIdentity"))
  {
    g_free(m->sim.imsi);
    m->sim.imsi = g_strdup(pc->val.str);
  }
  else if (!strcmp(property, "ServiceProviderName"))
  {
    g_free(m->sim.spn);
    m->sim.spn = g_strdup(pc->val.str);
  }
  else
    notify = FALSE;

  if (notify)
  {
    modem_changed mc;

    mc.modem = m;
    mc.type = OFONO_MANAGER_MODEM_CHANGE;
    notifier_notify(notifiers, &mc);
  }

  OFONO_EXIT
}

static void
ofono_net_property_change_cb(gpointer data, gpointer user_data)
{
  property_changed *pc = data;
  modem *m = user_data;
  const char *property = pc->property;
  gboolean notify = TRUE;

  OFONO_ENTER

  OFONO_DEBUG("NET property changed %s", property);

  if (!strcmp(property, "Status"))
  {
    m->net.registered = FALSE;
    m->net.roaming = FALSE;

    if (!strcmp(pc->val.str, "registered"))
      m->net.registered = TRUE;
    else if (!strcmp(pc->val.str, "roaming"))
    {
      m->net.registered = TRUE;
      m->net.roaming = TRUE;
    }
  }
  else if (!strcmp(property, "Name"))
  {
    g_free(m->net.name);
    m->net.name = g_strdup(pc->val.str);
  }
  else
    notify = FALSE;

  if (notify)
  {
    modem_changed mc;

    mc.modem = m;
    mc.type = OFONO_MANAGER_MODEM_CHANGE;
    notifier_notify(notifiers, &mc);
  }

  OFONO_EXIT
}
static void
ofono_modem_property_change_cb(gpointer data, gpointer user_data)
{
  property_changed *pc = data;
  modem *m = user_data;
  const char *path = m->path;
  const char *property = pc->property;
  gboolean notify = TRUE;

  OFONO_ENTER

  OFONO_DEBUG("Modem %s property changed %s", path, property);

  if (!strcmp(property, "Powered"))
    m->powered = pc->val.bool_val;
  else if (!strcmp(property, "Online"))
    m->online = pc->val.bool_val;
  else if (!strcmp(property, "Emergency"))
    m->emergency_call = pc->val.bool_val;
  else if (!strcmp(property, "Serial"))
    m->imei = g_strdup(pc->val.str);
  else if (!strcmp(property, "Interfaces"))
  {
    dbus_uint64_t old = m->interfaces;
    dbus_uint64_t diff = pc->val.u64 ^ old;

    if (diff & OFONO_MODEM_INTERFACE_SIM_MANAGER)
    {
      if (old & OFONO_MODEM_INTERFACE_SIM_MANAGER)
        ofono_sim_close(path, ofono_sim_property_change_cb, m);
      else
        ofono_sim_register(path, ofono_sim_property_change_cb, m);
    }

    if (diff & OFONO_MODEM_INTERFACE_NETWORK_REGISTRATION)
    {
      if (old & OFONO_MODEM_INTERFACE_NETWORK_REGISTRATION)
        ofono_net_close(path, ofono_net_property_change_cb, m);
      else
        ofono_net_register(path, ofono_net_property_change_cb, m);
    }

    m->interfaces = pc->val.u64;
  }
  else
    notify = FALSE;

  if (notify)
  {
    modem_changed mc;

    mc.modem = m;
    mc.type = OFONO_MANAGER_MODEM_CHANGE;
    notifier_notify(notifiers, &mc);
  }

  OFONO_EXIT
}

static void
_ofono_manager_add_modem(const gchar *path, gboolean powered)
{
  modem *m;
  modem_changed mc;

  m = modem_list_find(modems, path);

  if (!m)
  {
    m = modem_new(path, powered);
    modem_list_add(modems, m);
    modem_free(m);

    m = modem_list_find(modems, path);

    mc.type = OFONO_MANAGER_MODEM_ADD;
    mc.modem = m;

    notifier_notify(notifiers, &mc);

    ofono_modem_register(path, ofono_modem_property_change_cb, m);
  }
  else
  {
    mc.modem = m;
    mc.type = OFONO_MANAGER_MODEM_CHANGE;
    notifier_notify(notifiers, &mc);
  }

  if (!powered)
  {
    OFONO_INFO("Powering on %s", path);
    ofono_modem_set_power(path, TRUE, NULL, NULL);
  }
}

static gboolean
ofono_manager_modem_read_properties(DBusMessageIter *iter, gboolean *powered)
{
  DBusMessageIter sub;
  gboolean rv = FALSE;

  dbus_message_iter_recurse(iter, &sub);

  while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_DICT_ENTRY)
  {
    const char *property;
    DBusBasicValue val;
    int type = dbus_helper_read_basic_dict_property(&sub, &property, &val);

    if (type == DBUS_TYPE_INVALID)
      continue;

    if (!strcmp(property, "Powered"))
    {
      if (type != DBUS_TYPE_BOOLEAN)
        break;

      *powered = val.bool_val;
      rv = TRUE;
    }

    dbus_message_iter_next(&sub);
  }

  return rv;
}

static gboolean
ofono_manager_add_modem(DBusMessageIter *iter)
{
  gchar *path;
  gboolean rv = FALSE;

  OFONO_ENTER

  if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_OBJECT_PATH)
  {
    dbus_message_iter_get_basic(iter, &path);

    dbus_message_iter_next(iter);

    if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_ARRAY)
    {
      gboolean powered;

      if (ofono_manager_modem_read_properties(iter, &powered))
      {
        _ofono_manager_add_modem(path, powered);
        rv = TRUE;
      }
    }
  }
  else
    OFONO_WARN("Unknown dbus iterator arg type when adding modem");

  OFONO_EXIT

  return rv;
}

static void
ofono_manager_get_modems_cb(DBusPendingCall *pending, void *user_data)
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
    if (dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_ERROR)
    {
      DBusMessageIter iter;

      dbus_message_iter_init(reply, &iter);

      if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY)
      {
        DBusMessageIter array_iter;

        dbus_message_iter_recurse(&iter, &array_iter);

        while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRUCT)
        {
          DBusMessageIter struct_iter;

          dbus_message_iter_recurse(&array_iter, &struct_iter);

          if (!ofono_manager_add_modem(&struct_iter))
          {
            OFONO_WARN("Cannot add modem while processing GetModems reply");
            break;
          }

          dbus_message_iter_next(&array_iter);
        }
      }
      else
        OFONO_WARN("Unexpected argument type in GetModems reply");
    }
    else
      OFONO_WARN("GetModems returned '%s'", dbus_message_get_error_name(reply));

    dbus_message_unref(reply);
  }

  OFONO_EXIT
}

static DBusHandlerResult
ofono_manager_modem_filter(DBusConnection *connection, DBusMessage *message,
                           void *user_data)
{
  OFONO_ENTER

  if (dbus_message_is_signal(message,
                             OFONO_MANAGER_INTERFACE,
                             "ModemAdded"))
  {
    DBusMessageIter iter;

    if (dbus_message_iter_init(message, &iter))
    {
      if (!ofono_manager_add_modem(&iter))
        OFONO_WARN("Invalid arguments for ModemAdded signal");
    }
    else
      OFONO_WARN("Invalid arguments for ModemAdded signal");
  }
  else if (dbus_message_is_signal(message,
                                  OFONO_MANAGER_INTERFACE,
                                  "ModemRemoved"))
  {
    const char *path;

    if (dbus_message_get_args(message, NULL,
                              DBUS_TYPE_OBJECT_PATH, &path,
                              DBUS_TYPE_INVALID))
    {
      modem_changed mc;
      modem *m = modem_list_find(modems, path);

      mc.type = OFONO_MANAGER_MODEM_REMOVE;

      if (m)
      {
        mc.modem = m;
        notifier_notify(notifiers, &mc);
        ofono_modem_close(path, ofono_modem_property_change_cb, m);
        ofono_sim_close(path, ofono_sim_property_change_cb, m);
        ofono_net_close(path, ofono_net_property_change_cb, m);
        modem_list_remove(modems, path);
      }
      else
      {
        m = modem_new(path, FALSE);
        mc.modem = m;
        notifier_notify(notifiers, &mc);
        modem_free(m);
      }
    }
    else
      OFONO_WARN("Invalid arguments for ModemRemoved signal");
  }

  OFONO_EXIT

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
ofono_manager_modems_add_dbus_filter()
{
  return icd_dbus_connect_system_bcast_signal(
        OFONO_MANAGER_INTERFACE, ofono_manager_modem_filter, NULL, NULL);
}

static void
ofono_manager_modems_remove_dbus_filter()
{
  icd_dbus_disconnect_system_bcast_signal(
    OFONO_MANAGER_INTERFACE, ofono_manager_modem_filter, NULL, NULL);
}

static gboolean
ofono_manager_modems_init()
{
  DBusMessage *message;
  gboolean rv = FALSE;

  OFONO_ENTER

  message = dbus_message_new_method_call(OFONO_SERVICE,
                                       OFONO_MANAGER_PATH,
                                       OFONO_MANAGER_INTERFACE,
                                       "GetModems");

  if (message)
  {
    if (icd_dbus_send_system_mcall(message, -1, ofono_manager_get_modems_cb,
                                   NULL))
    {
      rv = TRUE;
    }
    else
      OFONO_ERR("could not send 'GetModems' message");

    dbus_message_unref(message);
  }
  else
    OFONO_ERR("could not create 'GetModems' method call");

  OFONO_EXIT

  return rv;
}

GHashTable *
ofono_manager_get_modems(void)
{
  return modems;
}

gboolean
ofono_manager_modems_register(notify_fn cb, gpointer user_data)
{
  gboolean rv = TRUE;

  if (!modems)
  {
    modems = modem_list_create();

    if ((rv = ofono_manager_modems_init()))
        rv = ofono_manager_modems_add_dbus_filter();
  }

  if (rv)
    notifier_register(&notifiers, cb, user_data);

  return rv;
}

void
ofono_manager_modems_close(notify_fn cb, gpointer user_data)
{
  notifier_close(&notifiers, cb, user_data);

  if (!notifiers)
  {
    ofono_manager_modems_remove_dbus_filter();
    modem_list_free(modems);
    modems = NULL;
  }
}
