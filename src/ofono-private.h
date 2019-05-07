#include "icd/network_api.h"

typedef struct _ofono_private ofono_private;

#include "utils.h"

struct _ofono_private
{
  icd_nw_close_fn close_fn;
  icd_nw_watch_pid_fn watch_fn;
  gpointer watch_fn_token;
  pending_operation_group_list *operation_groups;
};
