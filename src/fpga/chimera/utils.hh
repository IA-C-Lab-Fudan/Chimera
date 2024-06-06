#ifndef __FPGA_CHIMERA_UTILS_HH__
#define __FPGA_CHIMERA_UTILS_HH__

#include <stdio.h>
#include <string.h>
#include <sys/time.h>  
#include <sys/timeb.h> 
#include <time.h>      
#include <unistd.h>

namespace gem5
{
namespace fpga
{

long get_system_time_nanosecond(); 
long get_system_time_microsecond();
long get_system_time_millisecond();
long get_system_time_second();     

} // namespace fpga
} // namespace gem5


#endif