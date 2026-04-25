#include "platform.h"
#include "tiny_rtc.h"
int main()
{
    int ret;
    int trtc_rtc_state = 0;
    ret = trtc_rtc_open(&trtc_rtc_state);
    while (!trtc_rtc_state)
    {
        trtc_thread_sleep(3 * 1000);
        trtc_log("waiting");
    }
    return 0;
}