#ifndef _LINK_H_
#define _LINK_H_
void ofono_link_up(const gchar *network_type, const guint network_attrs, const gchar *network_id, icd_nw_link_up_cb_fn link_up_cb, const gpointer link_up_cb_token, gpointer *_priv);
void ofono_link_down(const gchar *network_type, const guint network_attrs, const gchar *network_id, const gchar *interface_name, icd_nw_link_down_cb_fn link_down_cb, const gpointer link_down_cb_token, gpointer *_priv);
#endif /* _LINK_H_ */
