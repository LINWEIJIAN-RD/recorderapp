#ifndef _SEPCAM_RECORDER_H
#define _SEPCAM_RECORDER_H

#include "os_ini.h"
#include "stream_frame_info.h"
#include "ipcnet_struct.h"

// added by lishun
#include "file_list.h"

// #define RECORDER_DEBUG
#define RECORDER_LOG 0
#define RECORDER_LOG2 0
// end by lishun

#define RECORD_TASK_ID 0x1000

// add by lwj
#define RECORDER_PATH "/mnt/s0/media/sensor0"
// end by lwj

typedef enum
{
    LOCAL_MSG_ALARM_DETECT_E = RECORD_TASK_ID + 1,
    LOCAL_MSG_STOP_RECORDER, //AV_RECORDER_MSG_STOP_RECORDER
    LOCAL_MSG_RECORDER_ON_OFF,
    LOCAL_MSG_RECORD_START,
    LOCAL_MSG_RECORD_STOP,
    LOCAL_MSG_REC_TIME_INFO,
    LOCAL_MSG_REC_CHANGE_TIMEZONE,
}RECORDER_TASK_MSG_e;

typedef struct
{
    STREAM_FRAME_INFO_t frameinfo;
    char *es;
    int eslen;
}AV_RECORDER_FILE_INFO_t;

typedef struct
{
    int flag; //是否已经回放
    IPCNET_RECORD_COMMAND_e status;     // command type;
    char stream_shm[32];
    //FILE *fp;
    int fd;
    AV_RECORDER_FILE_INFO_t file_info;
    uint32_t event_id; //进程间通信事件ID
    int block;

    int speed_flag;
    int play_speed;
    int play_slow;

    int seek_flag;          // 0: no jump   1: jump;
    int seek_secs;          // jump to time stamp;
    // int dur_secs;
    // added by lishun
    int offset;         // offset by bytes from the start of the file;
    MyDate  date_s;
    MyTime  time_s;
    // end by lishun

    //add by lwj
    // int recorder_date_index;    //0 :cur date info  1~30:other date info
    // int recorder_playing_fn;    //playing file number
    // add end by lwj
    mutex_t lock;
}AV_RECORDER_SERVER_t;

typedef struct
{
	int16_t Year;
    int8_t Mon;
    int8_t Day;
    int8_t WDay;
	int8_t Hour;
    int8_t Min;
    int8_t Sec;
}AV_RECORDER_TIME_t;

AV_RECORDER_SERVER_t *get_av_recorder_serv(int recorder_id);
int get_rec_file_by_time(int vi_ch,int type,AV_RECORDER_TIME_t *ptm,char *pfile,int size);
int create_replay_recorder_task(int recorder_id);
void parse_string_time(IPCNetTimeSimple_t*it, char*str);
int get_record_package_seconds(int time_index);

int init_recorder_server(char *process_name);
int regitster_recorder_task();

// int notify_recorder_task(uint32_t local_module,uint32_t cmd_type, void *msg, int32_t msg_len);
int get_reboot_flg();
int set_reboot_flg(int val);
int get_mmc_format_status();
int set_mmc_format_status(int val);

// added by lishun
// int save_video_list_to_json(VideoFileInfo_t *videoList, char *jsFile);
// int load_video_list_from_json(VideoFileInfo_t *videoList, char *jsFile);
// int save_alarm_record_list(char *path, IPCNETAlarmRecordList_st *alarmRecordList);
// int load_alarm_record_list(int date, int pageNum, IPCNETAlarmRecordList_st *alarmRecordList);
// end by lishun

// add by lwj
VideoFileInfo_t *getVideolistForOtherDate(int date);
VideoFileInfo_t *getVideolistForDate(int date, int *dateIndex);
VideoFileInfo_t *getCurVideolist(int index);
// end by lwj

#endif

