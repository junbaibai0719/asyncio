#include <coroutine>
#include <thread>
#include <iostream>
#include <chrono>
#include <variant>
#include <QtCore>



#define NOW_TIME_T std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())

#if __has_cpp_attribute(likely) && __has_cpp_attribute(unlikely)
#define AS_LIKELY [[likely]]
#define AS_UNLIKELY [[unlikely]]
#else
#define AS_LIKELY
#define AS_UNLIKELY
#endif

namespace qasync
{

#define CallbackInstance CallbackSignal::getInstance()

template<typename T>
struct async;

class CallbackSignal: public QObject
{
    Q_OBJECT
public:
    static CallbackSignal *getInstance();
Q_SIGNALS:
    void awaitHandleResume(void *handle);
public Q_SLOTS:
    void onAwaitHandleResume(void *handle);

private:
    CallbackSignal();
    static CallbackSignal *instance;
};


struct DetachedCoroutine {
    struct promise_type {
        std::suspend_never initial_suspend() noexcept
        {
            return {};
        }
        std::suspend_never final_suspend() noexcept
        {
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception()
        {
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception &e) {
                fprintf(stderr, "find exception %s\n", e.what());
                fflush(stderr);
                std::rethrow_exception(std::current_exception());
            }
        }
        DetachedCoroutine get_return_object() noexcept
        {
            return DetachedCoroutine();
        }
    };
};

template<typename T>
struct Promise {
    struct FinalAwaiter {
        bool await_ready() const noexcept { return false; }
        template <typename PromiseType>
        auto await_suspend(std::coroutine_handle<PromiseType> h) noexcept {
            // resume parent coro
            return h.promise().parent_coro;
        }
        void await_resume() noexcept {}
    };

    async<T> get_return_object()
    {
        return async<T> {std::coroutine_handle<Promise<T>>::from_promise(*this)};
    }

    std::suspend_always initial_suspend()noexcept
    {
        return {};
    }

    FinalAwaiter final_suspend() noexcept
    {
        qDebug() << "final suspend";
        return {};
    }

    template <typename V>
    void return_value(V &&value) noexcept(
        std::is_nothrow_constructible_v<
        T, V &&>) requires std::is_convertible_v<V &&, T> {
        _value.template emplace<T>(std::forward<V>(value));
        this->ready = true;
        if(resume_handle)
        {
            emit CallbackSignal::getInstance()->awaitHandleResume(resume_handle);
        }
    }

    void unhandled_exception() noexcept
    {
        _value.template emplace<std::exception_ptr>(std::current_exception());
    }

    T &result()& {
        if (std::holds_alternative<std::exception_ptr>(_value))
            AS_UNLIKELY {
            std::rethrow_exception(std::get<std::exception_ptr>(_value));
        }
        assert(std::holds_alternative<T>(_value));
        return std::get<T>(_value);
    }

    T &&result()&& {
        if (std::holds_alternative<std::exception_ptr>(_value))
            AS_UNLIKELY {
            std::rethrow_exception(std::get<std::exception_ptr>(_value));
        }
        assert(std::holds_alternative<T>(_value));
        return std::move(std::get<T>(_value));
    }

    std::variant<std::monostate, T, std::exception_ptr> _value;
    bool ready = false;
    void *resume_handle = nullptr;
    std::coroutine_handle<> parent_coro;
};

template<typename T>
struct Awaitable {
    using Handle = std::coroutine_handle<Promise<T>>;
    Handle _handle;
    Awaitable(Handle coro) : _handle(coro)
    {
    }
    ~Awaitable()
    {
        if (_handle) {
            _handle.destroy();
            _handle = nullptr;
        }
    }

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
            qDebug() << "destroy handle:" << _handle.address();
            _handle.destroy();
            _handle = nullptr;
            return r;
        }
    }

    auto await_suspend(std::coroutine_handle<> callback_handle) noexcept
    {
        std::cout << "await suspend ready:" << this->_handle.promise().ready << " thread id "<< std::this_thread::get_id() << std::endl;
        this->_handle.promise().parent_coro = callback_handle;
        // return self current coro, run current coro
        return this->_handle;
//        if (this->_handle.promise().ready) {
//            callback_handle();
//        } else {
//            qDebug() << "handle addr";
//            qDebug() << _handle.address();
//            std::cout << "await suspend set resume_handle:" << callback_handle.address() << " thread id "<< std::this_thread::get_id() << std::endl;
//            this->_handle.promise().resume_handle = callback_handle.address();
//        }
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
        QThreadPool::globalInstance()->start(
        [=] {
            std::cout<< NOW_TIME_T << "\t" << "New thread id " << std::this_thread::get_id() << std::endl;
            coro.resume();
        });
//        std::thread t(
//        [=] {
//            std::cout<< NOW_TIME_T << "\t" << "New thread id " << std::this_thread::get_id() << std::endl;
//            coro.resume();
//        });
//        t.detach();
    }

    void await_resume()
    {
    }
};

template<typename T>
struct async {
    using promise_type = Promise<T>;
    using ValueType = T;
    async(std::coroutine_handle<Promise<T>> coro) : _coro(coro)
    {
        std::cout << "init async " << this << " " << _coro.address() << std::endl;
    }
    ~async()
    {
        std::cout << "delete async " << this << std::endl;
//        if (_coro) {
//            std::cout << "delete async, delete _handle " << _coro.address() << std::endl;
//            _coro.destroy();
//            _coro = nullptr;
//        }
    };
    std::coroutine_handle<Promise<T>> _coro;

    auto operator co_await()
    {
        return Awaitable<T>(std::exchange(_coro, nullptr));
    }
    template <typename Func>
    void start(Func callback) requires(std::is_invocable_v<Func, T>)
    {
        auto func = [=, this]()->DetachedCoroutine{
            callback(co_await *this);
            qDebug() << "call func";
        };
        func();
        qDebug() << 123131313;
    }
};
}


