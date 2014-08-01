#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <ev.h>
#include <zmq.h>

#include "zsock_ev.h"

struct zsock_ev_t
{
	zmq_pollitem_t item;
	zloop_ev_fn callback;
	void *arg;

	// private
	ev_prepare w_prepare;
	ev_check w_check;
	ev_io w_io;
	ev_idle w_idle;
};

static
void s_idle_cb(struct ev_loop *loop, ev_idle *w, int revents)
{
}

static
void s_io_cb(struct ev_loop *loop, ev_io *w, int revents)
{
}

static
void s_get_revents(zmq_pollitem_t *item)
{
	item->revents = 0;

	int zmq_events;
	size_t optlen = sizeof(zmq_events);
	int rc = zmq_getsockopt(item->socket, ZMQ_EVENTS, &zmq_events, &optlen);
	assert(rc==0);

	if ((item->events & ZMQ_POLLOUT) && (zmq_events & ZMQ_POLLOUT))
		item->revents |= ZMQ_POLLOUT;
	if ((item->events & ZMQ_POLLIN) && (zmq_events & ZMQ_POLLIN))
		item->revents |= ZMQ_POLLIN;
}

static
void s_prepare_cb(struct ev_loop *loop, ev_prepare *w, int revents)
{
	zsock_ev_t *zev = (zsock_ev_t *)
		(((char *)w) - offsetof(zsock_ev_t, w_prepare));

	zmq_pollitem_t *item = &zev->item;
	s_get_revents(item);

	if (item->revents) {
		// idle ensures that libev will not block
		ev_idle_start(loop, &zev->w_idle);
	} else {
		// let libev block on the fd
		ev_io_start(loop, &zev->w_io);
	}
}

static
void s_check_cb(struct ev_loop *loop, ev_check *w, int revents)
{
	zsock_ev_t *zev = (zsock_ev_t *)
		(((char *)w) - offsetof(zsock_ev_t, w_check));

	ev_idle_stop(loop, &zev->w_idle);
	ev_io_stop(loop, &zev->w_io);

	zmq_pollitem_t *item = &zev->item;
	s_get_revents(item);

	if (item->revents)
	{
		zev->callback(loop, item, zev->arg);
	}
}

zsock_ev_t *
zsock_ev_register(struct ev_loop *loop, zmq_pollitem_t *item, zloop_ev_fn callback, void *arg)
{
	if (!item->socket || !callback)
		return NULL;

	zsock_ev_t *zev = malloc(sizeof(*zev));
	if (!zev)
		return NULL;

	memcpy(&zev->item, item, sizeof(zev->item));
	zev->callback = callback;
	zev->arg = arg;

	ev_prepare_init(&zev->w_prepare, s_prepare_cb);
	ev_prepare_start(loop, &zev->w_prepare);

	ev_check_init(&zev->w_check, s_check_cb);
	ev_check_start(loop, &zev->w_check);

	ev_idle_init(&zev->w_idle, s_idle_cb);

	int fd;
	size_t optlen = sizeof(fd);
	int rc = zmq_getsockopt(item->socket, ZMQ_FD, &fd, &optlen);
	assert(rc==0);

	ev_io_init(&zev->w_io, s_io_cb, fd, 
		item->events ? EV_READ : 0);	// same events as zmq_poll()

	return zev;
}

void zsock_ev_unregister(struct ev_loop *loop, zsock_ev_t **self_p)
{
	assert (self_p);
	if (*self_p) {
		zsock_ev_t *self = *self_p;
		ev_prepare_stop(loop, &self->w_prepare);
		ev_check_stop(loop, &self->w_check);
		ev_idle_stop(loop, &self->w_idle);
		ev_io_stop(loop, &self->w_io);

		free(self);
		*self_p = NULL;
	}
}

