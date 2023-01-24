#include "ofono-private.h"
#include "ofono-modem.h"
#include "link.h"
#include "log.h"

static void
destroy_modem_data(struct modem_data *md)
{
  g_hash_table_destroy(md->ctxhd);

  ofono_connmgr_unref(md->connmgr);
  ofono_simmgr_unref(md->sim);
  ofono_modem_unref(md->modem);

  g_free(md);
}

static void
power_up_modem(OfonoModem* modem, ofono_private *priv)
{
  if (!modem->powered)
    ofono_modem_set_powered(modem, TRUE);
}

static void
modem_valid_cb(OfonoModem *sender, void *arg)
{
  ofono_private *priv = arg;

  OFONO_ENTER

  if (sender->object.valid)
  {
    const char *path = ofono_modem_path(sender);
    struct modem_data *md = g_hash_table_lookup(priv->modems, path);

    if (md)
    {
      gulong id = md->modem_handler_id[MODEM_HANDLER_VALID];

      if (id)
      {
        ofono_modem_remove_handler(sender, id);
        md->modem_handler_id[MODEM_HANDLER_VALID] = 0;
      }
    }

    power_up_modem(sender, priv);
  }

  OFONO_EXIT
}

static void
create_modem(OfonoModem *modem, ofono_private *priv)
{
  struct modem_data *md = g_new0(struct modem_data, 1);
  const char *path = ofono_modem_path(modem);

  md->modem = ofono_modem_ref(modem);
  md->sim = ofono_simmgr_new(path);
  md->connmgr = ofono_connmgr_new(path);
  md->ctxhd = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                                    ofono_connctx_handler_data_destroy);

  g_hash_table_insert(priv->modems, g_strdup(path), md);

  if (ofono_modem_valid(modem))
    power_up_modem(modem, priv);
  else
  {
    md->modem_handler_id[MODEM_HANDLER_VALID] =
        ofono_modem_add_valid_changed_handler(modem, modem_valid_cb, priv);
  }
}

static void
modem_added_cb(OfonoManager* manager, OfonoModem* modem, void* arg)
{
  ofono_private *priv = arg;

  OFONO_ENTER

  create_modem(modem, priv);

  OFONO_EXIT
}

static void
modem_removed_cb(OfonoManager* manager, const char* path, void* arg)
{
  ofono_private *priv = arg;

  OFONO_ENTER

  OFONO_INFO("Removed modem, path %s", path);
  pending_operation_group_list_remove(priv->operation_groups, path);

  g_hash_table_remove(priv->modems, path);

  OFONO_EXIT
}

static void
manager_valid_cb(OfonoManager* manager, void* arg)
{
  ofono_private *priv = arg;
  GPtrArray *modems;
  guint i;

  OFONO_ENTER

  modems = ofono_manager_get_modems(priv->manager);

  for (i = 0; i < modems->len; i++)
    create_modem(modems->pdata[i], priv);

  OFONO_EXIT
}

void
ofono_modem_manager_init(ofono_private *priv)
{
  priv->modems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify)destroy_modem_data);
  priv->manager = ofono_manager_new();
  priv->manager_valid_id = ofono_manager_add_valid_changed_handler(
        priv->manager, manager_valid_cb, priv);
  priv->modem_added_id = ofono_manager_add_modem_added_handler(
        priv->manager, modem_added_cb, priv);
  priv->modem_removed_id = ofono_manager_add_modem_removed_handler(
        priv->manager, modem_removed_cb, priv);

  /*if (priv->manager->valid)
    power_up_modems(priv);*/

}

void
ofono_modem_manager_exit(ofono_private *priv)
{
  if (priv->modem_removed_id)
  {
    ofono_manager_remove_handler(priv->manager, priv->modem_removed_id);
    ofono_manager_remove_handler(priv->manager, priv->modem_added_id);
    ofono_manager_remove_handler(priv->manager, priv->manager_valid_id);
    priv->modem_removed_id = 0;
  }

  if (priv->manager)
  {
    ofono_manager_unref(priv->manager);
    priv->manager = NULL;
  }

  if (priv->modems)
  {
    g_hash_table_destroy(priv->modems);
    priv->modems = NULL;
  }

}

OfonoConnCtx *
ofono_modem_get_last_internet_context(struct modem_data *md)
{
  OfonoConnMgr *mgr;
  OfonoConnCtx *ctx = NULL;

  g_return_val_if_fail(md != NULL, NULL);

  mgr = md->connmgr;

  if (ofono_connmgr_valid(mgr))
  {
    GPtrArray *ctxs = ofono_connmgr_get_contexts(mgr);
    guint i;

    for (i = 0; i < ctxs->len; i++)
    {
      OfonoConnCtx *c = ctxs->pdata[i];

      if (ofono_connctx_valid(c) && c->type == OFONO_CONNCTX_TYPE_INTERNET)
        ctx = c;
    }
  }

  return ctx;
}

const char *
ofono_context_get_id(OfonoConnCtx *ctx)
{
  const char *context_id;

  g_return_val_if_fail(ctx != NULL, NULL);
  g_return_val_if_fail(ctx->object.path != NULL, NULL);

  context_id  = strrchr(ctx->object.path, '/');

  g_return_val_if_fail(context_id != NULL, NULL);

  return context_id + 1;
}

OfonoConnCtx *
ofono_modem_get_context_by_id(struct modem_data *md, const char *context_id)
{
  OfonoConnMgr *mgr;

  g_return_val_if_fail(md != NULL, NULL);
  g_return_val_if_fail(context_id != NULL, NULL);

  mgr = md->connmgr;

  if (ofono_connmgr_valid(mgr))
  {
    GPtrArray *ctxs = ofono_connmgr_get_contexts(mgr);
    guint i;

    for (i = 0; i < ctxs->len; i++)
    {
      OfonoConnCtx *ctx = ctxs->pdata[i];

      if (ofono_connctx_valid(ctx))
      {
        const char *candidate_id = ofono_context_get_id(ctx);

        if (candidate_id && !strcasecmp(candidate_id, context_id))
          return ctx;
      }
    }
  }

  return NULL;
}
