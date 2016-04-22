#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <uv.h>
#include <zmq.h>

#include "uv_zsock.h"

static void
sigint_cb(uv_signal_t *handle, int signum)
{
	uv_stop(handle->loop);
}

void timeout_cb(uv_timer_t *handle)
{
	uint64_t ts = uv_hrtime();
	void *zsock = handle->data;
	zmq_send(zsock, &ts, sizeof(ts), 0);
	zmq_send(zsock, &ts, sizeof(ts), 0);
}

void zsock_cb(uv_zsock_t *handle, int revents)
{
	uint64_t ts_recv = uv_hrtime();
	uint64_t ts_send;
	zmq_recv(handle->zsock, &ts_send, sizeof(ts_send), 0);

	printf("%f\n", (ts_recv - ts_send)*1e-9);
}

int main()
{
	uv_loop_t uvloop;
	uv_loop_init(&uvloop);

	uv_signal_t signal_watcher;
	uv_signal_init(&uvloop, &signal_watcher);
	uv_signal_start(&signal_watcher, sigint_cb, SIGINT);

	void *zctx = zmq_ctx_new();
	void *zsock_recv = zmq_socket(zctx, ZMQ_PULL);
	assert(zsock_recv!=NULL);
	int rc = zmq_bind(zsock_recv, "inproc://channel");
	assert(rc!=-1);

	uv_zsock_t wz;
	uv_zsock_init(&uvloop, &wz, zsock_recv);
	uv_zsock_start(&wz, zsock_cb, UV_READABLE);

	void *zsock_send = zmq_socket(zctx, ZMQ_PUSH);
	assert(zsock_send!=NULL);
	rc = zmq_connect(zsock_send, "inproc://channel");
	assert(rc!=-1);

	uv_timer_t timeout_watcher;
	uv_timer_init(&uvloop, &timeout_watcher);
	timeout_watcher.data = zsock_send;
	uv_timer_start(&timeout_watcher, timeout_cb, 1000, 1000);

	uv_run (&uvloop, 0);
	printf("loop exited\n");

	uv_close((uv_handle_t*)&signal_watcher, NULL);
	uv_close((uv_handle_t*)&timeout_watcher, NULL);
	uv_zsock_close(&wz, NULL);

	zmq_close(zsock_recv);
	zmq_close(zsock_send);
	zmq_ctx_destroy(zctx);

	uv_run(&uvloop, 0);
	rc = uv_loop_close(&uvloop);
	assert(rc==0);

	return 0;
}
