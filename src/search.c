#include "config.h"

#include <gofono_names.h>
#include <libxml/xmlreader.h>
#include <libxml/xpath.h>

#include "ofono-private.h"
#include "ofono-modem.h"
#include "search.h"
#include "iap.h"
#include "log.h"

static icd_nw_search_cb_fn _search_cb = NULL;
static gpointer _search_cb_token = NULL;

static void
remove_handlers(struct modem_data *md)
{
  if (md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY])
  {
    ofono_simmgr_remove_handler(
          md->sim, md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY]);
    md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY] = 0;
  }

  if (md->connmgr_handler_id[CONNMGR_HANDLER_CONTEXT_ADDED])
  {
    ofono_connmgr_remove_handler(
          md->connmgr, md->connmgr_handler_id[CONNMGR_HANDLER_CONTEXT_ADDED]);
    md->connmgr_handler_id[CONNMGR_HANDLER_CONTEXT_ADDED] = 0;
  }
}

static gboolean
search_pending_execute(gpointer user_data)
{
  ofono_private *priv = user_data;

  pending_operation_group_list_execute(priv->operation_groups);

  return FALSE;
}

static void
simmgr_property_changed(OfonoSimMgr* sender, const char* name, GVariant* value,
                        void* arg)
{
  search_pending_execute(arg);
}

static void
connmgr_context_added(OfonoConnMgr* sender, OfonoConnCtx* context, void* arg)
{
  search_pending_execute(arg);
}

static void
add_handlers(struct modem_data *md, ofono_private *priv)
{
  g_assert(md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY] == 0);
  g_assert(md->connmgr_handler_id[CONNMGR_HANDLER_CONTEXT_ADDED] == 0);

  md->simmgr_handler_id[SIMMGR_HANDLER_PROPERTY] =
      ofono_simmgr_add_property_changed_handler(md->sim,
                                                simmgr_property_changed, NULL,
                                                priv);
  md->connmgr_handler_id[CONNMGR_HANDLER_CONTEXT_ADDED] =
      ofono_connmgr_add_context_added_handler(md->connmgr,
                                              connmgr_context_added, priv);
}

struct operations_group_data
{
  ofono_private *priv;
  gchar *path;
  guint id;
};

static void
operations_group_data_destroy(struct operations_group_data *ogd)
{
  if (ogd->id)
    g_source_remove(ogd->id);

  g_free(ogd->path);
  g_free(ogd);
}

static enum operation_status
search_operation_check(const gchar *path, const gpointer token,
                       gpointer group_user_data, gpointer user_data)
{
  struct operations_group_data *ogd = group_user_data;
  ofono_private *priv = ogd->priv;
  enum operation_status rv = OPERATION_STATUS_CONTINUE;
  struct modem_data *md;

  OFONO_ENTER
  OFONO_DEBUG("Checking modem %s", path);

  md = g_hash_table_lookup(priv->modems, path);

  if (!md)
  {
    OFONO_ERR("No modem with path %s found", path);
    rv = OPERATION_STATUS_ERROR;
  }
  else
  {
    if (ofono_modem_valid(md->modem))
    {
      OfonoSimMgr *sim = md->sim;

      if (ofono_simmgr_valid(sim))
      {
        if (sim->present)
        {
          if (sim->imsi && *sim->imsi)
          {
            gchar *iap_id = ofono_iap_sim_is_provisioned(sim->imsi, priv);

            if (!md->modem->online && !iap_id)
            {
              /* we need modem online to get SPN */
              OFONO_DEBUG("Modem is offline, bring it online");
              ofono_modem_set_online(md->modem, TRUE);
            }

            if (iap_id)
            {
              /* SIM already provisioned, finish */

              OFONO_DEBUG("SIM already provisioned, iap_id [%s]", iap_id);

              g_free(iap_id);
              rv = OPERATION_STATUS_FINISHED;
            }
            else
            {
              OfonoConnCtx *ctx;

              OFONO_DEBUG("SIM not provisioned");

              /*
               * Find the *last* internet context. The reason to do so is -
               * if this is an LTE modem, context is auto-activated, however
               * if for some reason APN reported by the modem does not match
               * APN in mobile-broadband-provider-info (old data, etc.),
               * then another context is appended. We should use that
               * context for provisioning
               */
              ctx = ofono_modem_get_last_internet_context(md);

              if (ctx && ctx->apn && ctx->username && ctx->password)
              {
                /* we must provision IAP with context deactivated */
                if (ctx->active)
                {
                  OFONO_DEBUG("Deactivating chosen context %s",
                              ctx->object.path);
                  ofono_connctx_deactivate(ctx);
                }
                else
                  rv = OPERATION_STATUS_FINISHED;
              }
              else
                OFONO_DEBUG("No SIM internet context available yet");
            }
          }
          else
            OFONO_DEBUG("No SIM IMSI available yet");
        }
        else
        {
          OFONO_INFO("No SIM present");
          rv = OPERATION_STATUS_ABORT;
        }
      }
      else
        OFONO_DEBUG("SIM is not valid yet");
    }
    else
      OFONO_DEBUG("Modem is not valid yet");
  }

  if (rv == OPERATION_STATUS_FINISHED)
    remove_handlers(md);

  OFONO_INFO("Status %d", rv);
  OFONO_EXIT

  return rv;
}

static gboolean
operations_group_timeout(gpointer user_data)
{
  struct operations_group_data *ogd = user_data;

  OFONO_ENTER

  ogd->id = 0;
  pending_operation_group_list_remove(ogd->priv->operation_groups, ogd->path);

  OFONO_EXIT

  return G_SOURCE_REMOVE;
}

static gboolean
idle_check_group_list(gpointer user_data)
{
  ofono_private *priv = user_data;

  OFONO_ENTER

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

  OFONO_EXIT

  return FALSE;
}

static gchar *
_mbpi_get_name(int mcc, int mnc)
{
  xmlDocPtr doc;
  gchar *name = NULL;

  doc = xmlParseFile(MBPI_DATABASE);

  if (doc)
  {
    /* Create xpath evaluation context */
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);

    if (ctx)
    {
      gchar *xpath = g_strdup_printf(
        "//network-id[@mcc='%03d' and @mnc='%02d']/../../name/text()", mcc,
        mnc);
      xmlXPathObjectPtr obj = xmlXPathEvalExpression(BAD_CAST xpath, ctx);

      if (obj && obj->nodesetval)
      {
        xmlNodeSetPtr nodes = obj->nodesetval;

        if (nodes->nodeNr)
        {
          xmlChar *content = xmlNodeGetContent(nodes->nodeTab[0]);

          name = g_strdup((const gchar *)content);

          xmlFree(content);
        }

        xmlXPathFreeObject(obj);
      }
      else
        OFONO_WARN("Unable to evaluate xpath expression '%s'", xpath);

      g_free(xpath);
      xmlXPathFreeContext(ctx);
    }
    else
      OFONO_WARN("Unable to create new XPath context");

    xmlFreeDoc(doc);
  }
  else
    OFONO_WARN("Unable to parse '" MBPI_DATABASE "'");

  return name;
}

static gchar *
get_spn(struct modem_data *md)
{
  const gchar *name = md->sim->spn;
  int mnc;
  int mcc;

  if (name && *name)
    return g_strdup(name);

  if (sscanf(md->sim->imsi, "%03d%02d", &mcc, &mnc) != 2)
    return NULL;

  return _mbpi_get_name(mcc, mnc);
}

static void
add_search_result(struct modem_data *md, ofono_private *priv)
{
  OFONO_ENTER

  gchar *iap_id = ofono_iap_sim_is_provisioned(md->sim->imsi, priv);
  gchar *name = NULL;

  if (!iap_id)
  {
    OFONO_INFO("SIM %s seen for the very first time, provisioning.",
               md->sim->imsi);

    name = get_spn(md);

    if (!name)
    {
      OFONO_WARN("SIM %s, cannot create SPN", md->sim->imsi);
      name = g_strdup("Mobile Connection");
    }

    iap_id = ofono_iap_provision_sim(md, name, priv);
    g_free(name);
  }

  name = ofono_iap_get_name(iap_id);

  OFONO_DEBUG("Adding GPRS/%s/%s/%s", iap_id, name, md->sim->imsi);

  _search_cb(ICD_NW_SEARCH_CONTINUE, name, "GPRS",
             ICD_NW_ATTR_IAPNAME | ICD_NW_ATTR_AUTOCONNECT, iap_id,
             ICD_NW_LEVEL_NONE, "GPRS", ICD_NW_SUCCESS, _search_cb_token);

  g_free(iap_id);
  g_free(name);

  OFONO_EXIT
}

static void
ofono_start_search_finish(const gchar *path, enum operation_status status,
                          const gpointer token, gpointer user_data)
{
  struct operations_group_data *ogd = user_data;
  ofono_private *priv = ogd->priv;
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

    remove_handlers(md);
  }

  operations_group_data_destroy(ogd);

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
  ofono_private *priv = user_data;
  const gchar *path = key;
  pending_operation_group *group;
  struct modem_data *md = value;
  struct operations_group_data *ogd;

  ogd = g_try_new(struct operations_group_data, 1);

  OFONO_DEBUG("path %s", path);

  if (!ogd)
  {
    OFONO_CRIT("No enough memory allocating operations_group_data for path %s",
               path);
    return;
  }

  ogd->priv = priv;
  ogd->path = g_strdup(path);

  group = pending_operation_group_new(path, ofono_start_search_finish, ogd);

  pending_operation_group_add_operation(
        group, pending_operation_new(search_operation_check, NULL, NULL), -1);

  add_handlers(md, priv);

  pending_operation_group_list_add(priv->operation_groups, group);

  ogd->id =
      g_timeout_add_seconds(SEARCH_INTERVAL, operations_group_timeout, ogd);
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
      g_idle_add(search_pending_execute, priv);
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
