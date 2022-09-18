#include <icd/osso-ic-gconf.h>
#include <icd/icd_gconf.h>

#include "log.h"
#include "icd-gconf.h"

void
ofono_icd_gconf_check_error(GError **error)
{
  if (error && *error)
  {
    OFONO_ERR("icd gconf error: %s", (*error)->message);
    g_clear_error(error);
    *error = NULL;
  }
}

gboolean
ofono_icd_gconf_set_iap_string(ofono_private *priv, const char *iap_name,
                               const char *key, const char *val)
{
  GError *err = NULL;
  char *id = gconf_escape_key(iap_name, -1);
  gchar *dir = g_strdup_printf(ICD_GCONF_PATH "/%s/%s", id, key);
  gboolean rv = gconf_client_set_string(priv->gconf, dir, val, &err);

  g_free(id);
  g_free(dir);
  ofono_icd_gconf_check_error(&err);

  return rv;
}

gchar *
ofono_icd_gconf_get_iap_string(ofono_private *priv, const char *iap_name,
                               const char *key)
{
  GError *err = NULL;
  char *id = gconf_escape_key(iap_name, -1);
  gchar *dir = g_strdup_printf(ICD_GCONF_PATH "/%s/%s", id, key);
  gchar *rv = gconf_client_get_string(priv->gconf, dir, &err);

  g_free(id);
  g_free(dir);
  ofono_icd_gconf_check_error(&err);

  return rv;
}

gboolean
ofono_icd_gconf_set_iap_bool(ofono_private *priv, const char *iap_name,
                             const char *key, gboolean val)
{
  GError *err = NULL;
  char *id = gconf_escape_key(iap_name, -1);
  gchar *dir = g_strdup_printf(ICD_GCONF_PATH "/%s/%s", id, key);
  gboolean rv = gconf_client_set_bool(priv->gconf, dir, val, &err);

  g_free(id);
  g_free(dir);
  ofono_icd_gconf_check_error(&err);

  return rv;
}

GHashTable *
get_gprs_iaps(ofono_private *priv)
{
  GHashTable *gprs_iaps =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  GSList *dirs = gconf_client_all_dirs(priv->gconf, ICD_GCONF_PATH, NULL);
  GSList *l;

  for (l = dirs; l; l = l->next)
  {
    const gchar *dir = l->data;
    gchar *key = g_strconcat(dir, "/" TYPE, NULL);
    gchar *type = gconf_client_get_string(priv->gconf, key, NULL);

    g_free(key);

    if (type && !strcmp(type, "GPRS"))
    {
      gchar *sim_imsi;

      key = g_strconcat(dir, "/", SIM_IMSI, NULL);
      sim_imsi = gconf_client_get_string(priv->gconf, key, NULL);
      g_free(key);

      if (sim_imsi)
      {
        const char *id = g_strrstr(dir, "/");

        if (*sim_imsi)
        {
          if (id)
          {
            g_hash_table_insert(gprs_iaps, sim_imsi,
                                gconf_unescape_key(id + 1, -1));
          }
        }
        else
        {
          OFONO_ERR("Empty SIM imsi found for IAP %s", id);
          g_free(sim_imsi);
        }
      }
    }
  }

  g_slist_free_full(dirs, g_free);

  return gprs_iaps;
}

