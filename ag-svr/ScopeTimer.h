//
//  ScopeTimer.h
//  ag-svr
//
//  Created by crobertson on 11/24/14.
//  Copyright (c) 2014 crobertson. All rights reserved.
//

#ifndef ag_svr_ScopeTimer_h
#define ag_svr_ScopeTimer_h

#include <chrono>
#include <iostream>

namespace Timing
{
    
typedef std::chrono::high_resolution_clock ClockType;
typedef std::chrono::high_resolution_clock::time_point TimeStamp;
    
class ScopeTimer
{
public:
    
    ScopeTimer()
        : m_start(ClockType::now())
    {
        /* */
    }
    
    ~ScopeTimer()
    {
        TimeStamp stop = ClockType::now();
        const float elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - m_start).count();
        std::cout << elapsed << "ns" << std::endl;
    }
    
private:
    const TimeStamp m_start;
};

TimeStamp now()
{
    return ClockType::now();
}

float elapsedinUS(TimeStamp start)
{
    float elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(now() - start).count();
    return elapsed;
}

#endif


