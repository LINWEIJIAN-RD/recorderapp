#ifndef _DISK_MGR_H
#define _DISK_MGR_H
#include "os_ini.h"

#define DEFAULT_MMC_ROOT_PATH "/mnt/s0/"
#define DEFAULT_COMMOM_REC_PATH "/mnt/s0/media/sensor0/"
// #define DEFAULT_ALARM_REC_PATH "/mnt/s0/alarm/sensor0/"

//add by lwj
#define DEFAULT_ALARM_REC_PATH "/customer/tutkipc/alarm/sensor0/"
//add end by lwj

//unit:byte
typedef struct{
	unsigned long long size;	
	unsigned long long  free;
	unsigned long long  available;
}DISK_CAP_INTO_t;

/***用来查询文件，按最后修改时间排序的结构�?**/
#define NAME_LEN 96
typedef struct
{
	long time;
	long byte;		//单位字节
	char name[NAME_LEN];
}DirectoryFileInfo_st;

typedef struct
{
	int counter;
	DirectoryFileInfo_st *pFileStartAddr;
}DirFileListInfo_st;

typedef enum
{
	FILE_TYPE_DIR = 0,	//目录
	FILE_TYPE_REG,		//普通常规文�?
}FileType_st;

/****reserved_size-MB*****/
int recycle_disk_del_day_dir(int reserved_size);

const char *get_alarm_record_path();
const char *get_common_record_path();
const char *get_disk_root_path();
int init_record_path(char *diskRoot, char *commRecRoot, char *alarmRecRoot);
int need_recycle_disk(int reserved_size);
int get_disk_path_capacity_info(const char *path,DISK_CAP_INTO_t *pinfo);

#endif
