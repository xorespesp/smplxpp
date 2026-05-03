#pragma once
#include <cxlib/cxlib_defs.h>
#include <cxlib/utils/logger.hh>
#include <cxlib/utils/spin_lock.hh>

#include <functional>
#include <thread>
#include <atomic>
#include <stdexcept>

_CXLIB_NAMESPACE_BEGIN
namespace utils
{
    class polling_thread
    {
    private:
        mutable utils::spin_lock _mtx;
        std::thread _thread;
        std::function<bool(bool)> _poll_fn;
        std::atomic_bool _stop_flag{ false };

    public:
        polling_thread() = default;
        ~polling_thread() {
            if (this->joinable()) {
                this->stop();
            }
        }

        template <typename _TyPollFn>
        void start(_TyPollFn&& poll_fn) {
            std::scoped_lock lk{ _mtx };
            if (_thread.joinable()) {
                throw std::runtime_error{ "polling thread already started" };
            }

            _stop_flag = false;
            _poll_fn = std::forward<_TyPollFn>(poll_fn);
            _thread = std::thread(&polling_thread::_thread_proc, this);
        }

        void request_stop() noexcept {
            _stop_flag = true;
        }

        void stop() {
            std::scoped_lock lk{ _mtx };
            _stop_flag = true;
            if (_thread.joinable()) {
                _thread.join();
            }
        }

        bool joinable() const noexcept {
            std::scoped_lock lk{ _mtx };
            return _thread.joinable();
        }

        void join() {
            std::scoped_lock lk{ _mtx };
            if (_thread.joinable()) {
                _thread.join();
            }
        }

    private:
        static void _thread_proc(
            polling_thread* const this_ptr)
        {
            CXLIB_TRACE("polling thread started.");

            bool first_run = true;
            while (!this_ptr->_stop_flag)
            {
                if (!this_ptr->_poll_fn(first_run)) {
                    this_ptr->_stop_flag = true;
                }
                first_run = false;
            }

            CXLIB_TRACE("polling thread terminated.");
        }

    }; // class

} // namespace
_CXLIB_NAMESPACE_END