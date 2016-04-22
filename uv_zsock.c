#include <assert.h>
#include <stddef.h>

#include <uv.h>
#include <zmq.h>

#include "uv_zsock.h"

static
void s_idle_cb(uv_idle_t *handle)
{
}

static
void s_poll_cb(uv_poll_t *handle, int status, int events)
{
}

static
int s_get_revents(void *zsock, int events)
{
	int revents = 0;

	int zmq_events;
	size_t optlen = sizeof(zmq_events);
	int rc = zmq_getsockopt(zsock, ZMQ_EVENTS, &zmq_events, &optlen);

	if (rc==-1) {
		// on error, make callback get called
		return events;
	}

	if (zmq_events & ZMQ_POLLOUT)
		revents |= events & UV_WRITABLE;
	if (zmq_events & ZMQ_POLLIN)
		revents |= events & UV_READABLE;

	return revents;
}

static
void s_prepare_cb(uv_prepare_t *handle)
{
	uv_zsock_t *wz = (uv_zsock_t *)
		(((char *)handle) - offsetof(uv_zsock_t, w_prepare));

	int revents = s_get_revents(wz->zsock, wz->events);
	if (revents) {
		// idle ensures that libuv will not block
		uv_idle_start(&wz->w_idle, s_idle_cb);
	}
}

static
void s_check_cb(uv_check_t *handle)
{
	uv_zsock_t *wz = (uv_zsock_t *)
		(((char *)handle) - offsetof(uv_zsock_t, w_check));

	uv_idle_stop(&wz->w_idle);

	int revents = s_get_revents(wz->zsock, wz->events);
	if (revents)
	{
		wz->cb(wz, revents);
	}
}

void
uv_zsock_init(uv_loop_t *loop, uv_zsock_t *wz, void *zsock)
{
	wz->loop = loop;
	wz->zsock = zsock;
	wz->cb = NULL;
	wz->events = 0;

	wz->w_prepare.data = wz;
	wz->w_check.data = wz;
	wz->w_idle.data = wz;
	wz->w_poll.data = wz;

	uv_prepare_init(loop, &wz->w_prepare);
	uv_check_init(loop, &wz->w_check);
	uv_idle_init(loop, &wz->w_idle);

	uv_os_sock_t sockfd;
	size_t optlen = sizeof(sockfd);
	int rc = zmq_getsockopt(wz->zsock, ZMQ_FD, &sockfd, &optlen);
	assert(rc==0);

	uv_poll_init_socket(loop, &wz->w_poll, sockfd);
}

void
uv_zsock_start(uv_zsock_t *wz, uv_zsock_cbfn cb, int events)
{
	wz->cb = cb;
	wz->events = events;

	uv_prepare_start(&wz->w_prepare, s_prepare_cb);
	uv_check_start(&wz->w_check, s_check_cb);
	uv_poll_start(&wz->w_poll, wz->events ? UV_READABLE : 0, s_poll_cb);
}

void
uv_zsock_stop(uv_zsock_t *wz)
{
	uv_prepare_stop(&wz->w_prepare);
	uv_check_stop(&wz->w_check);
	uv_idle_stop(&wz->w_idle);
	uv_poll_stop(&wz->w_poll);
}

static
void s_close_cb(uv_handle_t *handle)
{
	uv_zsock_t *wz = (uv_zsock_t*)handle->data;
	handle->data = NULL;	// mark as closed

	if (wz->w_prepare.data==NULL && wz->w_check.data==NULL &&
			wz->w_idle.data==NULL && wz->w_poll.data==NULL) {
				if (wz->close_cb)	wz->close_cb(wz);
	}
}

void
uv_zsock_close(uv_zsock_t *wz, uv_zsock_close_cbfn cb)
{
	wz->close_cb = cb;
	uv_close((uv_handle_t*)&wz->w_prepare, s_close_cb);
	uv_close((uv_handle_t*)&wz->w_check, s_close_cb);
	uv_close((uv_handle_t*)&wz->w_idle, s_close_cb);
	uv_close((uv_handle_t*)&wz->w_poll, s_close_cb);
}
