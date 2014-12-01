//
//  main.cpp
//  ag-client
//
//  Created by crobertson on 11/28/14.
//  Copyright (c) 2014 crobertson. All rights reserved.
//

#include <iostream>
#include <zmq.hpp>
#include <zhelpers.hpp>

class client_task
{
public:
    client_task() :
        ctx_(1)
        , client_socket_(ctx_, ZMQ_DEALER)
    {
        /* */
    }

    void start()
    {
        // generate random identity
        char identity[10] = {};
        sprintf(identity, "%04X-%04X", within(0x10000), within(0x10000));
        printf("%s\n", identity);
        client_socket_.setsockopt(ZMQ_IDENTITY, identity, strlen(identity));
        client_socket_.connect("tcp://localhost:5570");

        zmq::pollitem_t items[] = {{client_socket_, 0, ZMQ_POLLIN, 0}};
        int request_nbr = 0;
        try {
            while (true)
            {
                for (int i = 0; i < 100; ++i)
                {
                    // 10 milliseconds
                    zmq::poll(items, 1, 10);
                    if (items[0].revents & ZMQ_POLLIN)
                    {
                        printf("\n%s ", identity);
                        s_dump(client_socket_);
                    }
                }
                char request_string[16] = {};
                sprintf(request_string, "request #%d", ++request_nbr);
                client_socket_.send(request_string, strlen(request_string));
            }
        }
        catch (std::exception &e) {}
    }

private:
    zmq::context_t ctx_;
    zmq::socket_t client_socket_;
};

int main(int argc, const char * argv[])
{
#if 0
    srandom((unsigned int)time(NULL));
    client_task ctask;
    ctask.start();
    return NULL;
#else
    //  Prepare our context and socket
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);

    std::cout << "Connecting to hello world server..." << std::endl;
    socket.connect("tcp://localhost:5555");

    //  Do 10 requests, waiting each time for a response
    // for (int request_nbr = 0; request_nbr != 10; request_nbr++)
    int request_nbr = 0;
    while (1)
    {
        zmq::message_t request(100);
        memcpy((void *)request.data(), argv[1], strlen(argv[1]));
        std::cout << "Sending  " << request_nbr << "..." << std::endl;
        socket.send (request);

        //  Get the reply.
        zmq::message_t reply;
        char buffer[1024];
        int len = 0;
        if (socket.recv(&reply))
        {
            len = reply.size();
            memcpy(buffer, reply.data(), len);
            buffer[len] = '\0';
        }
        else
        {
            buffer[0] = '\0';
        }
        std::cout << "Received " << request_nbr << ": \"" << buffer << "\"" << std::endl;
        request_nbr++;
    }
#endif
    return 0;
}