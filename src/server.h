
#ifndef SRC_SERVER_H_
#define SRC_SERVER_H_

#include <functional>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/context/all.hpp>
#include <thread>
#include <exception>
#include <stdexcept>

typedef boost::system::error_code Error;
typedef std::function<void(const Error&)> IoHandler;
typedef std::string Buffer;

class Socket;
typedef std::function<void()> Handler;

namespace coro {

#ifndef __unix__
#   define TLS  __declspec(thread)
#else
#   define TLS thread_local
#endif

// leave the coroutine
void yield();

// check if we are inside the coroutine
bool isInsideCoro();

// coroutine
class Coro
{
public:
    // just in case
    friend void yield();

    Coro();

    // create and invoke the handler
    Coro(Handler);

    // no comments
    ~Coro();

    // run the handler
    void start(Handler);

    // resume the coroutine's work (only when it was completed with yield)
    void resume();

    // check if the coroutine can be resumed
    bool isStarted() const;

private:
    void init0();
    void yield0();
    void jump0(intptr_t p = 0);
    static void starterWrapper0(intptr_t p);
    void starter0(intptr_t p);

    bool started;
    bool running;

    boost::context::fcontext_t * context;
    boost::context::fcontext_t savedContext;
    std::vector<unsigned char> stack;
    std::exception_ptr exc;

};
}

typedef std::function<void(coro::Coro*)> CoroHandler;

void defer(CoroHandler handler);

IoHandler onCompleteGoHandler(coro::Coro* coro, Handler handler);

class Acceptor;
class Socket
{
    friend struct Acceptor;

public:
    Socket();
    Socket(Socket&&);

    void read(Buffer&, IoHandler);
    void readSome(Buffer&);
    void readUntil(Buffer&, Buffer until);
    void write(const Buffer&);
    void readSome(Buffer&, IoHandler);
    void readUntil(Buffer&, Buffer until, IoHandler);
    void write(const Buffer&, IoHandler);
    void close();

private:
    boost::asio::ip::tcp::socket socket;
};

class Acceptor {

typedef std::function<void(Socket&)> Handler;

public:
    explicit Acceptor(int port);
    ~ Acceptor() {};

    void goAccept(Handler handler);
    void go(Handler handler);
    //void accept(Socket& socket);

private:
    boost::asio::ip::tcp::acceptor acceptor;
};

class Server {

public:
    Server() {};
    ~Server() {};

    void bind(int port);
};

void run();
void go(Handler handler);
void goAsync(Handler handler);
void goSync(Handler handler);
void dispatch(int threadCount = 0);


#endif /* SRC_SERVER_H_ */
