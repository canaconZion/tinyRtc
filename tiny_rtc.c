#include "tiny_rtc.h"

int trtc_rtc_open(int *state)
{
    int ret = 0;
    trtc_log("tiny rtc running");
    // TODO: 添加状态机，开启前判断状态，防止重复开启
    return ret;
}

void trtc_rtc_close()
{
    trtc_log("tiny rtc closed");
    // TODO: 添加状态机，防止重复关闭
    return;
}