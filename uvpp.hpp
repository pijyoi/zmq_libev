#ifndef UVPP_HPP
#define UVPP_HPP

#include <iostream>
#include <functional>
#include <memory>

#include <stdint.h>
#include <assert.h>

#include <uv.h>

namespace uvpp
{
  typedef std::function<void()> BasicCallback;

  int print_error(int err) {
    std::cerr << uv_err_name(err) << " : " << uv_strerror(err) << std::endl;
    return 0;
  }

  class Loop
  {
  public:
    Loop()
    {
      m_uvloop.reset(new uv_loop_t());
      uv_loop_init(m_uvloop.get());
    }

    ~Loop()
    {
      if (alive()) {
        // this is to call the close callbacks of the watchers
        // so that their handle memory can get freed
        run();
      }
      int rc = uv_loop_close(m_uvloop.get());
      assert(rc==0 || print_error(rc));
    }

    operator uv_loop_t*() { return m_uvloop.get(); }

    int run(uv_run_mode mode = UV_RUN_DEFAULT) {
      return uv_run(m_uvloop.get(), mode);
    }

    bool alive() { return uv_loop_alive(m_uvloop.get()); }
    void stop() { uv_stop(m_uvloop.get()); }
    uint64_t now() { return uv_now(m_uvloop.get()); }
    void update_time() { uv_update_time(m_uvloop.get()); }

  private:
    std::unique_ptr<uv_loop_t> m_uvloop;
  };

  template <typename T>
  class BaseHandle
  {
  public:
    bool is_active() { return uv_is_active(as_handle())!=0; }
    bool is_closing() { return uv_is_closing(as_handle())!=0; }
    void ref() { uv_ref(as_handle()); }
    void unref() { uv_unref(as_handle()); }
    bool has_ref() { return uv_has_ref(as_handle())!=0; }

  protected:
    BaseHandle() {
      m_handle = new T();
      m_handle->data = this;
    }

    ~BaseHandle()
    {
      m_handle->data = nullptr;
      uv_close(as_handle(), [](uv_handle_t *handle){
        delete reinterpret_cast<T*>(handle);
      });
    }

  public:
    BaseHandle(const BaseHandle&) = delete;
    BaseHandle& operator=(const BaseHandle&) = delete;

  private:
    uv_handle_t *as_handle() { return reinterpret_cast<uv_handle_t*>(m_handle); }

  protected:
    T *m_handle;
  };

  class Signal : public BaseHandle<uv_signal_t>
  {
  public:
    Signal(Loop& uvloop) {
      uv_signal_init(uvloop, m_handle);
    }

    void set_callback(BasicCallback cb) {
      m_callback = cb;
    }

    void start(int signum) {
      uv_signal_start(m_handle, [](uv_signal_t *handle, int signum){
        static_cast<Signal*>(handle->data)->m_callback();
      }, signum);
    }

    void stop() { uv_signal_stop(m_handle); }

  private:
    BasicCallback m_callback;
  };

  class Timer : public BaseHandle<uv_timer_t>
  {
  public:
    Timer(Loop& uvloop) {
      uv_timer_init(uvloop, m_handle);
    }

    void set_callback(BasicCallback cb) {
      m_callback = cb;
    }

    void start(uint64_t timeout, uint64_t repeat) {
      uv_timer_start(m_handle, [](uv_timer_t *handle){
        static_cast<Timer*>(handle->data)->m_callback();
      }, timeout, repeat);
    }

    void stop() { uv_timer_stop(m_handle); }
    void again() { uv_timer_again(m_handle); }

  private:
    BasicCallback m_callback;
  };

} // namespace uvpp
#endif
