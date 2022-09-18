#include "ofono-private.h"
#include "log.h"
#include "utils.h"

struct _pending_operation
{
  gpointer token;
  gpointer user_data;
  pending_operation_check_fn check;
  guint timeout_id;
};

struct _pending_operation_group
{
  gchar *path;
  GSList *operations;
  gpointer user_data;
  pending_operation_group_finish_fn group_finish;
};

struct _pending_operation_group_list
{
  GSList *groups;
};

/* pending operation */
pending_operation *
pending_operation_new(pending_operation_check_fn check, const gpointer token,
                      gpointer user_data)
{
  pending_operation *p = g_new0(pending_operation, 1);

  OFONO_INFO("Token %p", token);
  p->check = check;
  p->token = token;
  p->user_data = user_data;

  return p;
}

static void
pending_operation_free(pending_operation *p)
{
  g_free(p);
}

/* pending operation group */
pending_operation_group *
pending_operation_group_new(const gchar *path,
                            pending_operation_group_finish_fn group_finish,
                            gpointer user_data)
{
  pending_operation_group *g = g_new0(pending_operation_group, 1);

  g->path = g_strdup(path);
  g->group_finish = group_finish;
  g->user_data = user_data;

  return g;
}

void
pending_operation_group_free(pending_operation_group *g)
{
  g_slist_free_full(g->operations, (GDestroyNotify)pending_operation_free);
  g_free(g->path);
  g_free(g);
}

void
pending_operation_group_add_operation(pending_operation_group *g,
                                      pending_operation *p, int timeout)
{
  g->operations = g_slist_append(g->operations, p);
}

gboolean
pending_operation_group_is_empty(pending_operation_group *g)
{
  return g->operations == NULL;
}

static void
pending_operation_group_execute(pending_operation_group *group)
{
  GSList *l;
  GSList *to_remove = NULL;
  gboolean has_error = FALSE;
  enum operation_status status = OPERATION_STATUS_UNKNOWN;
  gpointer token = NULL;

  for (l = group->operations; l; l = l->next)
  {
    pending_operation *p = l->data;

    token = p->token;
    status = p->check(group->path, p->token, group->user_data, p->user_data);

    switch (status)
    {
      case OPERATION_STATUS_FINISHED:
        to_remove = g_slist_append(to_remove, p);
      /* falltrough */
      case OPERATION_STATUS_CONTINUE:
        break;
      default:
        has_error = TRUE;
        break;
    }

    if (has_error)
      break;
  }

  if (has_error)
  {
    g_slist_free_full(group->operations,
                      (GDestroyNotify)pending_operation_free);
    group->operations = NULL;
    group->group_finish(group->path, status, token, group->user_data);
  }
  else
  {
    for (l = to_remove; l; l = l->next)
    {
      group->operations = g_slist_remove(group->operations, l->data);
      pending_operation_free(l->data);
    }

    if (!group->operations)
    {
      group->group_finish(group->path, OPERATION_STATUS_FINISHED, NULL,
                          group->user_data);
    }
  }

  g_slist_free(to_remove);
}

static void
pending_operation_group_abort(pending_operation_group *group)
{
  group->group_finish(group->path, OPERATION_STATUS_ABORT, 0, group->user_data);
  g_slist_free_full(group->operations,
                    (GDestroyNotify)pending_operation_free);
  group->operations = NULL;
}

/* pending operation group list */
pending_operation_group_list *
pending_operation_group_list_create()
{
  return g_new0(pending_operation_group_list, 1);
}

void
pending_operation_group_list_destroy(pending_operation_group_list *list)
{
  g_slist_free_full(list->groups,
                    (GDestroyNotify)pending_operation_group_free);
  g_free(list);
}

gboolean
pending_operation_group_list_is_empty(pending_operation_group_list *list)
{
  return list->groups == NULL;
}

void
pending_operation_group_list_add(pending_operation_group_list *list,
                                 pending_operation_group *group)
{
  list->groups = g_slist_append(list->groups, group);
}

void
pending_operation_group_list_execute(pending_operation_group_list *list)
{
  GSList *l;
  GSList *to_remove = NULL;

  for (l = list->groups; l; l = l->next)
  {
    pending_operation_group *group = l->data;

    pending_operation_group_execute(group);

    if (pending_operation_group_is_empty(group))
      to_remove = g_slist_append(to_remove, group);
  }

  for (l = to_remove; l; l = l->next)
  {
    list->groups = g_slist_remove(list->groups, l->data);
    pending_operation_group_free(l->data);
  }

  g_slist_free(to_remove);
}

void
pending_operation_group_list_remove(pending_operation_group_list *list,
                                    const gchar *path)
{
  GSList *to_remove = NULL;
  GSList *l;

  g_return_if_fail(path != NULL);

  for (l = list->groups; l; l = l->next)
  {
    pending_operation_group *group = l->data;

    if (!g_strcmp0(group->path, path))
      to_remove = g_slist_append(to_remove, group);
  }

  for (l = to_remove; l; l = l->next)
  {
    list->groups = g_slist_remove(list->groups, l->data);
    pending_operation_group_abort(l->data);
    pending_operation_group_free(l->data);
  }

  g_slist_free(to_remove);
}
