#include "server.h"
#include <iostream>
#include <assert.h>

#define LOG(s) std::cout << s << std::endl;
#define VERIFY(cond, s) assert(cond && s)

#define HTTP_DELIM          "\r\n"
#define HTTP_DELIM_BODY     HTTP_DELIM HTTP_DELIM

// our response
Buffer httpContent(const Buffer& body)
{
    std::ostringstream o;
    o << "HTTP/1.1 200 Ok" HTTP_DELIM
        "Content-Type: text/html" HTTP_DELIM
        "Content-Length: " << body.size() << HTTP_DELIM_BODY
        << body;
    return o.str();
}


boost::asio::io_service sservice;

boost::asio::io_service& service()
{
    return sservice;
}

namespace coro {

TLS const Error* t_error;
TLS Coro* t_coro = NULL;
const size_t STACK_SIZE = 1024*32;

// return the control from the coroutine
void yield()
{
    VERIFY(isInsideCoro(), "yield() outside coro");
    t_coro->yield0();
}

// check if we are inside the coroutine
bool isInsideCoro()
{
    return t_coro != nullptr;
}

Coro::Coro()
{
    init0();
}

Coro::Coro(Handler handler)
{
    init0();
    start(std::move(handler));
}

Coro::~Coro()
{
    if (isStarted())
        LOG("Destroying started coro");
}

// resume the coroutine after yield
void Coro::resume()
{
    VERIFY(started, "Cannot resume: not started");
    VERIFY(!running, "Cannot resume: in running state");
    jump0();
}

// check if the coroutine is still running
bool Coro::isStarted() const
{
    return started || running;
}

void Coro::init0()
{
    started = false;
    running = false;
    context = nullptr;
    stack.resize(STACK_SIZE);
}

// go to the preserved context
void Coro::yield0()
{
    boost::context::jump_fcontext(context, &savedContext, 0);
}

void handleError()
{
    if (t_error)
        throw boost::system::system_error(*t_error, "synca");
}

void Coro::starter0(intptr_t p)
{
    started = true;
    try
    {
        exc = nullptr;
        std::function<void ()> handler = std::move(*reinterpret_cast<std::function<void ()>*>(p));
        handler();
    }
    catch (...)
    {
        exc = std::current_exception();
    }
    started = false;
    yield0();
}

void Coro::jump0(intptr_t p)
{
    Coro* old = this;
    std::swap(old, t_coro);
    running = true;
    boost::context::jump_fcontext(&savedContext, context, p);
    running = false;
    std::swap(old, t_coro);
    if (exc != std::exception_ptr())
    std::rethrow_exception(exc);
}

void Coro::starterWrapper0(intptr_t p)
{
    t_coro->starter0(p);
}

void Coro::start(Handler handler)
{
    VERIFY(!isStarted(), "Trying to start already started coro");
    context = boost::context::make_fcontext(&stack.back(), stack.size(), &starterWrapper0);
    jump0(reinterpret_cast<intptr_t>(&handler));
}
}

TLS CoroHandler* t_deferHandler;

void onCoroComplete(coro::Coro* coro)
{
    VERIFY(!coro::isInsideCoro(), "Complete inside coro");
    VERIFY(coro->isStarted() == (t_deferHandler != nullptr), "Unexpected condition in defer/started state");
    if (t_deferHandler != nullptr)
    {
        LOG("invoking defer handler");
        (*t_deferHandler)(coro);
        t_deferHandler = nullptr;
        LOG("completed defer handler");
    }
    else
    {
        LOG("nothing to do, deleting coro");
        delete coro;
    }
}

TLS const Error* t_error;

void handleError()
{
    if (t_error)
        throw boost::system::system_error(*t_error, "synca");
}

void defer(CoroHandler handler)
{
    VERIFY(coro::isInsideCoro(), "defer() outside coro");
    VERIFY(t_deferHandler == nullptr, "There is unexecuted defer handler");
    t_deferHandler = &handler;
    coro::yield();
    handleError();
}

void goSync(std::function<void ()> handler)
{
    LOG("sync::go");
    std::thread([handler] {
        try
        {
            LOG("new thread had been created");
            handler();
            LOG("thread was ended successfully");
        }
        catch (std::exception& e)
        {
            LOG("thread was ended with error: " << e.what());
        }
    }).detach();
}

void goAsync(Handler handler)
{
    LOG("async::go");
    service().post(std::move(handler));
}

void go(Handler handler)
{
    LOG("synca::go");
    goAsync([handler] {
        coro::Coro* coro = new coro::Coro(std::move(handler));
        onCoroComplete(coro);
    });
}

void run()
{
    service().run();
}

void dispatch(int threadCount)
{
    int threads = threadCount > 0 ? threadCount : int(std::thread::hardware_concurrency());
    LOG("Threads: " << threads);
    for (int i = 1; i < threads; ++ i)
        goSync(run);
    run();
}

Acceptor::Acceptor(int port) :
    acceptor(service(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
{
}

void onComplete(coro::Coro* coro, const Error& error)
{
    LOG("async completed, coro: " << coro << ", error: " << error.message());
    VERIFY(coro != nullptr, "Coro is null");
    VERIFY(!coro::isInsideCoro(), "Completion inside coro");
    t_error = error ? &error : nullptr;
    coro->resume();
    LOG("after resume");
    onCoroComplete(coro);
}

IoHandler onCompleteHandler(coro::Coro* coro)
{
    return [coro](const Error& error) {
        onComplete(coro, error);
    };
}

IoHandler onCompleteGoHandler(coro::Coro* coro, Handler handler)
{
    return [coro, handler](const Error& error) {
        if (!error) {
            go(handler);
        }
        onComplete(coro, error);
    };
}

//void Acceptor::accept(Socket& socket)
//{
//    VERIFY(coro::isInsideCoro(), "accept must be called inside coro");
//    defer([this, &socket](coro::Coro* coro) {
//        VERIFY(!coro::isInsideCoro(), "accept completion must be called outside coro");
//        acceptor.accept(socket.socket, onCompleteHandler(coro));
//        LOG("accept scheduled");
//    });
//}

void Acceptor::goAccept(Handler handler)
{
    VERIFY(coro::isInsideCoro(), "goAccept must be called inside coro");
    defer([this, handler](coro::Coro* coro) {
        VERIFY(!coro::isInsideCoro(), "goAccept completion must be called outside coro");
        Socket* socket = new Socket;
        acceptor.async_accept(socket->socket, onCompleteGoHandler(coro, [socket, handler] {
            Socket s = std::move(*socket);
            delete socket;
            handler(s);
        }));
        LOG("accept scheduled");
    });
}

Socket::Socket() :
    socket(service())
{
}

Socket::Socket(Socket&& s) :
    socket(std::move(s.socket))
{
}

void Socket::read(Buffer& buffer, IoHandler handler)
{
    boost::asio::async_read(socket, boost::asio::buffer(&buffer[0], buffer.size()),
        [&buffer, handler](const Error& error, std::size_t) {
            handler(error);
    });
}

void Socket::readSome(Buffer& buffer)
{
    VERIFY(coro::isInsideCoro(), "readSome must be called inside coro");
    defer([this, &buffer](coro::Coro* coro) {
        VERIFY(!coro::isInsideCoro(), "readSome completion must be called outside coro");
        readSome(buffer, onCompleteHandler(coro));
        LOG("readSome scheduled");
    });
}

void Socket::readUntil(Buffer& buffer, Buffer until)
{
    VERIFY(coro::isInsideCoro(), "readUntil must be called inside coro");
    defer([this, &buffer, until](coro::Coro* coro) {
        VERIFY(!coro::isInsideCoro(), "readUntil completion must be called outside coro");
        readUntil(buffer, std::move(until), onCompleteHandler(coro));
        LOG("readUntil scheduled");
    });
}

void Socket::write(const Buffer& buffer)
{
    VERIFY(coro::isInsideCoro(), "write must be called inside coro");
    defer([this, &buffer](coro::Coro* coro) {
        VERIFY(!coro::isInsideCoro(), "write completion must be called outside coro");
        write(buffer, onCompleteHandler(coro));
        LOG("write scheduled");
    });
}

void Socket::readSome(Buffer& buffer, IoHandler handler)
{
    socket.async_read_some(boost::asio::buffer(&buffer[0], buffer.size()),
        [&buffer, handler](const Error& error, std::size_t bytes) {
            buffer.resize(bytes);
            handler(error);
    });
}

bool hasEnd(size_t posEnd, const Buffer& b, const Buffer& end)
{
    return posEnd >= end.size() &&
        b.rfind(end, posEnd - end.size()) != std::string::npos;
}

void Socket::readUntil(Buffer& buffer, Buffer until, IoHandler handler)
{
    VERIFY(buffer.size() >= until.size(), "Buffer size is smaller than expected");
    struct UntilHandler
    {
        UntilHandler(Socket& socket_, Buffer& buffer_, Buffer until_, IoHandler handler_) :
            offset(0),
            socket(socket_),
            buffer(buffer_),
            until(std::move(until_)),
            handler(std::move(handler_))
        {
        }

        void read()
        {
            LOG("read at offset: " << offset);
            socket.socket.async_read_some(boost::asio::buffer(&buffer[offset], buffer.size() - offset), *this);
        }

        void complete(const Error& error)
        {
            handler(error);
        }

        void operator()(const Error& error, std::size_t bytes)
        {
            if (!!error)
            {
                return complete(error);
            }
            offset += bytes;
            VERIFY(offset <= buffer.size(), "Offset outside buffer size");
            LOG("buffer: '" << buffer.substr(0, offset) << "'");
            if (hasEnd(offset, buffer, until))
            {
                // found end
                buffer.resize(offset);
                return complete(error);
            }
            if (offset == buffer.size())
            {
                LOG("not enough size: " << buffer.size());
                buffer.resize(buffer.size() * 2);
            }
            read();
        }

    private:
        size_t offset;
        Socket& socket;
        Buffer& buffer;
        Buffer until;
        IoHandler handler;
    };
    UntilHandler(*this, buffer, std::move(until), std::move(handler)).read();
}

void Socket::write(const Buffer& buffer, IoHandler handler)
{
    boost::asio::async_write(socket, boost::asio::buffer(&buffer[0], buffer.size()),
        [&buffer, handler](const Error& error, std::size_t) {
            handler(error);
    });
}

void Socket::close()
{
    socket.close();
}

void Server::bind(int port) {

    Acceptor acceptor(port);

    LOG("accepting");
    go([&acceptor] {
        while (true)
        {
            acceptor.goAccept([](Socket& socket) {
                try
                {
                    Buffer buffer;
                    while (true)
                    {
                        buffer.resize(4000);
                        socket.readUntil(buffer, HTTP_DELIM_BODY);
                        socket.write(httpContent("<h1>Hello synca!</h1>"));
                    }
                }
                catch (std::exception& e)
                {
                    LOG("error: " << e.what());
                }
            });
        }
    });
    dispatch();

}



