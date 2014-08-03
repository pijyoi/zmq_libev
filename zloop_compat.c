#include <czmq.h>
#include "ev_zsock.h"

typedef struct _s_poller_t s_poller_t;
typedef struct _s_timer_t s_timer_t;

struct _zloop_t {
	struct ev_loop *evloop;

	zlist_t *pollers;
	zlist_t *timers;
	int last_timer_id;
};

struct _s_poller_t {
	ev_zsock_t w_zsock;

	zmq_pollitem_t item;
	zloop_fn *handler;
	void *arg;
};

struct _s_timer_t {
	ev_timer w_timer;
	
	int timer_id;
	zloop_timer_fn *handler;
	size_t times;
	void *arg;
};

static int
s_next_timer_id(zloop_t *self)
{
	return ++self->last_timer_id;
}

zloop_t *
zloop_new()
{
	zloop_t *self;
	self = (zloop_t *)zmalloc(sizeof(zloop_t));
	if (self) {
		self->pollers = zlist_new();
		self->timers = zlist_new();
	}
	return self;
}

void
zloop_destroy(zloop_t **self_p)
{
	assert(self_p);
	if (*self_p) {
		zloop_t *self = *self_p;

		while (zlist_size(self->pollers))
			free (zlist_pop(self->pollers));
		zlist_destroy(&self->pollers);
		
		while (zlist_size(self->timers))
			free(zlist_pop(self->timers));
		zlist_destroy(&self->timers);

		free (self);
		*self_p = NULL;
	}
}

static
void s_zsock_shim(struct ev_loop *evloop, ev_zsock_t *wz, int revents)
{
	s_poller_t *poller = (s_poller_t *)wz;

	poller->item.revents = (revents & EV_READ ? ZMQ_POLLIN : 0)
			| (revents & EV_WRITE ? ZMQ_POLLOUT : 0);

	zloop_t *zloop = (zloop_t *)wz->data;
	int rc = poller->handler(zloop, &poller->item, poller->arg);
	if (rc!=0) {
		ev_break(evloop, EVBREAK_ONE);
	}
}

static s_poller_t *
s_poller_new(zmq_pollitem_t *item, zloop_fn handler, void *arg)
{
	s_poller_t *poller = (s_poller_t *)zmalloc(sizeof(s_poller_t));
	if (poller) {
		int events = (item->events & ZMQ_POLLIN ? EV_READ : 0)
				| (item->events & ZMQ_POLLOUT ? EV_WRITE : 0);
		ev_zsock_init(&poller->w_zsock, s_zsock_shim, item->socket, events);

		poller->item = *item;
		poller->handler = handler;
		poller->arg = arg;
	}
	return poller;
}

int
zloop_poller(zloop_t *self, zmq_pollitem_t *item, zloop_fn handler, void *arg)
{
	assert (self);
	
	s_poller_t *poller = s_poller_new(item, handler, arg);
	if (poller) {
		if (zlist_append(self->pollers, poller))
			return -1;

		poller->w_zsock.data = self;
		ev_zsock_start(self->evloop, &poller->w_zsock);
		return 0;
	}
	else {
		return -1;
	}
}

void
zloop_poller_end(zloop_t *self, zmq_pollitem_t *item)
{
	assert(self);
	assert(item->socket || item->fd);

	s_poller_t *poller = (s_poller_t *)zlist_first(self->pollers);
	while (poller) {
		if (item->socket && item->socket==poller->item.socket) {
			zlist_remove(self->pollers, poller);

			ev_zsock_stop(self->evloop, &poller->w_zsock);

			free(poller);
		}
		poller = (s_poller_t *)zlist_next(self->pollers);
	}
}

static
void s_timer_shim(struct ev_loop *evloop, ev_timer *wt, int revents)
{
	s_timer_t *timer = (s_timer_t *)wt;

	zloop_t *zloop = (zloop_t *)wt->data;
	int rc = timer->handler(zloop, timer->timer_id, timer->arg);

	if (timer->times > 0 && --timer->times==0) {
		zlist_remove(zloop->timers, timer);

		ev_timer_stop(zloop->evloop, &timer->w_timer);

		free(timer);
	}

	if (rc!=0) {
		ev_break(evloop, EVBREAK_ONE);
	}
}

static s_timer_t *
s_timer_new(int timer_id, size_t delay, size_t times, zloop_timer_fn handler, void *arg)
{
	s_timer_t *timer = (s_timer_t *)zmalloc(sizeof(s_timer_t));
	if (timer) {
		ev_timer *w_timer = &timer->w_timer;
		double delay_sec = delay * 1e-3;
		ev_timer_init(w_timer, s_timer_shim, delay_sec, times!=1 ? delay_sec : 0.0);

		timer->timer_id = timer_id;
		timer->times = times;
		timer->handler = handler;
		timer->arg = arg;
	}
	return timer;
}

int
zloop_timer(zloop_t *self, size_t delay, size_t times, zloop_timer_fn handler, void *arg)
{
	assert(self);
	int timer_id = s_next_timer_id(self);
	s_timer_t *timer = s_timer_new(timer_id, delay, times, handler, arg);
	if (!timer)
		return -1;
	if (zlist_append(self->timers, timer)) {
		return -1;
	}

	timer->w_timer.data = self;
	ev_timer_start(self->evloop, &timer->w_timer);

	return timer_id;
}

int
zloop_timer_end(zloop_t *self, int timer_id)
{
	assert(self);

	s_timer_t *timer = (s_timer_t *)zlist_first(self->timers);
	while (timer) {
		if (timer_id==timer->timer_id) {
			zlist_remove(self->timers, timer);

			ev_timer_stop(self->evloop, &timer->w_timer);

			free(timer);
		}
		timer = (s_timer_t *)zlist_next(self->timers);
	}

	return 0;
}

int
zloop_start(zloop_t *self)
{
	assert(self);
	assert(self->evloop);

	ev_run(self->evloop, 0);

	return 0;
}

static int
s_cancel_timer_event(zloop_t *zloop, int timer_id, void *arg)
{
	int cancel_timer_id = *((int *)arg);
	printf("timer %d: canceling timer %d\n", timer_id, cancel_timer_id);
	return zloop_timer_end(zloop, cancel_timer_id);
}

static int
s_timer_event(zloop_t *zloop, int timer_id, void *output)
{
	const char *msg = "PING";
	printf("timer %d: sending %s\n", timer_id, msg);
	zstr_send(output, msg);
	return 0;
}

static int
s_socket_event(zloop_t *zloop, zmq_pollitem_t *item, void *arg)
{
	char *msg = zstr_recv(item->socket);
	printf("received %s\n", msg);
	zstr_free(&msg);

	return -1;
}

void
zloop_compat_test()
{
	void *zctx = zmq_ctx_new();

	void *zsock_recv = zmq_socket(zctx, ZMQ_PULL);
	assert(zsock_recv!=NULL);
	int rc = zmq_bind(zsock_recv, "tcp://127.0.0.1:5555");
	assert(rc!=-1);

	void *zsock_send = zmq_socket(zctx, ZMQ_PUSH);
	assert(zsock_send!=NULL);
	rc = zmq_connect(zsock_send, "tcp://127.0.0.1:5555");
	assert(rc!=-1);

	zloop_t *zloop = zloop_new();
	zloop->evloop = ev_default_loop(0);

	int timer_id = zloop_timer(zloop, 1000, 1, s_timer_event, NULL);
	zloop_timer(zloop, 5, 1, s_cancel_timer_event, &timer_id);

	zloop_timer(zloop, 20, 1, s_timer_event, zsock_send);

	zmq_pollitem_t item = { zsock_recv, 0, ZMQ_POLLIN };
	zloop_poller(zloop, &item, s_socket_event, NULL);

	zloop_start(zloop);
	printf("loop exited\n");

	zloop_destroy(&zloop);

	zmq_close(zsock_send);
	zmq_close(zsock_recv);
	zmq_ctx_destroy(zctx);
}

int main()
{
	zloop_compat_test();
	return 0;
}

