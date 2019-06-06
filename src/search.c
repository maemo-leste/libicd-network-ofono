#include <gofono_names.h>

#include "ofono-private.h"
#include "ofono-modem.h"
#include "search.h"
#include "iap.h"
#include "log.h"

static icd_nw_search_cb_fn _search_cb = NULL;
static gpointer _search_cb_token = NULL;
static guint operations_group_timeout_id = 0;

static void
simmgr_remove_handler(struct modem_data *md)
{
  if (md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY])
  {
    ofono_simmgr_remove_handler(
          md->sim, md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY]);
    md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY] = 0;
  }
}

static enum operation_status
search_operation_check(const gchar *path, const gpointer token,
                       gpointer group_user_data, gpointer user_data)
{
  ofono_private *priv = group_user_data;
  enum operation_status rv = OPERATION_STATUS_CONTINUE;
  struct modem_data *md;

  OFONO_ENTER

  md = g_hash_table_lookup(priv->modems, path);

  if (!md)
  {
    OFONO_ERR("No modem with path %s found", path);
    rv = OPERATION_STATUS_ERROR;
  }
  else
  {
    OfonoSimMgr *sim = md->sim;

    if (ofono_modem_valid(md->modem) &&
        ofono_simmgr_valid(sim) &&
        sim->present &&
        sim->imsi && *sim->imsi)
    {
      gchar *iap_id = ofono_iap_sim_is_provisioned(sim->imsi, priv);

      if (iap_id)
      {
        /* SIM already provisioned, finish */
        g_free(iap_id);
        rv = OPERATION_STATUS_FINISHED;
      }
      else
      {
        if (!sim->spn || !*sim->spn)
        {
          /* we need modem online to get SPN */
          if (!md->modem->online)
            ofono_modem_set_online(md->modem, TRUE);
        }
        else
          rv = OPERATION_STATUS_FINISHED;
      }
    }
  }

  if (rv == OPERATION_STATUS_FINISHED)
    simmgr_remove_handler(md);

  OFONO_INFO("Status %d", rv);
  OFONO_EXIT

  return rv;
}

static gboolean
operations_group_timeout(gpointer user_data)
{
  ofono_private *priv = user_data;

  OFONO_ENTER

  operations_group_timeout_id = 0;
  pending_operation_group_list_remove(priv->operation_groups, NULL);

  OFONO_EXIT

  return FALSE;
}

static gboolean
idle_check_group_list(gpointer user_data)
{
  ofono_private *priv = user_data;

  OFONO_ENTER

  if (pending_operation_group_list_is_empty(priv->operation_groups))
  {
    OFONO_INFO("Search finished");

    if (operations_group_timeout_id)
    {
      g_source_remove(operations_group_timeout_id);
      operations_group_timeout_id = 0;
      pending_operation_group_list_remove(priv->operation_groups, NULL);
    }

    if (_search_cb)
    {
      OFONO_DEBUG("Calling search_cb to finish searching");
      _search_cb(ICD_NW_SEARCH_COMPLETE, NULL, NULL, 0, NULL, 0, NULL, 0,
                 _search_cb_token);
      _search_cb = NULL;
      _search_cb_token = NULL;
    }
  }

  OFONO_EXIT

  return FALSE;
}

static void
add_search_result(struct modem_data *md, ofono_private *priv)
{
  OFONO_ENTER

  gchar *iap_id = ofono_iap_sim_is_provisioned(md->sim->imsi, priv);
  gchar *name;

  if (!iap_id)
  {
    OFONO_INFO("SIM %s seen for the very first time, provisioning.",
               md->sim->imsi);
    iap_id = ofono_iap_provision_sim(md, priv);
  }

  name = ofono_iap_get_name(iap_id);
  g_free(iap_id);

  OFONO_DEBUG("Adding GPRS/%s/%s", name, md->sim->imsi);

  _search_cb(ICD_NW_SEARCH_CONTINUE, name, "GPRS",
             ICD_NW_ATTR_IAPNAME | ICD_NW_ATTR_AUTOCONNECT, iap_id,
             ICD_NW_LEVEL_NONE, "GPRS", ICD_NW_SUCCESS, _search_cb_token);

  g_free(name);

  OFONO_EXIT
}

static void
ofono_start_search_finish(const gchar *path, enum operation_status status,
                          const gpointer token, gpointer user_data)
{
  ofono_private *priv = user_data;
  struct modem_data *md;

  OFONO_ENTER
  OFONO_DEBUG("Search group for path %s finished, status %d", path, status);

  md = g_hash_table_lookup(priv->modems, path);

  if (!md)
    OFONO_ERR("No modem with path %s found", path);
  else
  {
    if (status == OPERATION_STATUS_FINISHED)
      add_search_result(md, priv);

    simmgr_remove_handler(md);
  }
  /*
    we need to do idle check as current cb is called before group is removed
    from the list
  */
  g_idle_add(idle_check_group_list, priv);

  OFONO_EXIT
}

static gboolean
operations_check(gpointer user_data)
{
  ofono_private *priv = user_data;

  pending_operation_group_list_execute(priv->operation_groups);

  return FALSE;
}

static void
simmgr_property_changed(OfonoSimMgr* sender, const char* name, GVariant* value,
                        void* arg)
{
  operations_check(arg);
}

static void
modems_foreach(gpointer key, gpointer value, gpointer user_data)
{
  const gchar *path = key;
  ofono_private *priv = user_data;
  pending_operation_group *g =
      pending_operation_group_new(path, ofono_start_search_finish, priv);
  struct modem_data *md = value;

  OFONO_DEBUG("path %s", path);

  pending_operation_group_add_operation(
        g, pending_operation_new(search_operation_check, NULL, NULL), -1);

  g_assert(md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY] == 0);

  md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY] =
      ofono_simmgr_add_property_changed_handler(md->sim,
                                                simmgr_property_changed, NULL,
                                                priv);

  pending_operation_group_list_add(priv->operation_groups, g);
}

void
ofono_start_search(const gchar *network_type, guint search_scope,
                   icd_nw_search_cb_fn search_cb,
                   const gpointer search_cb_token, gpointer *_priv)
{
  ofono_private *priv = *_priv;
  gboolean finish = TRUE;

  OFONO_ENTER

  if (priv->online)
  {
    if (g_hash_table_size(priv->modems))
    {
      _search_cb = search_cb;
      _search_cb_token = search_cb_token;
      g_hash_table_foreach(priv->modems, modems_foreach, priv);
      g_idle_add(operations_check, priv);
      operations_group_timeout_id = g_timeout_add_seconds(
            SEARCH_INTERVAL, operations_group_timeout, priv);
      finish = FALSE;
    }
    else
      OFONO_INFO("No modems found");
  }
  else
    OFONO_INFO("Device is in offline mode");

  if (finish)
  {
    search_cb(ICD_NW_SEARCH_COMPLETE, NULL, NULL, 0, NULL, 0, NULL, 0,
              search_cb_token);
  }

  OFONO_EXIT
}

void
ofono_stop_search(gpointer *_priv)
{
  OFONO_ENTER
  OFONO_EXIT
}
