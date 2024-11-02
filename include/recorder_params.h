#ifndef _RECORDER_PARAMS_H
#define _RECORDER_PARAMS_H

#include "os_ini.h"

#define MAX_VI_CHANNEL_NUM 1
#define DEV_PARAM_INVALID (-1)

// added by lishun
#include "file_list.h"

#define MAX_RECORD_DAYS 30
// end by lishun

//预留最小容量MB
#define MIN_RESERVED_SIZE 50

#define RECORD_FILE_TIME 180 //3*60 //add by lwj

typedef struct //时间结构
{
    T_U8 hour;
    T_U8 min;
    T_U8 sec;
    T_U8 res;
}DEV_TIME_t;

typedef struct DEV_DEFEND_TIME_INFO //布防时间段
{
    int en;                 // 0: close     1: open
    DEV_TIME_t start1;
    DEV_TIME_t end1;
    DEV_TIME_t start2;
    DEV_TIME_t end2;
}DEV_DEFEND_TIME_INFO_t;

// adden by lwj
typedef struct DEV_DEFEND_TIME_LIST_INFO //布防时间段
{
    int en;                 // 0: close     1: open
    DEV_DEFEND_TIME_INFO_t record_time_policy[8]//0: 为每天, 1-7对应周日至周六
}DEV_DEFEND_TIME_LIST_INFO_t;
// end by lwj

typedef struct DEV_RECORD_CFG//录像
{
    T_U16 vich;
    T_U16 vech;
    
    T_U8 auto_del;//是否自动删除的标志位
    T_U8 rec_time;//录像时间(这是pkt_tm_options对应的索引)
    T_U8 reserve[2];
    
    T_U32 storage_type;//存取文件的磁盘类型
    int time_type;	/**1-本地时间，0-gmt时间***/
    
    T_U8 pkt_tm_options[8];
    T_U8 pkt_tm_opt_num;
    
    DEV_DEFEND_TIME_LIST_INFO_t record_time_policy_list[3];
    int recMode;    //add by lwj //0：全时录像 1.报警录像
    int enable;     //add by lwj //0:close 1：open
}DEV_RECORD_CFG_t;

// added by lishun
typedef struct {
    VideoFileInfo_t videoList[MAX_RECORD_DAYS];
    int dateIndex;
    // int curDay;
    int days;
} RECORDER_LIST;

// end by lishun

int dev_set_record_cfg(DEV_RECORD_CFG_t *info_p);
int dev_get_record_cfg(int vich, DEV_RECORD_CFG_t *info_p);
RESULT_t init_record_info();
int get_recorder_auto_del(int vich);
int get_recorder_time_type(int Vich);
int dev_get_record_pack_time_idx(int vich, int *p_min, int *p_max);

//char get_rec_min(int vi);
//char get_rec_max(int vi);


#define TEST_DISK_RESERVED_FILE "/tmp/test_recycle.ini"
int get_reserved_size_from_file_for_test();

#endif
