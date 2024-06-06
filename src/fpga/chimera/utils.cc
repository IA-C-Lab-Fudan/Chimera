#include "utils.hh"

namespace gem5
{
namespace fpga
{

long get_system_time_nanosecond()
{
    struct timespec timestamp = {};
    if (0 == clock_gettime(CLOCK_REALTIME, &timestamp))
        return timestamp.tv_sec * 1000000000 + timestamp.tv_nsec;
    else
        return 0;
}

long get_system_time_microsecond()
{
    struct timeval timestamp = {};
    if (0 == gettimeofday(&timestamp, NULL))
        return timestamp.tv_sec * 1000000 + timestamp.tv_usec;
    else
        return 0;
}

long get_system_time_millisecond()
{
    struct timeb timestamp = {};

    if (0 == ftime(&timestamp))
        return timestamp.time * 1000 + timestamp.millitm;
    else
        return 0;
}

long get_system_time_second()
{
    return time(NULL);
}

} // namespace fpga
} // namespace gem5
