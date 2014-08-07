#ifndef UV_ZSOCK_H_
#define UV_ZSOCK_H_

#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uv_zsock_s;
typedef struct uv_zsock_s uv_zsock_t;

typedef void (*uv_zsock_cbfn)(uv_zsock_t *handle, int revents);

struct uv_zsock_s
{
	void *data;		// rw
	uv_loop_t *loop;	// ro

	void *zsock;		// read-only
	uv_zsock_cbfn cb;	// read-only
	int events;		// read-only

	// private
	uv_prepare_t w_prepare;
	uv_check_t w_check;
	uv_idle_t w_idle;
	uv_poll_t w_poll;
};

void uv_zsock_init(uv_loop_t *loop, uv_zsock_t *wz, void *zsock);
void uv_zsock_start(uv_zsock_t *wz, uv_zsock_cbfn cb, int events);
void uv_zsock_stop(uv_zsock_t *wz);

#ifdef __cplusplus
}
#endif

#endif

