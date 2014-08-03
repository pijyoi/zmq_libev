#include <assert.h>
#include <stddef.h>

#include <ev.h>
#include <zmq.h>

#include "ev_zsock.h"

static
void s_idle_cb(struct ev_loop *loop, ev_idle *w, int revents)
{
	ev_idle_stop(loop, w);
}

static
void s_io_cb(struct ev_loop *loop, ev_io *w, int revents)
{
}

static
int s_get_revents(void *zsock, int events)
{
	int revents = 0;

	int zmq_events;
	size_t optlen = sizeof(zmq_events);
	int rc = zmq_getsockopt(zsock, ZMQ_EVENTS, &zmq_events, &optlen);
	assert(rc==0);

	if (zmq_events & ZMQ_POLLOUT)
		revents |= events & EV_WRITE;
	if (zmq_events & ZMQ_POLLIN)
		revents |= events & EV_READ;

	return revents;
}

static
void s_prepare_cb(struct ev_loop *loop, ev_prepare *w, int revents)
{
	ev_zsock_t *wz = (ev_zsock_t *)
		(((char *)w) - offsetof(ev_zsock_t, w_prepare));

	revents = s_get_revents(wz->zsock, wz->events);
	if (revents) {
		// idle ensures that libev will not block
		ev_idle_start(loop, &wz->w_idle);
	}
}

static
void s_check_cb(struct ev_loop *loop, ev_check *w, int revents)
{
	ev_zsock_t *wz = (ev_zsock_t *)
		(((char *)w) - offsetof(ev_zsock_t, w_check));

	revents = s_get_revents(wz->zsock, wz->events);
	if (revents)
	{
		wz->cb(loop, wz, revents);
	}
}

void
ev_zsock_init(ev_zsock_t *wz, ev_zsock_cbfn cb, void *zsock, int events)
{
	wz->cb = cb;
	wz->zsock = zsock;
	wz->events = events;

	ev_prepare *pw_prepare = &wz->w_prepare;
	ev_prepare_init(pw_prepare, s_prepare_cb);

	ev_check *pw_check = &wz->w_check;
	ev_check_init(pw_check, s_check_cb);

	ev_idle *pw_idle = &wz->w_idle;
	ev_idle_init(pw_idle, s_idle_cb);

	zmq_pollitem_t item;
	size_t optlen = sizeof(item.fd);
	int rc = zmq_getsockopt(wz->zsock, ZMQ_FD, &item.fd, &optlen);
	assert(rc==0);

	#ifdef _WIN32
	// XXX not tested
	int fd = _open_osfhandle(item.fd, 0);
	// there is a problem here:
	// 	we are leaking the C runtime file descriptor.
	// 	if we close it, the underlying handle also gets closed.
	#else
	int fd = item.fd;
	#endif

	ev_io *pw_io = &wz->w_io;
	ev_io_init(pw_io, s_io_cb, fd, wz->events ? EV_READ : 0);
}

void ev_zsock_start(struct ev_loop *loop, ev_zsock_t *wz)
{
	ev_prepare_start(loop, &wz->w_prepare);
	ev_check_start(loop, &wz->w_check);
	ev_io_start(loop, &wz->w_io);
}

void ev_zsock_stop(struct ev_loop *loop, ev_zsock_t *wz)
{
	ev_prepare_stop(loop, &wz->w_prepare);
	ev_check_stop(loop, &wz->w_check);
	ev_idle_stop(loop, &wz->w_idle);
	ev_io_stop(loop, &wz->w_io);
}

