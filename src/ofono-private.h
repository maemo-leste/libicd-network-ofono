#include <gofono_manager.h>
#include "icd/network_api.h"
#include <gconf/gconf-client.h>

typedef struct _ofono_private ofono_private;

#include "utils.h"

struct _ofono_private
{
  icd_nw_close_fn close_fn;
  icd_nw_watch_pid_fn watch_fn;
  gpointer watch_fn_token;
  pending_operation_group_list *operation_groups;
  GHashTable *iaps;
  GConfClient *gconf;
  gboolean online;

  OfonoManager* manager;
  gulong modem_removed_id;
  gulong modem_added_id;
  gulong manager_valid_id;
  GHashTable *modems;
};

#define SEARCH_INTERVAL 20
#define SEARCH_LIFETIME 30

#define SIM_IMSI "sim_imsi"
