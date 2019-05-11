#include "ofono-private.h"
#include "log.h"

#include "search.h"
#include "ofono-manager.h"
#include "ofono-modem.h"
#include "mce.h"
#include "iap.h"

enum search_token
{
  SEARCH_TOKEN_SIM_PRESENT = 1,
  SEARCH_TOKEN_SIM_IMSI,
  SEARCH_TOKEN_SIM_SPN,
  SEARCH_TOKEN_ONLINE
};

icd_nw_search_cb_fn _search_cb = NULL;
gpointer _search_cb_token = NULL;

static enum operation_status
ofono_start_search_operation_check(const gchar *path, const gpointer token,
                                   gpointer user_data)
{
  enum operation_status rv = OPERATION_STATUS_CONTINUE;
  GHashTable *modems;
  const modem *m;

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
      case SEARCH_TOKEN_SIM_SPN:
      {
        if (m->sim.spn && *m->sim.spn)
          rv = OPERATION_STATUS_FINISHED;

        break;
      }
      case SEARCH_TOKEN_ONLINE:
      {
        if (m->online == TRUE)
          rv = OPERATION_STATUS_FINISHED;
        else if (m->online == FALSE)
          ofono_modem_set_online(m->path, TRUE, NULL, NULL);

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
add_search_result(const modem *m, ofono_private *priv)
{
  gchar *iap_id = ofono_iap_is_provisioned(m->sim.imsi, priv);
  gchar *iap_id_unescaped;
  gchar *name;

  if (!iap_id)
    iap_id = ofono_provision_iap(m->sim.imsi, m->sim.spn, priv);

  iap_id_unescaped = gconf_unescape_key(iap_id, -1);
  name = ofono_iap_get_name(iap_id);

  g_free(iap_id);

  OFONO_DEBUG("Adding GPRS/%s/%s", name, m->sim.imsi);

  _search_cb(ICD_NW_SEARCH_CONTINUE, name, "GPRS",
             ICD_NW_ATTR_IAPNAME | ICD_NW_ATTR_AUTOCONNECT, iap_id_unescaped,
             ICD_NW_LEVEL_NONE, "GPRS", ICD_NW_SUCCESS, _search_cb_token);

  g_free(iap_id_unescaped);
  g_free(name);
}

static void
ofono_start_search_finish(const gchar *path, enum operation_status status,
                          const gpointer token, gpointer user_data)
{
  ofono_private *priv = user_data;

  OFONO_ENTER

  OFONO_DEBUG("Search group for path %s finished, status %d", path, status);

  if (status == OPERATION_STATUS_FINISHED)
  {
    GHashTable *modems = ofono_manager_get_modems();
    const modem *m = g_hash_table_lookup(modems, path);

    g_assert(modems != NULL);

    if (m)
      add_search_result(m, priv);
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
  const modem *m = value;
  const gchar *path = key;
  gpointer *data = user_data;
  ofono_private *priv = data[0];
  gboolean *has_pending = data[1];
  pending_operation_group *g =
      pending_operation_group_new(path, ofono_start_search_finish, priv);

  if (m->sim.present != TRUE)
  {
    pending_operation_group_add_operation(
          g, pending_operation_new(ofono_start_search_operation_check,
                                   GINT_TO_POINTER(SEARCH_TOKEN_SIM_PRESENT)),
          -1);
  }

  if (!m->sim.imsi || !*m->sim.imsi)
  {
    pending_operation_group_add_operation(
          g, pending_operation_new(ofono_start_search_operation_check,
                                   GINT_TO_POINTER(SEARCH_TOKEN_SIM_IMSI)),
          -1);
  }
  else
  {
    gchar *iap_id = ofono_iap_is_provisioned(m->sim.imsi, priv);

    if (!iap_id)
    {
      OFONO_INFO("SIM %s seen for the very first time, provisioning.",
                 m->sim.imsi);

      if (!m->sim.spn || !*m->sim.spn)
      {
        if (m->online != TRUE)
        {
          /* looks like ofono needs modem online and registered to get
             ServiceProviderName (HPLMN) */
          OFONO_INFO("modem %s is not online, bringing up", path);
          pending_operation_group_add_operation(
                g, pending_operation_new(ofono_start_search_operation_check,
                                         GINT_TO_POINTER(SEARCH_TOKEN_ONLINE)),
                -1);
          /*
           * this is weird, I know, but the idea is that we might not have
           * 'Online' property valid yet. If that is the case, we'll request
           * to set it in the property check callback
           */
          if (m->online == FALSE)
            ofono_modem_set_online(m->path, TRUE, NULL, NULL);
        }

        pending_operation_group_add_operation(
              g, pending_operation_new(ofono_start_search_operation_check,
                                       GINT_TO_POINTER(SEARCH_TOKEN_SIM_SPN)),
              -1);
      }
    }

    g_free(iap_id);
  }

  if (pending_operation_group_is_empty(g))
  {
    pending_operation_group_free(g);
    add_search_result(m, priv);
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
