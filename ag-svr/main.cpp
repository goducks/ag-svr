//
//  main.cpp
//  ag-svr
//
//  Created by crobertson on 11/24/14.
//  Copyright (c) 2014 crobertson. All rights reserved.
//

#include <assert.h>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <zmq.hpp>

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
    pthread_mutex_t* pMutex = (pthread_mutex_t*)pVoid;

    // setup zeromq
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REP);
    socket.bind("tcp://*:5555");
 
    while (received_signal == 0)
    {
        Timing::HRTimeStamp start = Timing::hrNow();
        const float hzUS = (1.0f / (float)threadHz) * 1000.0f * 1000.0f;

        // fib(10);

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
            pthread_mutex_lock(pMutex);
            std::cout << "Received \"" << buffer << "\"" << std::endl;
            pthread_mutex_unlock(pMutex);

            //  Send reply back to client
            zmq::message_t reply(len);
            memcpy((void *)reply.data(), buffer, len);
            socket.send(reply);
        }

        float elapsed = Timing::elapsedinUS(start);
        if (elapsed < hzUS)
        {
            int sleepAmt = static_cast<int>(hzUS - elapsed);
            pthread_mutex_lock(pMutex);
            std::cout << "RunThread sleeping: " << sleepAmt << "us\n";
            pthread_mutex_unlock(pMutex);
            usleep(sleepAmt);
        }
        else
        {
            char ctimestr[26];
            pthread_mutex_lock(pMutex);
            std::cout << "RunThread: " << Timing::sysNow(ctimestr);
            pthread_mutex_unlock(pMutex);
        }
    }
    
    pthread_mutex_lock(pMutex);
    std::cout << "RunThread: exiting\n";
    pthread_mutex_unlock(pMutex);

    return NULL;
}
 
void LaunchThread(pthread_mutex_t* pMutex)
{
    pthread_attr_t attr;
    pthread_t posixThreadID;
    
    int returnVal = pthread_attr_init(&attr);
    assert(!returnVal);
    returnVal = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    assert(!returnVal);
 
    int threadError = pthread_create(&posixThreadID, &attr, &RunThread, pMutex);
 
    pthread_mutex_lock(pMutex);
    std::cout << "Exiting LaunchThread\n";
    pthread_mutex_unlock(pMutex);

    returnVal = pthread_attr_destroy(&attr);
    assert(!returnVal);
    if (threadError != 0)
    {
         // Report an error.
    }
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

}; // namespace unnamed

int main(int argc, const char * argv[])
{
    pthread_mutex_t mutex;
    Timing::HRTimeStamp start;

    // setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // setup mongoose
    struct mg_server* webserver = mg_create_server(NULL, ev_handler);
    mg_set_option(webserver, "listening_port", "8080");
    
    // create thread stuff
    pthread_mutex_init(&mutex, NULL);
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
            pthread_mutex_lock(&mutex);
            std::cout << "MainThread sleeping: " << sleepAmt << "us\n";
            pthread_mutex_unlock(&mutex);
            usleep(sleepAmt);
        }
        else
        {
            char ctimestr[26];
            pthread_mutex_lock(&mutex);
            std::cout << "MainThread : " << Timing::sysNow(ctimestr);
            pthread_mutex_unlock(&mutex);
        }
    }
    
    pthread_mutex_destroy(&mutex);
    
    // destroy mongoose
    mg_destroy_server(&webserver);

    std::cout << "Exiting Main\n";
    return 0;
}

