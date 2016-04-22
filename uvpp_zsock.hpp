#ifndef UVPP_ZSOCK_HPP
#define UVPP_ZSOCK_HPP

#include "uvpp.hpp"
#include "uv_zsock.h"

namespace uvpp
{
    class ZsockWatcher
    {
    public:
        ZsockWatcher(Loop& uvloop, void *zsock)
        {
            m_watcher.reset(new uv_zsock_t());
            uv_zsock_init(uvloop, m_watcher.get(), zsock);
            m_watcher->data = this;
        }

        ~ZsockWatcher()
        {
            auto handle = m_watcher.release();
            uv_zsock_close(handle, [](uv_zsock_t *handle){
                delete handle;
            });
        }

        void set_callback(BasicCallback cb)
        {
            m_callback = cb;
        }

        void start()
        {
            uv_zsock_start(m_watcher.get(), [](uv_zsock_t *handle, int revents){
                static_cast<ZsockWatcher*>(handle->data)->m_callback();
            }, UV_READABLE);
        }

        void stop()
        {
            uv_zsock_stop(m_watcher.get());
        }

    private:
        std::unique_ptr<uv_zsock_t> m_watcher;
        BasicCallback m_callback;
    };
}

#endif
