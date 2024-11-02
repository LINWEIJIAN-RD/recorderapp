#ifndef _RECORDER_UTILS_H_
#define _RECORDER_UTILS_H_

#include "recorder_params.h"
#include "task_mgr.h"
#include "ipcnet_struct.h"
#include "recorder_defs.h"
#include "av_packet_ps.h"

// #define RECORDER_DEBUG

#ifdef MAX_PATH
#undef MAX_PATH
#endif
#define MAX_PATH	260

#define MAX_CMD     256

#define MAX_NAME    32

#define MAX_UNITS   60

#define UNITS_A_DAY  8640

#define RECORDER_REC_END 1
#define RECORDER_IS_RECORDING 2
#define RECORDER_GET_NO_FRAME 3

// #define CHECK_RECORD_TASK_TIMESOUT 5*100
#define CHECK_RECORD_TASK_TIMESOUT 10*100


#define RECORD_NWRITE_LEN (64*1024)//(16*1024)//(512*8)

#define REC_FILE_LEN_BY_MINS        10
#define REC_UNIT_LEN_BY_SECS        10


#define    RECORDER_COLSED  0
#define    RECORDER_COMMON  1
#define    RECORDER_ALARM   2

typedef struct {
    unsigned char     type;          // 0: 关闭     1: 全时录像     2: 报警录像
    // alarmed:前4个bit用于标记单位录像里面的偏移时间，单位录像目前是10S,通过这个偏移可以快速找到报警是在10S里的哪时刻
    unsigned char     alarmed;       // 0: not alarmed    3:人形侦测报警录像   4:人脸抓拍 
    unsigned int      offset;        // offset by bytes;
} RECORDER_UNIT;

typedef struct {
    RECORDER_UNIT   recUnits[UNITS_A_DAY];
    MyDate          date_s;
    bool            bLoaded;
} SEPCAM_PLAYBACK_STATUS_s;

typedef enum {
    STOP_BY_ALARM_STOP = 1,
    STOP_BY_TIME_OUT,
    STOP_BY_RECORD_DISABLE
} STOP_REC_TYPE;        // close record files;

typedef enum {
    START_BY_ALARM_START = 1,
    START_BY_TIME_IN,
    START_BY_RECORD_ENABLE
} START_REC_TYPE;       // open record files;

typedef struct {
    unsigned char     alarmed;       // 0: not alarmed     1: alarmed
    MyTime            lastAlarmTime;
} ALARM_STATUS;

// added by lwj

typedef struct 
{
    unsigned char     en;         // 0: close     1: open
    MyTime recTime_s;
    MyTime recTime_e;
}TIME_INFO;

typedef struct {
    unsigned char     en;         // 0: close     1: open
    TIME_INFO timeInfo[7]       //0~6,星期天是0      
} RECORDER_TIME_LIST;
// end by lwj

typedef struct {
    unsigned char     type;         // 0: 关闭     1: 全时录像     2: 报警录像
    // MyTime            recTime_s;
    // MyTime            recTime_e;
    RECORDER_TIME_LIST recTimeList[3];
} RECORDER_TYPE;

// typedef struct
// {
//     char    year;       //20, 21, 22
//     char    mon;
//     char    day;
//     // char    recName[MAX_NAME];
    
// }RECORD_DAY_NIGHT_INDEX;

typedef struct
{
    RECORDER_UNIT       recMinUnits[MAX_UNITS];
    int                 curFileLen;
    MyTime              unitTime_s;          // sec: 0, 10, 20, 30, 40, 50;
    MyTime              fileTime_s;          // min: 0, 10, 20, 30, 40, 50; sec: 0;
}RECORD_REC_FILE_INDEX;

typedef struct
{
    char                        rec_dir[MAX_PATH];
    int                         rec_fd;
    char                        rec_name[MAX_NAME]; // without .idx or .pes

    RECORDER_TYPE               recType;
    ALARM_STATUS                alarmStat;
    // RECORD_DAY_NIGHT_INDEX      aDayNightIdx;     // [0, 143] in 24 hours
    RECORD_REC_FILE_INDEX       aRecFileIdx;      // [0, 59] in 10 minutes;

    mutex_t sLock;

    UINT64_t        vstart_timestamp;
    UINT64_t        astart_timestamp;
    
    char            *enc_buf;
    int             buf_len;
    RDWR_BUFF_t     *wr_buf;
    int             steam_shm_init;	/**是否已经初始化共享内存0-未初始化，1-已经初始化**/
} SEPCAM_RECORDER_STATUS_s;

typedef enum {
    ERROR = 0,
    NO_RECORD,
    RECORDING
} RECORD_STATUS;

RECORD_STATUS check_and_start_record(struct THR_TASK * ptask, EVENT_HANDLE_LIST_t * handler_list);

RECORD_STATUS check_and_stop_record(struct THR_TASK * ptask, EVENT_HANDLE_LIST_t * handler_list);

int get_all_list_in_a_day(RECORDER_UNIT *recUnitsADay, MyDate *date_s, MyTime *time_s, MyTime *time_e, int retN, IPCNetAvRecFileInfo_t *recInfos);

int get_alarm_list_in_a_day(RECORDER_UNIT *recUnits, MyDate *date_s, MyTime *time_s, MyTime *time_e, int retN, IPCNetAvRecFileInfo_t *recInfos);

int get_record_list_in_a_day(RECORDER_UNIT *recUnits, MyDate *date_s, MyTime *time_s, MyTime *time_e, int retN, IPCNetAvRecFileInfo_t *recInfos);

char *get_record_dir_by_date(char *root_dir, MyDate *pDate, bool bCreat);

char *get_record_name_by_time(MyTime *pTime);

char *get_rec_root();

void int_to_time(int time_i, MyTime *time_s);
void int_to_date(int date_i, MyDate *date_s);
int time_to_int(MyTime *time_s);
int date_to_int(MyDate *date_s);

int load_rec_units_a_day(char *root_dir, MyDate *pDate, RECORDER_UNIT recUnits[]);

int get_offset_by_time(MyTime *pTime, RECORDER_UNIT *recUnitsInADay);

void get_file_time_by_time(MyTime *pTime, MyTime *pTime_s);

int get_next_file_time_by_time(MyTime *pTime, MyTime *pTime_s);  // ret: 0-current day, 1-next day, -1-error;

void set_record_type(int vencCh, RECORDER_TYPE *recType);

// int save_idx_file(const char *idxName);

// int load_idx_file(const char *idxName, RECORDER_UNIT  *recUnits);

time_t get_local_time_in_secs(int timeZone);

void get_local_time(int timeZone, IPCNetTimeInfo_st *pTimeInfo);

int save_rec_units(const char *idxPath);

int load_rec_units(const char *idxPath, RECORDER_UNIT  *recUnits);

void get_a_day_time_by_unit_idx(int idx, MyTime *pTime);

int get_a_day_unit_idx_by_time(MyTime *pTime);
// added by lwj
int notify_recorder_task(uint32_t local_module,uint32_t cmd_type, void *msg, int32_t msg_len);
// end by lwj

#endif