zmq_libev
=========

using zeromq with libev and libuv

This project is licensed under the terms of the MIT license.



ev_zsock.{c,h} implement a libzmq socket watcher for libev.
ev_zsock_test.c is an example of usage.

uv_zsock.{c,h} implement a libzmq socket watcher for libuv.
uv_zsock_test.c is an example of usage.


zloop_compat.c aims to be a compatible replacement for CZMQ's zloop class,
implemented using libev.
zloop_compat_test.c is a simple test case of the above.

