#ifndef __SEP_FILE_LIST_H
#define __SEP_FILE_LIST_H

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/vfs.h>
#include "cJSON.h"


#define MAX_VIDEOS_IN_A_DAY 1440

typedef struct{
	unsigned long long size;
	unsigned long long  free;
	unsigned long long  available;
}disk_info_t;

typedef struct
{
    int t; //DT_DIR:文件夹，DT_REG: 普通文件
    char n[24]; //filename
    int sl; // size 低8位
    int sh; // size 高8位
}FileInfo_t;

//add by lwj
typedef struct
{
    int stTime; //开始时间
    int eTime; // 结束时间
}VideoFileDayInfo_t;

typedef struct
{
    VideoFileDayInfo_t fileList[MAX_VIDEOS_IN_A_DAY];
    int date;
    int fileTotal;
}VideoFileInfo_t;
//add end by lwj

// int list_file_to_jsonstr(char*buf,int len,int mode,int start,int count,const char*dir_path,const char*dir_name,int recurse);
cJSON * list_file(const char*dir_path,const char*dir_name,int recurse);
int get_folder_free_space(const char *path,disk_info_t *info);
int get_dir_file_num(const char*path,int mode,int start,int end);

//add by lwj
int list_file_to_jsonstr(FileInfo_t *FileInfo,int len,int mode,int start,int count,const char*dir_path,const char*dir_name,int recurse);
int get_dir_file_list(FileInfo_t *FileInfo, int count, const char*path,int mode,int start,int end);
int getFileListToQueue(FileInfo_t *FileInfo, const char*path, int mode,int start,int end);
int videoFileSeek(VideoFileInfo_t *pFileInfo, int time, char *outFilename, int *dayTimesStamp, int *playingFN);
int videoFileSeek2(VideoFileInfo_t *pFileInfo, int time, char *outFilename ,int *outTime, int *playingFN);
//add end by lwj

// added by lishun
int listVideoFileInDir(char *dirPath, VideoFileInfo_t *FileInfo, int *pVideoCap);
// end by lishun

#endif

