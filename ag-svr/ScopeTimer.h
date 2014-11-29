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
#include <ctime>
#include <iostream>

namespace Timing
{
    
typedef std::chrono::high_resolution_clock HRClockType;
typedef std::chrono::high_resolution_clock::time_point HRTimeStamp;
typedef std::chrono::system_clock SysClockType;
typedef std::chrono::system_clock::time_point SysTimeStamp;

    
class ScopeTimer
{
public:
    
    ScopeTimer()
        : m_start(HRClockType::now())
    {
        /* */
    }
    
    ~ScopeTimer()
    {
        HRTimeStamp stop = HRClockType::now();
        const float elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - m_start).count();
        std::cout << elapsed << "ns" << std::endl;
    }
    
private:
    const HRTimeStamp m_start;
};

HRTimeStamp hrNow()
{
    return HRClockType::now();
}

float elapsedinUS(HRTimeStamp start)
{
    float elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(hrNow() - start).count();
    return elapsed;
}

char* sysNow(char* ctimestr)
{
    SysTimeStamp now = SysClockType::now();
    std::time_t tt = SysClockType::to_time_t (now);
    return ctime_r(&tt, ctimestr);
}
  
} // namespace Timing

#endif


