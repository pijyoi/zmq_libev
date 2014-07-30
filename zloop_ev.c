#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <libev/ev.h>
#include <zmq.h>

typedef int (*zloop_ev_fn)(struct ev_loop *loop, zmq_pollitem_t *item, void *arg);

struct zsock_evargs_t
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
	struct zsock_evargs_t *evargs = (struct zsock_evargs_t *)
		(((char *)w) - offsetof(struct zsock_evargs_t, w_prepare));

	zmq_pollitem_t *item = &evargs->item;
	s_get_revents(item);

	if (item->revents) {
		// idle ensures that libev will not block
		ev_idle_start(loop, &evargs->w_idle);
	} else {
		// let libev block on the fd
		int fd;
		size_t optlen = sizeof(fd);
		int rc = zmq_getsockopt(item->socket, ZMQ_FD, &fd, &optlen);
		assert(rc==0);

		ev_io_init(&evargs->w_io, s_io_cb, fd, 
			item->events ? EV_READ : 0);	// same events as zmq_poll()
		ev_io_start(loop, &evargs->w_io);
	}
}

static
void s_check_cb(struct ev_loop *loop, ev_check *w, int revents)
{
	struct zsock_evargs_t *evargs = (struct zsock_evargs_t *)
		(((char *)w) - offsetof(struct zsock_evargs_t, w_check));

	ev_idle_stop(loop, &evargs->w_idle);
	ev_io_stop(loop, &evargs->w_io);

	zmq_pollitem_t *item = &evargs->item;
	s_get_revents(item);

	if (item->revents)
	{
		evargs->callback(loop, item, evargs->arg);
	}
}

int zsock_ev_register(struct ev_loop *loop, struct zsock_evargs_t *evargs)
{
	zmq_pollitem_t *item = &evargs->item;

	if (!item->socket || !evargs->callback)
		return -1;

	ev_prepare_init(&evargs->w_prepare, s_prepare_cb);
	ev_prepare_start(loop, &evargs->w_prepare);

	ev_check_init(&evargs->w_check, s_check_cb);
	ev_check_start(loop, &evargs->w_check);

	ev_idle_init(&evargs->w_idle, s_idle_cb);

	return 0;
}

static void
sigint_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
	ev_break(loop, EVBREAK_ALL);
}

void timeout_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
	ev_tstamp ts = ev_now (loop);
	printf("%.3f\n", ts);
}

int zsock_handler(struct ev_loop *loop, zmq_pollitem_t *item, void *arg)
{
	char text[64] = {0};

	zmq_msg_t msg;
	zmq_msg_init(&msg);
	zmq_msg_recv(&msg, item->socket, 0);
	if (zmq_msg_size(&msg) < sizeof(text))
		memcpy(text, zmq_msg_data(&msg), zmq_msg_size(&msg));
	zmq_msg_close(&msg);
	printf("%s\n", text);

	return 0;
}

int main()
{
	struct ev_loop *loop = ev_default_loop(0);
	ev_signal signal_watcher;
	ev_signal_init(&signal_watcher, sigint_cb, SIGINT);
	ev_signal_start(loop, &signal_watcher);

	ev_timer timeout_watcher;
	ev_timer_init (&timeout_watcher, timeout_cb, 1.0, 1.0);
	ev_timer_start (loop, &timeout_watcher);

	void *zctx = zmq_ctx_new();
	void *zsock = zmq_socket(zctx, ZMQ_PULL);
	assert(zsock!=NULL);
	int rc = zmq_bind(zsock, "tcp://127.0.0.1:5555");
	assert(rc!=-1);

	struct zsock_evargs_t evargs;
	memset(&evargs, 0, sizeof(evargs));
	evargs.item.socket = zsock;
	evargs.item.events = ZMQ_POLLIN;
	evargs.callback = zsock_handler;
	zsock_ev_register(loop, &evargs);

	ev_run (loop, 0);
	printf("loop exited\n");

	zmq_close(zsock);
	zmq_ctx_destroy(zctx);

	return 0;
}

