//
//  main.cpp
//  ag-svr
//
//  Created by crobertson on 11/24/14.
//  Copyright (c) 2014 crobertson. All rights reserved.
//

#include <assert.h>
#include <iostream>
#include <mutex>
#include <signal.h>
#include <thread>
#include <vector>
#include <zmq.hpp>
#include <zhelpers.hpp>

#include "ScopeTimer.h"
#include "mg-web/mongoose.h"

namespace // unnamed
{

// globals
sig_atomic_t received_signal = 0;
int mainHz = 10;
int threadHz = 30;
const char *html_form =
    "<html><body>"
    "<img src=\"http://upload.wikimedia.org/wikipedia/en/9/9e/Silver_Surfer_01_cover.jpg\" width=\"200\"><br/>"
    "AG Svr Control Page"
    "<form method=\"POST\" action=\"/handle_post_request\">"
    "main Hz: <input type=\"text\" name=\"input_1\" /> <br/>"
    "thread Hz: <input type=\"text\" name=\"input_2\" /> <br/>"
    "<input type=\"submit\" />"
    "</form></body></html>";
typedef int (*idlefunc)(int);

int fib(int n)
{
    if (n <= 1)
    {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

void* RunThread(void* pVoid)
{
    assert(pVoid);
    std::mutex* pMutex = (std::mutex*)pVoid;

    // setup zeromq
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REP);
    socket.bind("tcp://*:5555");
 
    while (received_signal == 0)
    {
        Timing::HRTimeStamp start = Timing::hrNow();
        const float hzUS = (1.0f / (float)threadHz) * 1000.0f * 1000.0f;
        
        // idle work

        // zmq work
        {
            zmq::message_t request;
            char buffer[1024];
            int len = 0;
            //  Wait for next request from client
            if (socket.recv(&request))
            {
                len = std::min(1023, (int)request.size());
                memcpy(buffer, request.data(), len);
                buffer[len] = '\0';
            }
            else
            {
                buffer[0] = '\0';
            }
            pMutex->lock();
            std::cout << "Received \"" << buffer << "\"" << std::endl;
            pMutex->unlock();

            //  Send reply back to client
            zmq::message_t reply(len);
            memcpy((void *)reply.data(), buffer, len);
            socket.send(reply);
        }

        float elapsed = Timing::elapsedinUS(start);
        if (elapsed < hzUS)
        {
            int sleepAmt = static_cast<int>(hzUS - elapsed);
            pMutex->lock();
            std::cout << "RunThread sleeping: " << sleepAmt << "us\n";
            pMutex->unlock();
            usleep(sleepAmt);
        }
        else
        {
            char ctimestr[26];
            pMutex->lock();
            std::cout << "RunThread: " << Timing::sysNow(ctimestr);
            pMutex->unlock();
        }
    }
    
    pMutex->lock();
    std::cout << "RunThread: exiting\n";
    pMutex->unlock();

    return NULL;
}
 
void LaunchThread(std::mutex* pMutex)
{
    std::thread thread(RunThread, pMutex);
    
    // spin it off
    thread.detach();
 
    pMutex->lock();
    std::cout << "Exiting LaunchThread\n";
    pMutex->unlock();
}

void send_reply(struct mg_connection *conn)
{
    char var1[500], var2[500];

    if (strcmp(conn->uri, "/handle_post_request") == 0)
    {
        // User has submitted a form, show submitted data and a variable value
        // Parse form data. var1 and var2 are guaranteed to be NUL-terminated
        mg_get_var(conn, "input_1", var1, sizeof(var1));
        mg_get_var(conn, "input_2", var2, sizeof(var2));
        mainHz = std::min(60, std::max(1, atoi(var1)));
        threadHz = std::min(60, std::max(1, atoi(var2)));

        // Send reply to the client, showing submitted form values.
        // POST data is in conn->content, data length is in conn->content_len
        mg_send_header(conn, "Content-Type", "text/plain");
        mg_printf_data(conn,
                   "Submitted data: [%.*s]\n"
                   "Submitted data length: %d bytes\n"
                   "mainHz: [%d]\n"
                   "threadHz: [%d]\n",
                   conn->content_len, conn->content,
                   conn->content_len, mainHz, threadHz);
    }
    else
    {
        // Show HTML form.
        mg_send_data(conn, html_form, strlen(html_form));
    }
}

int ev_handler(struct mg_connection *conn, enum mg_event ev)
{
    switch (ev)
    {
        case MG_AUTH:
            return MG_TRUE;
        case MG_REQUEST:
            send_reply(conn);
            return MG_TRUE;
        default:
            return MG_FALSE;
    }
}

void signal_handler(int sig_num)
{
    received_signal = sig_num;
}

// FOR ASYNCHRONOUS CLIENT - SERVER

//  Each worker task works on one request at a time and sends a random number
//  of replies back, with random delays between replies:
class server_worker
{
public:
    server_worker(zmq::context_t &ctx, int sock_type) :
        ctx_(ctx)
        , worker_(ctx_, sock_type)
    {
        /* */
    }

    void work()
    {
        worker_.connect("inproc://backend");

        try
        {
            while (true)
            {
                zmq::message_t identity;
                zmq::message_t msg;
                zmq::message_t copied_id;
                zmq::message_t copied_msg;
                worker_.recv(&identity);
                worker_.recv(&msg);

                int replies = within(5);
                for (int reply = 0; reply < replies; ++reply)
                {
                    s_sleep(within(1000) + 1);
                    copied_id.copy(&identity);
                    copied_msg.copy(&msg);
                    worker_.send(copied_id, ZMQ_SNDMORE);
                    worker_.send(copied_msg);
                }
            }
        }
        catch (std::exception &e) {}
    }

private:
    zmq::context_t& ctx_;
    zmq::socket_t worker_;
};

//  This is our server task.
//  It uses the multithreaded server model to deal requests out to a pool
//  of workers and route replies back to clients. One worker can handle
//  one request at a time but one client can talk to multiple workers at
//  once.

class server_task
{
public:
    server_task() :
        ctx_(1)
        , frontend_(ctx_, ZMQ_ROUTER)
        , backend_(ctx_, ZMQ_DEALER)
    {
        /* */
    }

    enum { kMaxThread = 5 };

    void run()
    {
        frontend_.bind("tcp://*:5570");
        backend_.bind("inproc://backend");

        std::vector<server_worker *> worker;
        std::vector<std::thread *> worker_thread;
        for (int i = 0; i < kMaxThread; ++i)
        {
            worker.push_back(new server_worker(ctx_, ZMQ_DEALER));

            worker_thread.push_back(new std::thread(std::bind(&server_worker::work, worker[i])));
            worker_thread[i]->detach();
        }


        try
        {
            zmq::proxy(frontend_, backend_, nullptr);
        }
        catch (std::exception &e) {}

        for (int i = 0; i < kMaxThread; ++i)
        {
            delete worker[i];
            delete worker_thread[i];
        }
    }

private:
    zmq::context_t ctx_;
    zmq::socket_t frontend_;
    zmq::socket_t backend_;
};

}; // namespace unnamed

int main(int argc, const char * argv[])
{
    std::mutex mutex;
    Timing::HRTimeStamp start;

    // setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // setup mongoose
    struct mg_server* webserver = mg_create_server(NULL, ev_handler);
    mg_set_option(webserver, "listening_port", "8080");
    
    // create thread stuff
    LaunchThread(&mutex);
    
    while (received_signal == 0)
    {
        Timing::HRTimeStamp start = Timing::hrNow();
        const float hzUS = (1.0f / (float)mainHz) * 1000.0f * 1000.0f;

        // main work
        {
            mg_poll_server(webserver, 1000);
        }
        
        float elapsed = Timing::elapsedinUS(start);
        if (elapsed < hzUS)
        {
            int sleepAmt = static_cast<int>(hzUS - elapsed);
            mutex.lock();
            std::cout << "MainThread sleeping: " << sleepAmt << "us\n";
            mutex.unlock();
            usleep(sleepAmt);
        }
        else
        {
            char ctimestr[26];
            mutex.lock();
            std::cout << "MainThread : " << Timing::sysNow(ctimestr);
            mutex.unlock();
        }
    }
    
    // destroy mongoose
    mg_destroy_server(&webserver);

    std::cout << "Exiting Main\n";
    return 0;
}

