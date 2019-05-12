#include <libofono/log.h>

#include "ofono-private.h"

#include "link.h"

void
ofono_link_up(const gchar *network_type, const guint network_attrs,
              const gchar *network_id, icd_nw_link_up_cb_fn link_up_cb,
              const gpointer link_up_cb_token, gpointer *_priv)
{
  //ofono_private *priv = *_priv;
  const char *err_msg = "no_network";

  OFONO_ENTER

  link_up_cb(ICD_NW_ERROR, err_msg, 0, link_up_cb_token, 0);

  OFONO_EXIT
}

void
ofono_link_down(const gchar *network_type, const guint network_attrs,
                const gchar *network_id, const gchar *interface_name,
                icd_nw_link_down_cb_fn link_down_cb,
                const gpointer link_down_cb_token, gpointer *_priv)
{
  OFONO_ENTER

  link_down_cb(ICD_NW_SUCCESS, link_down_cb_token);

  OFONO_EXIT
}
