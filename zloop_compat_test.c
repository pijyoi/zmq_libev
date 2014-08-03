#include <czmq.h>

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

