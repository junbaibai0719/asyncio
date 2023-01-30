#include <iostream>

#include "qasync.h"

using namespace qasync;

CallbackSignal *CallbackSignal::instance = nullptr;

CallbackSignal::CallbackSignal():QObject(){
    QObject::connect(this, &CallbackSignal::awaitHandleResume, this, &CallbackSignal::onAwaitHandleResume);
}

CallbackSignal *CallbackSignal::getInstance()
{
    if(instance == nullptr) {
        instance = new CallbackSignal();
    }
    return instance;
}

void CallbackSignal::onAwaitHandleResume(void *handle)
{

    qDebug() << "handle is nullptr" << (handle==nullptr);
    if (handle) {
        qDebug() << "handle addr:" << handle ;
        std::coroutine_handle<> resume_handle = std::coroutine_handle<>::from_address(handle);
        qDebug() << "handle addr:" << resume_handle.address();
        resume_handle.resume();
    }
}

