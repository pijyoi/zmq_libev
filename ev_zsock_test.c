#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ev.h>
#include <zmq.h>

#include "ev_zsock.h"

static void
sigint_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
	ev_break(loop, EVBREAK_ALL);
}

void timeout_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
	ev_tstamp ts = ev_now (loop);
	void *zsock = w->data;
	zmq_send(zsock, &ts, sizeof(ts), 0);
}

void zsock_cb(struct ev_loop *loop, ev_zsock_t *wz, int revents)
{
	ev_tstamp ts_recv = ev_now (loop);
	ev_tstamp ts_send;
	zmq_recv(wz->zsock, &ts_send, sizeof(ts_send), 0);

	printf("%f\n", ts_recv - ts_send);
}

int main()
{
	struct ev_loop *loop = ev_default_loop(0);
	ev_signal signal_watcher;
	ev_signal *p_signal_watcher = &signal_watcher;
	ev_signal_init(p_signal_watcher, sigint_cb, SIGINT);
	ev_signal_start(loop, &signal_watcher);

	void *zctx = zmq_ctx_new();
	void *zsock_recv = zmq_socket(zctx, ZMQ_PULL);
	assert(zsock_recv!=NULL);
	int rc = zmq_bind(zsock_recv, "inproc://channel");
	assert(rc!=-1);

	ev_zsock_t wz;
	ev_zsock_init(&wz, zsock_cb, zsock_recv, ZMQ_POLLIN);
	ev_zsock_start(loop, &wz);

	void *zsock_send = zmq_socket(zctx, ZMQ_PUSH);
	assert(zsock_send!=NULL);
	rc = zmq_connect(zsock_send, "inproc://channel");
	assert(rc!=-1);

	ev_timer timeout_watcher;
	ev_timer *p_timeout_watcher = &timeout_watcher;
	ev_timer_init (p_timeout_watcher, timeout_cb, 1.0, 1.0);
	timeout_watcher.data = zsock_send;
	ev_timer_start (loop, &timeout_watcher);

	ev_run (loop, 0);
	printf("loop exited\n");

	zmq_close(zsock_recv);
	zmq_close(zsock_send);
	zmq_ctx_destroy(zctx);

	return 0;
}

