#include <czmq.h>
#include "ev_zsock.h"
#include "utlist.h"

typedef struct _s_poller_t s_poller_t;
typedef struct _s_timer_t s_timer_t;

struct _zloop_t {
	struct ev_loop *evloop;

	s_poller_t *pollers;
	s_timer_t *timers;
	int last_timer_id;

	bool canceled;
};

struct _s_poller_t {
	ev_zsock_t w_zsock;

	zmq_pollitem_t item;
	zloop_fn *handler;
	void *arg;

	s_poller_t *prev;
	s_poller_t *next;
};

struct _s_timer_t {
	ev_timer w_timer;
	
	int timer_id;
	zloop_timer_fn *handler;
	size_t times;
	void *arg;

	s_timer_t *prev;
	s_timer_t *next;
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
	self = (zloop_t *)malloc(sizeof(zloop_t));
	if (self) {
		self->evloop = ev_loop_new(0);

		self->pollers = NULL;
		self->timers = NULL;
		self->last_timer_id = 0;

		self->canceled = false;
	}
	return self;
}

void
zloop_destroy(zloop_t **self_p)
{
	assert(self_p);
	if (*self_p) {
		zloop_t *self = *self_p;

		ev_loop_destroy(self->evloop);

		{
			s_poller_t *head = self->pollers;
			s_poller_t *elt, *tmp;
			DL_FOREACH_SAFE(head, elt, tmp) {
				DL_DELETE(head, elt);
				free(elt);
			}
		}

		{
			s_timer_t *head = self->timers;
			s_timer_t *elt, *tmp;
			DL_FOREACH_SAFE(head, elt, tmp) {
				DL_DELETE(head, elt);
				free(elt);
			}
		}

		free (self);
		*self_p = NULL;
	}
}

static void
s_zsock_shim(struct ev_loop *evloop, ev_zsock_t *wz, int revents)
{
	s_poller_t *poller = (s_poller_t *)wz;

	poller->item.revents = (revents & EV_READ ? ZMQ_POLLIN : 0)
			| (revents & EV_WRITE ? ZMQ_POLLOUT : 0);

	zloop_t *zloop = (zloop_t *)wz->data;
	int rc = poller->handler(zloop, &poller->item, poller->arg);
	if (rc!=0) {
		zloop->canceled = true;
		ev_break(evloop, EVBREAK_ONE);
	}
}

static s_poller_t *
s_poller_new(zloop_t *zloop, zmq_pollitem_t *item, zloop_fn handler, void *arg)
{
	s_poller_t *poller = (s_poller_t *)malloc(sizeof(s_poller_t));
	if (poller) {
		int events = (item->events & ZMQ_POLLIN ? EV_READ : 0)
				| (item->events & ZMQ_POLLOUT ? EV_WRITE : 0);
		ev_zsock_init(&poller->w_zsock, s_zsock_shim, item->socket, events);
		poller->w_zsock.data = zloop;
		ev_zsock_start(zloop->evloop, &poller->w_zsock);

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
	
	s_poller_t *poller = s_poller_new(self, item, handler, arg);
	if (!poller)
		return -1;
	DL_APPEND(self->pollers, poller);

	return 0;
}

void
zloop_poller_end(zloop_t *self, zmq_pollitem_t *item)
{
	assert(self);
	assert(item->socket || item->fd);

	s_poller_t *head = self->pollers;
	s_poller_t *poller, *tmp;
	DL_FOREACH_SAFE(head, poller, tmp) {
		if (item->socket && item->socket==poller->item.socket) {
			DL_DELETE(head, poller);
			ev_zsock_stop(self->evloop, &poller->w_zsock);
			free(poller);
		}
	}
}

static void
s_timer_shim(struct ev_loop *evloop, ev_timer *wt, int revents)
{
	s_timer_t *timer = (s_timer_t *)wt;

	zloop_t *zloop = (zloop_t *)wt->data;
	int rc = timer->handler(zloop, timer->timer_id, timer->arg);

	if (timer->times > 0 && --timer->times==0) {
		DL_DELETE(zloop->timers, timer);
		ev_timer_stop(zloop->evloop, &timer->w_timer);
		free(timer);
	}

	if (rc!=0) {
		zloop->canceled = true;
		ev_break(evloop, EVBREAK_ONE);
	}
}

static s_timer_t *
s_timer_new(zloop_t *zloop, int timer_id, size_t delay, size_t times, zloop_timer_fn handler, void *arg)
{
	s_timer_t *timer = (s_timer_t *)malloc(sizeof(s_timer_t));
	if (timer) {
		ev_timer *w_timer = &timer->w_timer;
		double delay_sec = delay * 1e-3;
		ev_timer_init(w_timer, s_timer_shim, delay_sec, times!=1 ? delay_sec : 0.0);
		timer->w_timer.data = zloop;
		ev_timer_start(zloop->evloop, &timer->w_timer);

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
	s_timer_t *timer = s_timer_new(self, timer_id, delay, times, handler, arg);
	if (!timer)
		return -1;
	DL_APPEND(self->timers, timer);

	return timer_id;
}

int
zloop_timer_end(zloop_t *self, int timer_id)
{
	assert(self);

	s_timer_t *head = self->timers;
	s_timer_t *timer, *tmp;
	DL_FOREACH_SAFE(head, timer, tmp) {
		if (timer_id==timer->timer_id) {
			DL_DELETE(head, timer);
			ev_timer_stop(self->evloop, &timer->w_timer);
			free(timer);
		}
	}

	return 0;
}

int
zloop_start(zloop_t *self)
{
	assert(self);

	self->canceled = false;
	ev_run(self->evloop, 0);

	return self->canceled ? -1 : 0;
}

