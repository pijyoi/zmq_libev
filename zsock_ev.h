#include <zmq.h>

struct ev_loop;

typedef int (*zloop_ev_fn)(struct ev_loop *loop, zmq_pollitem_t *item, void *arg);

struct zsock_ev_t;
typedef struct zsock_ev_t zsock_ev_t;

zsock_ev_t *zsock_ev_register(struct ev_loop *loop, zmq_pollitem_t *item, zloop_ev_fn callback, void *arg);
void zsock_ev_unregister(struct ev_loop *loop, zsock_ev_t **self_p);

