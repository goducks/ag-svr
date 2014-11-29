//
//  main.cpp
//  ag-client
//
//  Created by crobertson on 11/28/14.
//  Copyright (c) 2014 crobertson. All rights reserved.
//

#include <zmq.hpp>
#include <iostream>

int main(int argc, const char * argv[])
{
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
    return 0;
}