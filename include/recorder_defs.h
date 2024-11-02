#ifndef _RECORDER_DEFS_H_
#define _RECORDER_DEFS_H_

//added by lishun
#include <sys/time.h>

#define IPCNET_RET_RESOURCE_ERROR -2

#define FRAME_TYPE_IS_VIDEO(frame_type) (frame_type<255 ? 1 : 0)

#define FRAME_TYPE_IS_V_KEY(frame_type) (frame_type>4 ? 1 : 0)

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

// end by lishun

#endif