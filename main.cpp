#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <coroutine>
#include <iostream>

template<typename T>
struct async;

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
        std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
    }

    void unhandled_exception() noexcept
    {
        _value.template emplace<std::exception_ptr>(std::current_exception());
    }

    T &result() & {
        std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
        if (std::holds_alternative<std::exception_ptr>(_value))
            AS_UNLIKELY {
            std::rethrow_exception(std::get<std::exception_ptr>(_value));
        }
        assert(std::holds_alternative<T>(_value));
        return std::get<T>(_value);
    }

    T &&result() && {
        std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
        if (std::holds_alternative<std::exception_ptr>(_value))
            AS_UNLIKELY {
            std::rethrow_exception(std::get<std::exception_ptr>(_value));
        }
        std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #3 my thread id is " << std::this_thread::get_id() << std::holds_alternative<T>(_value) << std::endl;
        assert(std::holds_alternative<T>(_value));
        return std::move(std::get<T>(_value));
    }

    std::variant<std::monostate, T, std::exception_ptr> _value;
};

template<typename T>
struct Awaitable {
    using Handle = std::coroutine_handle<Promise<T>>;
    Handle _handle;
    Awaitable(Handle coro) : _handle(coro) {}
    bool await_ready() const noexcept
    {
        std::cout << "await_ready" << std::endl;
        return false;
    }

    auto await_resume()
    {
        std::cout << "await resume" << std::endl;
        std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
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
        std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " fork my thread id is " << std::this_thread::get_id() << std::endl;
        std::thread t(
        [=] {
            std::cout<< std::chrono::system_clock::now() << "\t" << "New thread id " << std::this_thread::get_id() << std::endl;
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

async<int> task1()
{
    std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #1 my thread id is " << std::this_thread::get_id() << std::endl;
    co_await fork{};
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
    co_return 1231313;
}
async<int> task2()
{
    std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #1 my thread id is " << std::this_thread::get_id() << std::endl;
    auto fut = task1();
    std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    int r = co_await fut;
    std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #3 my thread id is " << std::this_thread::get_id() << std::endl;
    co_return r;
}
async<int> test()
{
    std::cout << "start test" << std::endl;
    int r = co_await task2();
    std::cout<< std::chrono::system_clock::now() << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
    std::cout << "await res:\t" << r<< std::endl;
    co_return r;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "async_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    std::cout << 1 << std::endl;
    test();
    return a.exec();
}
