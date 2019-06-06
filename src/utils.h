#ifndef __ICD_OFONO_UTILS_H__
#define __ICD_OFONO_UTILS_H__

enum operation_status
{
  OPERATION_STATUS_UNKNOWN,
  OPERATION_STATUS_CONTINUE,
  OPERATION_STATUS_FINISHED,
  OPERATION_STATUS_ERROR,
  OPERATION_STATUS_TIMEOUT,
  OPERATION_STATUS_CANCEL,
  OPERATION_STATUS_ABORT
};

typedef enum operation_status (*pending_operation_check_fn)(const gchar *path, const gpointer token, gpointer group_user_data, gpointer user_data);
typedef void (*pending_operation_group_finish_fn)(const gchar *path, enum operation_status status, const gpointer token, gpointer user_data);

typedef struct _pending_operation pending_operation;
typedef struct _pending_operation_group pending_operation_group;
typedef struct _pending_operation_group_list pending_operation_group_list;

pending_operation *pending_operation_new(pending_operation_check_fn check, const gpointer token, gpointer user_data);

pending_operation_group *pending_operation_group_new(const gchar *path, pending_operation_group_finish_fn group_finish, gpointer user_data);
void pending_operation_group_free(pending_operation_group *g);
void pending_operation_group_add_operation(pending_operation_group *g, pending_operation *p, int timeout);
gboolean pending_operation_group_is_empty(pending_operation_group *g);

pending_operation_group_list *pending_operation_group_list_create();
void pending_operation_group_list_destroy(pending_operation_group_list *list);
gboolean pending_operation_group_list_is_empty(pending_operation_group_list *list);
void pending_operation_group_list_add(pending_operation_group_list *list, pending_operation_group *group);
void pending_operation_group_list_remove(pending_operation_group_list *list, const gchar *path);
void pending_operation_group_list_execute(pending_operation_group_list *list);

#endif /* __ICD_OFONO_UTILS_H__ */
