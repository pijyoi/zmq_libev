#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <zmq.h>

#include "uvpp_zsock.hpp"

int main()
{
    uvpp::Loop uvloop;

    uvpp::Signal usignal(uvloop);
    usignal.set_callback([&uvloop](){
      uvloop.stop();
    });
    usignal.start(SIGINT);

	void *zctx = zmq_ctx_new();
	void *zsock_recv = zmq_socket(zctx, ZMQ_PULL);
	assert(zsock_recv!=NULL);
	int rc = zmq_bind(zsock_recv, "inproc://channel");
	assert(rc!=-1);

    uvpp::ZsockWatcher uzsock(uvloop, zsock_recv);
    uzsock.set_callback([zsock_recv](){
        uint64_t ts_recv = uv_hrtime();
    	uint64_t ts_send;
    	zmq_recv(zsock_recv, &ts_send, sizeof(ts_send), 0);
    	printf("%f\n", (ts_recv - ts_send)*1e-9);
    });
    uzsock.start();

	void *zsock_send = zmq_socket(zctx, ZMQ_PUSH);
	assert(zsock_send!=NULL);
	rc = zmq_connect(zsock_send, "inproc://channel");
	assert(rc!=-1);

    uvpp::Timer utimer(uvloop);
    utimer.set_callback([zsock_send](){
        uint64_t ts = uv_hrtime();
	    zmq_send(zsock_send, &ts, sizeof(ts), 0);
        zmq_send(zsock_send, &ts, sizeof(ts), 0);
    });
    utimer.start(1000, 1000);

    uvloop.run();
	printf("loop exited\n");

	zmq_close(zsock_recv);
	zmq_close(zsock_send);
	zmq_ctx_destroy(zctx);
}
