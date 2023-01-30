#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <coroutine>
#include <iostream>
#include <atomic>

//#include "asyncio/asyncio.h"
#include "qasync/qasync.h"
#include "message_handler.h"

using namespace qasync;

async<int> task1()
{
    std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #1 my thread id is " << std::this_thread::get_id() << std::endl;
    co_await fork{};
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
    co_return 1231313;
}

async<int> task2()
{
    std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #1 my thread id is " << std::this_thread::get_id() << std::endl;
    auto fut = task1();
    std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #2 sleep_for my thread id is " << std::this_thread::get_id() << std::endl;
    int r = co_await fut;
    std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #3 my thread id is " << std::this_thread::get_id() << std::endl;
    co_return r;
}


async<int> test()
{
    std::cout << "start test" << std::endl;
    int r = co_await task2();
    std::cout<< NOW_TIME_T << "\t" << __FUNCTION__ << " #2 my thread id is " << std::this_thread::get_id() << std::endl;
    std::cout << "await res:\t" << r<< std::endl;
    co_return r;
}

void test_test(){
    auto func = [=]()->DetachedCoroutine{
        co_await test();
    };
    func();
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageHandler);
    QCoreApplication a(argc, argv);
//    CallbackSignal::getInstance();
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "async_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    test_test();
    return a.exec();
}
