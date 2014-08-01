#include <assert.h>
#include <stddef.h>

#include <ev.h>
#include <zmq.h>

#include "ev_zsock.h"

static
void s_idle_cb(struct ev_loop *loop, ev_idle *w, int revents)
{
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

	if ((events & ZMQ_POLLOUT) && (zmq_events & ZMQ_POLLOUT))
		revents |= ZMQ_POLLOUT;
	if ((events & ZMQ_POLLIN) && (zmq_events & ZMQ_POLLIN))
		revents |= ZMQ_POLLIN;

	return revents;
}

static
void s_prepare_cb(struct ev_loop *loop, ev_prepare *w, int revents)
{
	ev_zsock_t *wz = (ev_zsock_t *)
		(((char *)w) - offsetof(ev_zsock_t, w_prepare));

	int zmq_revents = s_get_revents(wz->zsock, wz->events);

	if (zmq_revents) {
		// idle ensures that libev will not block
		ev_idle_start(loop, &wz->w_idle);
	} else {
		// let libev block on the fd
		ev_io_start(loop, &wz->w_io);
	}
}

static
void s_check_cb(struct ev_loop *loop, ev_check *w, int revents)
{
	ev_zsock_t *wz = (ev_zsock_t *)
		(((char *)w) - offsetof(ev_zsock_t, w_check));

	ev_idle_stop(loop, &wz->w_idle);
	ev_io_stop(loop, &wz->w_io);

	int zmq_revents = s_get_revents(wz->zsock, wz->events);

	if (zmq_revents)
	{
		wz->cb(loop, wz, zmq_revents);
	}
}

void
ev_zsock_init(ev_zsock_t *wz, ev_zsock_cbfn cb, void *zsock, int events)
{
	wz->cb = cb;
	wz->zsock = zsock;
	wz->events = events;

	ev_prepare_init(&wz->w_prepare, s_prepare_cb);
	ev_check_init(&wz->w_check, s_check_cb);
	ev_idle_init(&wz->w_idle, s_idle_cb);

	int fd;
	size_t optlen = sizeof(fd);
	int rc = zmq_getsockopt(wz->zsock, ZMQ_FD, &fd, &optlen);
	assert(rc==0);

	ev_io_init(&wz->w_io, s_io_cb, fd, 
		wz->events ? EV_READ : 0);	// same events as zmq_poll()
}

void ev_zsock_start(struct ev_loop *loop, ev_zsock_t *wz)
{
	ev_prepare_start(loop, &wz->w_prepare);
	ev_check_start(loop, &wz->w_check);
}

void ev_zsock_stop(struct ev_loop *loop, ev_zsock_t *wz)
{
	ev_prepare_stop(loop, &wz->w_prepare);
	ev_check_stop(loop, &wz->w_check);
	ev_idle_stop(loop, &wz->w_idle);
	ev_io_stop(loop, &wz->w_io);
}

