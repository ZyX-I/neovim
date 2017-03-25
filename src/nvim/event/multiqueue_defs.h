#ifndef NVIM_EVENT_MULTIQUEUE_DEFS_H
#define NVIM_EVENT_MULTIQUEUE_DEFS_H

typedef struct multiqueue MultiQueue;
typedef void (*put_callback)(MultiQueue *multiq, void *data);

#define multiqueue_put(q, h, ...) \
  multiqueue_put_event(q, event_create(1, h, __VA_ARGS__));

#endif  // NVIM_EVENT_MULTIQUEUE_DEFS_H
