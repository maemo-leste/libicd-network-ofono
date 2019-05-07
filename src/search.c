#include "ofono-private.h"
#include "log.h"

#include "search.h"
#include "ofono-manager.h"
#include "ofono-modem.h"
#include "mce.h"

enum search_token
{
  SEARCH_TOKEN_SIM_PRESENT = 1,
  SEARCH_TOKEN_SIM_IMSI,
  SEARCH_TOKEN_ONLINE,
  SEARCH_TOKEN_NET_IFACE,
  SEARCH_TOKEN_NET_REG,
  SEARCH_TOKEN_NET_NAME
};

icd_nw_search_cb_fn _search_cb = NULL;
gpointer _search_cb_token = NULL;

static enum operation_status
ofono_start_search_operation_check(const gchar *path, const gpointer token,
                                   gpointer user_data)
{
  enum operation_status rv = OPERATION_STATUS_CONTINUE;
  GHashTable *modems;
  modem *m;

  OFONO_ENTER

  modems = ofono_manager_get_modems();

  g_assert(modems != NULL);

  m = g_hash_table_lookup(modems, path);

  if (m)
  {
    switch (GPOINTER_TO_INT(token))
    {
      case SEARCH_TOKEN_SIM_PRESENT:
      {
        if (m->sim.present)
          rv = OPERATION_STATUS_FINISHED;
        break;
      }
      case SEARCH_TOKEN_SIM_IMSI:
      {
        if (m->sim.imsi && *m->sim.imsi)
          rv = OPERATION_STATUS_FINISHED;
        break;
      }
      case SEARCH_TOKEN_ONLINE:
      {
        if (m->online)
          rv = OPERATION_STATUS_FINISHED;
        else
          /* not the best way, but meh */
          ofono_modem_set_online(m->path, TRUE);

        break;
      }
      case SEARCH_TOKEN_NET_IFACE:
      {
        if (m->interfaces & OFONO_MODEM_INTERFACE_NETWORK_REGISTRATION)
          rv = OPERATION_STATUS_FINISHED;
        break;
      }
      case SEARCH_TOKEN_NET_REG:
      {
        if (m->net.registered)
          rv = OPERATION_STATUS_FINISHED;
        break;
      }
      case SEARCH_TOKEN_NET_NAME:
      {
        if (m->net.name && *m->net.name)
        {
          rv = OPERATION_STATUS_FINISHED;
        }
        break;
      }
      default:
      {
        OFONO_ERR("Unknown token %p", token);
        rv = OPERATION_STATUS_ERROR;
      }
    }

    OFONO_INFO("Token %p status %d", token, rv);
  }
  else
  {
    OFONO_ERR("No modem with path %s found", path);
    rv = OPERATION_STATUS_ERROR;
  }

  OFONO_EXIT

  return rv;
}

static gboolean
idle_check_group_list(gpointer user_data)
{
  ofono_private *priv = user_data;

  if (pending_operation_group_list_is_empty(priv->operation_groups))
  {
    OFONO_INFO("Search finished");

    if (_search_cb)
    {
      OFONO_DEBUG("Calling search_cb to finish searching");
      _search_cb(ICD_NW_SEARCH_COMPLETE, NULL, NULL, 0, NULL, 0, NULL, 0,
                 _search_cb_token);
      _search_cb = NULL;
      _search_cb_token = NULL;
    }
  }

  return FALSE;
}

static void
ofono_start_search_finish(const gchar *path, enum operation_status status,
                          gpointer user_data)
{
  ofono_private *priv = user_data;

  OFONO_ENTER

  OFONO_DEBUG("Search group for path %s finished, status %d", path, status);

  if (status == OPERATION_STATUS_FINISHED)
  {
    GHashTable *modems = ofono_manager_get_modems();
    modem *m = g_hash_table_lookup(modems, path);

    g_assert(modems != NULL);

    if (m)
    {
      OFONO_DEBUG("Adding GPRS/%s/%s", m->net.name, m->sim.imsi);
      _search_cb(ICD_NW_SEARCH_CONTINUE, m->net.name, "GPRS",
                 ICD_NW_ATTR_IAPNAME | ICD_NW_ATTR_AUTOCONNECT, m->sim.imsi,
                 ICD_NW_LEVEL_NONE, "GPRS", ICD_NW_SUCCESS, _search_cb_token);
    }
  }

  /*
    we need to do idle check as current cb is called before group is removed
    from the list
  */
  g_idle_add(idle_check_group_list, priv);

  OFONO_EXIT
}

static void
modems_foreach(gpointer key, gpointer value, gpointer user_data)
{
  modem *m = value;
  const gchar *path = key;
  gpointer *data = user_data;
  ofono_private *priv = data[0];
  gboolean *has_pending = data[1];
  pending_operation_group *g =
      pending_operation_group_new(path, ofono_start_search_finish, priv);

  if (!m->sim.present)
  {
    pending_operation_group_add_operation(
          g, pending_operation_new(ofono_start_search_operation_check,
                                   GINT_TO_POINTER(SEARCH_TOKEN_SIM_PRESENT)),
          -1);
  }

  if (!m->sim.imsi)
  {
    pending_operation_group_add_operation(
          g, pending_operation_new(ofono_start_search_operation_check,
                                   GINT_TO_POINTER(SEARCH_TOKEN_SIM_IMSI)),
          -1);
  }

  if (!m->online)
  {
    OFONO_INFO("modem %s is not online, bringing up", path);
    ofono_modem_set_online(m->path, TRUE);

    pending_operation_group_add_operation(
          g, pending_operation_new(ofono_start_search_operation_check,
                                   GINT_TO_POINTER(SEARCH_TOKEN_ONLINE)),
          -1);
  }

  if (!(m->interfaces & OFONO_MODEM_INTERFACE_NETWORK_REGISTRATION))
  {
    pending_operation_group_add_operation(
          g, pending_operation_new(ofono_start_search_operation_check,
                                   GINT_TO_POINTER(SEARCH_TOKEN_NET_IFACE)),
          -1);
  }

  if (!m->net.registered)
  {
    pending_operation_group_add_operation(
          g, pending_operation_new(ofono_start_search_operation_check,
                                   GINT_TO_POINTER(SEARCH_TOKEN_NET_REG)),
          -1);
  }

  if (!m->net.name)
  {
    pending_operation_group_add_operation(
          g, pending_operation_new(ofono_start_search_operation_check,
                                   GINT_TO_POINTER(SEARCH_TOKEN_NET_NAME)),
          -1);
  }

  if (pending_operation_group_is_empty(g))
  {
    pending_operation_group_free(g);
    _search_cb(ICD_NW_SEARCH_CONTINUE, m->net.name, "GPRS",
               ICD_NW_ATTR_IAPNAME | ICD_NW_ATTR_AUTOCONNECT, m->sim.imsi,
               ICD_NW_LEVEL_NONE, "GPRS", ICD_NW_SUCCESS, _search_cb_token);
  }
  else
  {
    pending_operation_group_list_add(priv->operation_groups, g);
    *has_pending = TRUE;
  }
}

void
ofono_start_search(const gchar *network_type, guint search_scope,
                   icd_nw_search_cb_fn search_cb,
                   const gpointer search_cb_token, gpointer *_priv)
{
  ofono_private *priv = *_priv;
  gboolean has_pending = FALSE;

  OFONO_ENTER

  _search_cb = search_cb;
  _search_cb_token = search_cb_token;

  if (ofono_mce_get_device_mode() == OFONO_MCE_DEVICE_MODE_NORMAL)
  {
    gpointer data[2] = {priv, &has_pending};

    g_hash_table_foreach(ofono_manager_get_modems(), modems_foreach, &data);
  }
  else
    OFONO_INFO("device is in offline mode");

  if (!has_pending)
  {
    search_cb(ICD_NW_SEARCH_COMPLETE, NULL, NULL, 0, NULL, 0, NULL, 0,
              search_cb_token);
  }
  else
    OFONO_DEBUG("PENDING!!!");

  OFONO_EXIT
}

void
ofono_stop_search(gpointer *_priv)
{
  ofono_private *priv = *_priv;

  OFONO_ENTER

  pending_operation_group_list_remove(priv->operation_groups, NULL);

  OFONO_EXIT
}
