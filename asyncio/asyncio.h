#include <coroutine>
#include <thread>
#include <iostream>
#include <chrono>
#include <variant>

template<typename T>
struct async;

#define NOW_TIME_T std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())

#if __has_cpp_attribute(likely) && __has_cpp_attribute(unlikely)
#define AS_LIKELY [[likely]]
#define AS_UNLIKELY [[unlikely]]
#else
#define AS_LIKELY
#define AS_UNLIKELY
#endif

template<typename T>
struct Promise {
    async<T> get_return_object()
    {
        return async<T> {std::coroutine_handle<Promise>::from_promise(*this)};
    }

    std::suspend_never initial_suspend()noexcept
    {
        std::cout << "initial suspend"<< std::endl;
        return {};
    }

    std::suspend_always final_suspend() noexcept
    {
        std::cout << "final suspend"<< std::endl;
        return {};
    }

    template <typename V>
    void return_value(V &&value) noexcept(
        std::is_nothrow_constructible_v<
        T, V &&>) requires std::is_convertible_v<V &&, T> {
        _value.template emplace<T>(std::forward<V>(value));
        std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
        this->ready = true;
    }

    void unhandled_exception() noexcept
    {
        _value.template emplace<std::exception_ptr>(std::current_exception());
    }

    T &result() & {
        std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
        if (std::holds_alternative<std::exception_ptr>(_value))
            AS_UNLIKELY {
            std::rethrow_exception(std::get<std::exception_ptr>(_value));
        }
        assert(std::holds_alternative<T>(_value));
        return std::get<T>(_value);
    }

    T &&result() && {
        std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
        if (std::holds_alternative<std::exception_ptr>(_value))
            AS_UNLIKELY {
            std::rethrow_exception(std::get<std::exception_ptr>(_value));
        }
        std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #3 my thread id is " << std::this_thread::get_id() << std::holds_alternative<T>(_value) << std::endl;
        assert(std::holds_alternative<T>(_value));
        return std::move(std::get<T>(_value));
    }

    std::variant<std::monostate, T, std::exception_ptr> _value;
    bool ready = false;
};

template<typename T>
struct Awaitable {
    using Handle = std::coroutine_handle<Promise<T>>;
    Handle _handle;
    Awaitable(Handle coro) : _handle(coro) {}
    bool await_ready() const noexcept
    {
        std::cout << "await_ready" << this->_handle.promise().ready << std::endl;
        return this->_handle.promise().ready;
    }

    auto await_resume()
    {
        std::cout << "await resume" << std::endl;
        std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
        if constexpr (std::is_void_v<T>) {
            _handle.promise().result();
            // We need to destroy the handle expclictly since the awaited
            // coroutine after symmetric transfer couldn't release it self any
            // more.
            _handle.destroy();
            _handle = nullptr;
        } else {
            auto r = std::move(_handle.promise()).result();
            _handle.destroy();
            _handle = nullptr;
            return r;
        }
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept
    {
        std::cout << "await suspend" << std::endl;
        while (!this->_handle.promise().ready) {
            // 让出cpu，直到promise准备好
            std::this_thread::yield();
        }
        std::cout << "await suspend resume handle, thread " << std::this_thread::get_id() << std::endl;
        handle();
    }

};

struct fork {
    fork()
    {

    }
    bool await_ready()
    {
        return false;
    }

    void await_suspend(std::coroutine_handle<> coro)
    {
        std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " fork my thread id is " << std::this_thread::get_id() << std::endl;
        std::thread t(
        [=] {
            std::cout<< NOW_TIME_T << "\t" << "New thread id " << std::this_thread::get_id() << std::endl;
            coro.resume();
        });
        t.detach();
    }

    void await_resume()
    {
    }
};

template<typename T>
struct async {
    using promise_type = Promise<T>;
    using ValueType = T;
    explicit async(std::coroutine_handle<Promise<T>> coro) : _coro(coro)
    {
        std::cout << "init async"<< std::endl;
    }
    ~async()
    {
        if (_coro) {
            _coro.destroy();
            _coro = nullptr;
        }
    };
    std::coroutine_handle<Promise<T>> _coro;

    auto operator co_await()
    {
        return Awaitable<T>(std::exchange(_coro, nullptr));
    }
};
