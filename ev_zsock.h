#ifndef EV_ZSOCK_H_
#define EV_ZSOCK_H_

#include <ev.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ev_zsock_t;
typedef struct ev_zsock_t ev_zsock_t;

typedef void (*ev_zsock_cbfn)(struct ev_loop *loop, ev_zsock_t *wz, int revents);

struct ev_zsock_t
{
	void *data;		// rw

	ev_zsock_cbfn cb;	// read-only
	void *zsock;		// read-only
	int events;		// read-only

	// private
	ev_prepare w_prepare;
	ev_check w_check;
	ev_idle w_idle;
	ev_io w_io;
};

void ev_zsock_init(ev_zsock_t *wz, ev_zsock_cbfn cb, void *zsock, int events);
void ev_zsock_start(struct ev_loop *loop, ev_zsock_t *wz);
void ev_zsock_stop(struct ev_loop *loop, ev_zsock_t *wz);

#ifdef __cplusplus
}
#endif

#endif

