#ifndef __ICD_OFONO_NOTIFIER_H__
#define __ICD_OFONO_NOTIFIER_H__

#include <glib.h>

typedef void (*notify_fn)(const gpointer data, gpointer user_data);

void notifier_register(GSList **notifiers, notify_fn cb, gpointer user_data);
void notifier_notify(GSList *notifiers, const gpointer data);
void notifier_close(GSList **notifiers, notify_fn cb, gpointer user_data);

#endif /* __ICD_OFONO_NOTIFIER_H__ */
