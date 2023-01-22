#ifndef _IAP_H_
#define _IAP_H_
gchar *ofono_iap_provision_sim(struct modem_data *md, const char *name,
                               ofono_private *priv);
gchar *ofono_iap_sim_is_provisioned(const gchar *imsi, ofono_private *priv);
gchar *ofono_iap_get_name(const gchar *id);
#endif /* _IAP_H_ */
