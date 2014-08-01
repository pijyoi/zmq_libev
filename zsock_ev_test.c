#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <ev.h>
#include <zmq.h>

#include "zsock_ev.h"

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

	zmq_pollitem_t item = { zsock, 0, ZMQ_POLLIN };
	zsock_ev_t *zev = zsock_ev_register(loop, &item, zsock_handler, NULL);

	ev_run (loop, 0);
	printf("loop exited\n");

	zsock_ev_unregister(loop, &zev);

	zmq_close(zsock);
	zmq_ctx_destroy(zctx);

	return 0;
}

