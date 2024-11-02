/**
* created by yys(Vincent.Yeh)
*/
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/syscall.h> 
#include <stdbool.h>
#include "os_ini.h"
#include "mempool.h"
#include "msg_queue.h"
#include "event_handle.h"
#include "task_mgr.h"
#include "frame_buff.h"
#include "logServer.h"
#include "atomic.h"
#include "sepcam_ipc_internal.h"
#include "sepcam_ipc_api.h"
#include "sepcam_recorder.h"
#include "recorder_params.h"
#include "disk_mgr.h"

#include "recorder_utils.h"


static pid_t sChildPID = 0;
static int recorder_process_run = 1;
static char *g_recorder_serv_name = SEPCAM_RECORDER_IPC_NAME;
static int g_reboot_flg = 0;

// added by lishun
// static int g_sd_abnormal = 0;

static RECORDER_LIST    g_recorderList;
static atomic_t         g_recorderListEnable;

// end by lishun

// added by lwj
extern int gSDCardStatus;
extern int g_recorder_log_flag;
// extern int AlarmRecorderStatus;
IPCNETAlarmRecordList_st g_alarmRecordList = {0};
// end by lwj

//add by lwj test
#include "file_list.h"
extern VideoFileInfo_t gVideoFileInfo;
extern mutex_t videoFileInfoLock;
//add end by lwj test

int set_reboot_flg(int val)
{
	char cmd[64] = {0};
	g_reboot_flg = val;

	if(0 == val)
		return 0;

	if(0 != access(RECORDER_EXITS_FLAG_FILE,F_OK))	//不存在，创建
	{
		sprintf(cmd,"echo %s>%s","reboot now,exist",RECORDER_EXITS_FLAG_FILE);
	    system(cmd);
    }
	return 0;
}

int get_reboot_flg()
{
	return g_reboot_flg;
}

static int g_format_status = 0;

int set_mmc_format_status(int val)
{
	g_format_status	= val;
	return 0;
}

//1-正在格式化，0-正常
int get_mmc_format_status()
{
	return g_format_status;
}

static void ipc_alarm_callback(IPCNetAlarmMsgReport_st *alarm_msg)
{
	int need_reboot = 0;
    	
    if((IPCNET_SYSTEM_REBOOT == alarm_msg->AlarmType) || (IPCNET_SYSTEM_RESET == alarm_msg->AlarmType))
    {
		LOGI("IPCNET_SYSTEM_REBOOT");
		set_reboot_flg(1);
		notify_recorder_task(0,LOCAL_MSG_STOP_RECORDER,NULL,0);
		recorder_process_run = 0;
        if (g_recorder_log_flag)
        {
            Logfi("system reboot");
            update_logs_mgr();
        }
    }
    else if(IPCNET_MMC_FORMAT == alarm_msg->AlarmType)
    {
		LOGI("IPCNET_MMC_FORMAT,alarm_msg->Val=%d",alarm_msg->Val);
		if(1 == alarm_msg->Val)
		{
			LOGI("MMC start format!!");
			notify_recorder_task(0,LOCAL_MSG_STOP_RECORDER,NULL,0);
			set_mmc_format_status(1);
		}
		else 
		{
			LOGI("MMC format complete!");
			set_mmc_format_status(0);
		}
    }
    #if 0
// added by lishun
    else if(IPCNET_ALARM_MOTIONDETECT_START == alarm_msg->AlarmType)
    {
        LOGI("AlarmType:%d AlarmSta:%d", alarm_msg->AlarmType, alarm_msg->AlarmSta);
        alarm_msg->bStatus = true;
        notify_recorder_task(0, LOCAL_MSG_ALARM_DETECT_E, alarm_msg, sizeof(IPCNetAlarmMsgReport_st));
    }
    else if(IPCNET_ALARM_MOTIONDETECT_END == alarm_msg->AlarmType)
    {
        LOGI("AlarmType:%d AlarmSta:%d", alarm_msg->AlarmType, alarm_msg->AlarmSta);
        alarm_msg->bStatus = false;
        notify_recorder_task(0, LOCAL_MSG_ALARM_DETECT_E, alarm_msg, sizeof(IPCNetAlarmMsgReport_st));
    }
// end by lishun
#else
    else if(IPCNET_ALARM_MOTIONDETECT_STATUS == alarm_msg->AlarmType)
    {
        if (1 == alarm_msg->AlarmSta)
        {
            LOGI("AlarmType:%d AlarmSta:%d", alarm_msg->AlarmType, alarm_msg->AlarmSta);
            alarm_msg->bStatus = true;
            notify_recorder_task(0, LOCAL_MSG_ALARM_DETECT_E, alarm_msg, sizeof(IPCNetAlarmMsgReport_st));
        }
        else
        {
            LOGI("AlarmType:%d AlarmSta:%d", alarm_msg->AlarmType, alarm_msg->AlarmSta);
            alarm_msg->bStatus = false;
            notify_recorder_task(0, LOCAL_MSG_ALARM_DETECT_E, alarm_msg, sizeof(IPCNetAlarmMsgReport_st));
        }
    }
#endif 
    // else if(IPCNET_ALARM_REC_ON_OFF == alarm_msg->AlarmType)
    // {
    //     LOGI("IPCNET_ALARM_REC_ON_OFF");
    //     notify_recorder_task(0, LOCAL_MSG_RECORDER_ON_OFF, NULL, 0);
    // }
    // else if(IPCNET_ALARM_CHANGE_TIMEZONE == alarm_msg->AlarmType)
    // {
    //     LOGI("IPCNET_ALARM_CHANGE_TIMEZONE diff:%d", alarm_msg->Val);
    //     notify_recorder_task(0, LOCAL_MSG_REC_CHANGE_TIMEZONE, &alarm_msg->Val, sizeof(alarm_msg->Val));
    // }
    else
    {
		LOGE("unkown msg type!ignore it,alarm_msg->AlarmType=%d",alarm_msg->AlarmType);
    }
	
    return;
}
//lwj test
#if 0
int changetime(int itime)
{
    int h,m,s;
    h = itime/3600;
    m = (itime - h*3600)/60;
    s = itime%60;
    int out = h*10000+m*100+s;
    return out;
}
int alarmInfo()
{
    IPCNETAlarmRecordList_st testList;
    int alarmtime = 10;
    for (int i = 0; i < 100; i++)
    {
        testList.AlarmArr[i].Idx = i;
        testList.AlarmArr[i].AlarmRecordType = 1;
        testList.AlarmArr[i].StartTime = changetime(alarmtime);
        testList.AlarmArr[i].EndTime = changetime(alarmtime+10);
        alarmtime += 60;
    }
    testList.PageNum=1;
    testList.AlarmNum = 100;

    // save_alarm_record_list("/tmp/", &testList);
    return 0;
}

int chackAndSaveAlarmInfo()
{    
    struct tm *time_p;
    struct tm result;
    time_t tm;
    time(&tm);
    tm += 8*3600;
    if ((tm%(24*3600))>(24*3600-5) && (g_alarmRecordList.AlarmNum > 0))
    {
        LOGI("end day save alarm info");
        time_p = localtime_r(&tm, &result);
        char path[128];
        sprintf(path, "%s%d%02d/%02d", get_common_record_path(), result.tm_year+1900, result.tm_mon+1, result.tm_mday);
        LOGI("save path %s", path);
        // if (AlarmRecorderStatus == 1)
        // {
        //     int idx = g_alarmRecordList.AlarmNum;
        //     g_alarmRecordList.AlarmArr[idx].EndTime = 235959;
        //     g_alarmRecordList.AlarmNum++;
        // }
        
        // int ret = save_alarm_record_list(path, &g_alarmRecordList);
        // if(0 != ret) {
        //     LOGE("save_alarm_record_list failed\n");
        // }
        g_alarmRecordList.AlarmNum = 0;
        g_alarmRecordList.PageNum = 0;
    }

    // if ((tm%(24*3600))<5 && AlarmRecorderStatus == 1)
    // {
    //     g_alarmRecordList.AlarmArr[0].StartTime = 000000;
    // }
    return 0;
}
#endif

// added by lishun
int self_check_for_sd(char *recorderPath) {
    atomic_set(&g_recorderListEnable, 0);
    memset(&g_recorderList, 0, sizeof(g_recorderList));
    struct dirent *entry;
 	DIR *dirp = opendir(recorderPath);
    if(NULL == dirp){
        LOGE("opendir:%s failed\n", recorderPath);
        return -1;
    }

    int videosCapTot = 0;   // in megabyte;
    int videosCap;

    g_recorderList.days = 0;
    while(1) {
        if(g_recorderList.days > MAX_RECORD_DAYS) {
            LOGE("g_recorderList.days > MAX_RECORD_DAYS");
            atomic_set(&g_recorderListEnable, 0);
            closedir(dirp);
            return -1;
        }
        entry = readdir(dirp);
        if(NULL == entry) {
            break;
        }

        if(entry->d_type == DT_DIR)    ///file
        {
            if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) 
            {
                continue;
            }
            char dirPath[260];
            sprintf(dirPath, "%s/%s", recorderPath, entry->d_name);
            LOGI("dirPath:%s", dirPath);
            struct dirent *entry2;
            DIR *dirp2 = opendir(dirPath);
            if(NULL == dirp2){
                LOGE("opendir:%s failed\n", dirPath);
                continue;
            }
            while(1) {
                entry2 = readdir(dirp2);
                if(NULL == entry2) {
                    break;
                }
                if(entry2->d_type == DT_DIR)    ///file
                {
                    if(strcmp(entry2->d_name,".")==0 || strcmp(entry2->d_name,"..")==0) 
                    {
                        continue;
                    }

                    int date = atoi(entry->d_name)*100;
                    date += atoi(entry2->d_name);
                    g_recorderList.videoList[g_recorderList.days].date = date;
                    
                    char dirPath2[260];
                    // char jsFilePath[260];
                    sprintf(dirPath2, "%s/%s", dirPath, entry2->d_name);
                    LOGI("date[%d] dirPath:%s", g_recorderList.videoList[g_recorderList.days].date, dirPath2);
                    // sprintf(jsFilePath, "%s/videos.json", dirPath);
                    if(0 < listVideoFileInDir(dirPath2, &g_recorderList.videoList[g_recorderList.days], &videosCap))
                    {
                        videosCapTot += videosCap;
                        // save_video_list_to_json(&g_recorderList.videoList[i], jsFilePath);
                    }
                    LOGI("fileTotal[%d]", g_recorderList.videoList[g_recorderList.days].fileTotal);
                }
                g_recorderList.days++;
            }
            closedir(dirp2);
        }
        // g_recorderList.days++;
    }
	closedir(dirp);
    atomic_set(&g_recorderListEnable, 1);

    return videosCapTot;
}
// end by lishun

// add by lwj
VideoFileInfo_t *getVideolistForDate(int date, int *dateIndex)
{
    void *fileList = NULL;
    if (date == gVideoFileInfo.date)
    {
        *dateIndex = 0;
        // return &gVideoFileInfo;
        fileList = my_malloc(sizeof(VideoFileInfo_t));
        if (NULL == fileList)
        {
            LOGE("malloc fail");
            return NULL;
        }
        mutex_lock(&videoFileInfoLock);
        memcpy(fileList, &gVideoFileInfo, sizeof(VideoFileInfo_t));
        mutex_unlock(&videoFileInfoLock);
        return fileList;
    }
    else
    {
        for (int i = 0; i < g_recorderList.days; i++)
        {
            LOGI("date:%d", g_recorderList.videoList[i].date);
            if(g_recorderList.videoList[i].date == date)
            {
                *dateIndex = i+1;
                // return &g_recorderList.videoList[i];
                fileList = my_malloc(sizeof(VideoFileInfo_t));
                if (NULL == fileList)
                {
                    LOGE("malloc fail");
                    return NULL;
                }
                memcpy(fileList, &g_recorderList.videoList[i], sizeof(VideoFileInfo_t));
                return fileList;
            }
        }
    }
    return NULL;
}

VideoFileInfo_t *getVideolistForOtherDate(int date)
{
    void *fileList = NULL;
    if(atomic_read(&g_recorderListEnable) == 0)
    {
        sleep(1);
        if(atomic_read(&g_recorderListEnable) == 0)
        {
            LOGE("g_recorderListEnable fail");
            return NULL;
        }
    }
    for (int i = 0; i < g_recorderList.days; i++)
    {
        LOGI("date:%d", g_recorderList.videoList[i].date);
        if(g_recorderList.videoList[i].date == date)
        {
            // return &g_recorderList.videoList[i];
            fileList = my_malloc(sizeof(VideoFileInfo_t));
            if (NULL == fileList)
            {
                LOGE("malloc fail");
                return NULL;
            }
            memcpy(fileList, &g_recorderList.videoList[i], sizeof(VideoFileInfo_t));
            return fileList;
        }
    }
    LOGE("date[%d] fail", date);
    // g_recorderList.dateIndex = -1;    
    return NULL;
}

VideoFileInfo_t *getCurVideolist(int index)
{
    // VideoFileInfo_t *pFileInfo = NULL;
    void *fileList = NULL;
    if (index < 0)
    {
        return NULL;
    }
    else
    {
        fileList = my_malloc(sizeof(VideoFileInfo_t));
        if (NULL == fileList)
        {
            LOGE("malloc fail");
            return NULL;
        }
        if (0 == index)
        {
            mutex_lock(&videoFileInfoLock);
            memcpy(fileList, &gVideoFileInfo, sizeof(VideoFileInfo_t));
            mutex_unlock(&videoFileInfoLock);
        }
        else
        {
            memcpy(fileList, &g_recorderList.videoList[index - 1], sizeof(VideoFileInfo_t));
            // return &g_recorderList.videoList[index - 1];
        }
    }
    return fileList;
}


extern SEPCAM_RECORDER_STATUS_s    g_recorder_status;

static int recorder_app()
{
    int ret;
	DEV_RECORD_CFG_t rec_cfg;

    set_debug_level(DEBUG_MSG);
    // set_debug_level(VERBOSE_MSG);
    
    LOGI("sepcam_ipc_init");
    ret = sepcam_ipc_init(g_recorder_serv_name);
    if(ret != 0)
    {
        LOGE("sepcam_ipc_init FAIL");
        return -1;
    }

    IPCNetCamInfo_st cam_info;
    memset(&cam_info, 0, sizeof(cam_info));
    ipcam_get_device_config(IPCNET_GET_CAM_INFO_REQ, &cam_info);
    if(ret != 0)
    {
        LOGE("IPCNET_GET_SYS_INFO FAIL");
        return -1;
    }

	/**进程刚起来的时候，先release掉共享内存，避免程序异常时没释放**/
	release_av_enc(ALL_CHANNEL);

    LOGI("IPCNET_GET_SYS_INFO SysVer:%s ConfigPath:%s", cam_info.SysVer, cam_info.ConfigPath);
    init_record_info(cam_info.ConfigPath);

#if RECORDER_LOG
    if (g_recorder_log_flag)
    {
        Logfi("recorder --------- start ----------");
        update_logs_mgr();
    }
#endif

    /* 等avapp加载模型成功 */
    while (1)   
    {
        int ret = ipcam_get_device_config(IPCNET_AVAPP_MODEL_REQ, NULL);
        if (ret == IPCNET_RET_OK)
        {
            LOGI("IPCNET_AVAPP_MODEL_REQ success");
            Logfi("AVAPP_MODEL_REQ success");
            break;
        }
        LOGI("IPCNET_AVAPP_MODEL_REQ wait ret=%d", ret);
        sleep(1);
    }
    
#ifdef RECORDER_DEBUG
    set_record_type(0, &g_recorder_status.recType);

    for ( int i = 0; i < (int)(sizeof(rec_cfg.record_time_policy_list)/sizeof(DEV_DEFEND_TIME_LIST_INFO_t)); i++)
    {
        for ( int j = 0; j < (int)(sizeof(g_recorder_status.recType.recTimeList[i].timeInfo)/sizeof(TIME_INFO)); j++)
        {
            LOGI("type:%d, list:%d, lisEn:%d, dayEn:%d, %02d:%02d:%02d - %02d:%02d:%02d", g_recorder_status.recType.type, i, 
                                                                        g_recorder_status.recType.recTimeList[i].en,
                                                                        g_recorder_status.recType.recTimeList[i].timeInfo[j].en,
                                                                        g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_s.Hour,
                                                                        g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_s.Min,
                                                                        g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_s.Sec,
                                                                        g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_e.Hour,
                                                                        g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_e.Min,
                                                                        g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_e.Sec);
        }
    }
#endif

    init_record_path(cam_info.DiskRoot, cam_info.CommRecRoot, cam_info.AlarmRecRoot);
    
    LOGI("ipcam_regist_alarm_event_callback");
    ipcam_regist_alarm_event_callback(ipc_alarm_callback);

#if 0
#define TEST_SIZE (100*1024)
    frame_buff_t *pframe;
    unsigned char buf[TEST_SIZE];
    while(1)
    {
        LOGI("frame test");
        pframe = alloc_frame(TEST_SIZE);
        frame_copy_data(pframe, buf, TEST_SIZE);

        free_frame(frame_clone(pframe));
        free_frame(pframe);
    }
#endif

    //录像线程
    LOGI("regitster_recorder_task");
    ret = regitster_recorder_task();
    if(ret != 0)
    {
        Logfi("regitster_recorder_task FAIL");
        return -1;
    }

// added by lishun
    // self check for flash;
#if 1
    IPCNetDiskInfo_st diskInfo;
    ipcam_get_device_config(IPCNET_GET_DISK_CFG_REQ, &diskInfo);
    LOGI("diskInfo.Total=[%u]", diskInfo.Total);
    if(!diskInfo.isValid)
    {
        LOGI("not find sdcard");
        gSDCardStatus = -1;
        if (access("/dev/mmcblk0p1", F_OK) == 0 || access("/dev/mmcblk0", F_OK) == 0)
        {
            // 有设备但挂载不上
            gSDCardStatus = -2;
        }
    }
    else
    {
        if (access("/mnt/s0", F_OK) != 0)
        {
            LOGI("path:/mnt/s0 error");
            system("mkdir -p /mnt/s0");
            // gSDCardStatus = -2;
        }
        int videoCapTot = self_check_for_sd(RECORDER_PATH);
        int capDiff = abs((diskInfo.Total-diskInfo.Free) - videoCapTot);
        LOGI("------------videoCapTot[%d]-diskInfo.Total[%u]-diskInfo.Free[%u]---------capDiff:%d",videoCapTot, diskInfo.Total,diskInfo.Free,capDiff);
        if(capDiff > 2000) {    // 录像视频总容量，比sd卡已使用容量差2G，则认为需要格式化sd卡。
            LOGI("Need to format the SD card");
            // g_sd_abnormal = 1;
            gSDCardStatus = -2;
        }
    }
    Logfi("gSDCardStatus %d", gSDCardStatus);
#endif
// end by lishun
    
    //recorder server
    LOGI("init_recorder_server g_recorder_serv_name=%s",g_recorder_serv_name);
    init_recorder_server(g_recorder_serv_name);
    
    time_t syncFileLastTime = -1;
    while(recorder_process_run)
    {	
    	LOGV("child check upgrade file if exist");
		if((0 == access(LOCAL_UPDATE_FLAG_FILE,F_OK)) 
			|| (0 == access(RECORDER_EXITS_FLAG_FILE,F_OK))
			|| (0 == access(REMOVE_UPDATE_FLAG_FILE,F_OK)))
		{
			LOGW("###child program exit####");
            // sleep 一段时间等idx文件保存完成
            sleep(3);
			exit(0);
		}
        //add by lwj 
        syncTimeZoneFromIpc();
        #if 0
        if((time(NULL) - syncFileLastTime) > 10)
        {
            mutex_lock(&videoFileInfoLock);
            syncFileListToArray(&gVideoFileInfo, 0, 0);
            mutex_unlock(&videoFileInfoLock);
            syncFileLastTime = time(NULL);
        }
        
        // chackAndSaveAlarmInfo();
        // alarmInfo(); // test
        // IPCNETAlarmRecordList_st alarmRecordList;
        // for (int i = 0; i < 8; i++)
        // {
        //     int ret = load_alarm_record_list(20220920, i+1, &alarmRecordList);
        //     if(ret == -2)
        //     {
        //         break;
        //     }
        // }
        #endif
        //add end by lwj 
        sleep(1);
    }

    LOGE("recorder_process_run=%d,recorder_app exit!!",recorder_process_run);
    return 0;
}

#if 0	//国科用来追段错误
#define excute_name "/tmp/nfs/seprecorder"
#define gdb_lib_path "/tmp/nfs/gk7102/usr/lib"
#define gdb_path "/tmp/nfs/gk7102/gdb"
void debug_dump(int signo)
{
        char buf[1024];
        char cmd[1024];
        char name[64] = {0};
        FILE *fh;

		LOGI("catch signo:%s",(SIGSEGV==signo)?"SIGSEGV":"unkown signo");
		strcpy(name,"LD_LIBRARY_PATH");
		
		//setenv(name,"/rom/gdb/usr/lib",1);
		setenv(name,gdb_lib_path,1);
		
		usleep(1000);

		#if 0
        snprintf(buf, sizeof(buf), "/proc/%d/cmdline", getpid());
        LOGI("###buf:%s####",buf);
        if(!(fh = fopen(buf, "r")))
        {
        	LOGI("fopen %s FAIL",buf);
			exit(0);
        }
        if(!fgets(buf, sizeof(buf), fh))
        {
        	LOGI("fgets FAIL!");
			exit(0);
        }
        fclose(fh);
        if(buf[strlen(buf) - 1] == '/n')
                buf[strlen(buf) - 1] = '/0';
        #endif        
        //snprintf(cmd, sizeof(cmd), "/rom/gdb/gdb %s %d -ex=bt > /rom/gdb/debug.txt", buf, getpid());
        snprintf(cmd, sizeof(cmd), "%s %s %d -q --batch --ex=\"set pagination 0\" --ex=\"thread apply all bt\" > /rom/gdb/debug.txt",
        gdb_path, excute_name, getpid());
        LOGI("##%s##",cmd);
        system(cmd);
        exit(0);
}

void signal_handler(int signo)
{
	LOGI("signo=%d",signo);
	if(SIGSEGV == signo)
	{
		LOGI("debug_dump");
		debug_dump(signo);
	}
}

int init_signal()
{
    install_signal_hanlder(SIGSEGV,signal_handler);
    return 0;
}
#endif

int main(int argc, char *argv[])
{
    bool needFork = FALSE; //FALSE;//TRUE;
    int status = 0;
    int pid = 0;
    pid_t processID = 0;

    LOGI("init_signal");
	//init_signal();

#if 0
    int opt = -1;
    while( (opt = getopt( argc, argv, "fu:" )) != -1 )
    {
        LOGI("opt:%c\n",opt);
        switch( opt )
        {
        case 'f':
            needFork = FALSE;
        break;

        case 'u':
            strcpy(gUID, test_uid[atoi(optarg)]);
        break;
        }
    }
#endif

#if 0
	struct stat statbuf;
    int ret = stat(argv[1], &statbuf);
    printf("argv:%s ret:%d\n", argv[1], ret);
    return 0;
#endif

// test lwj
#if 0
    // IPCNetDiskInfo_st diskInfo;
    // ipcam_get_device_config(IPCNET_GET_DISK_CFG_REQ, &diskInfo);
    int videoCapTot = self_check_for_sd(RECORDER_PATH);
    while (1);
    {
        sizeof(g_recorderList);
        sleep(1);
    }
#endif

    if(needFork)
    {
        do // fork at least once but stop on the status conditions returned by wait or if autoStart pref is false
        {
        	LOGI("father recorder_process_run=%d",recorder_process_run);
			if(0 == recorder_process_run)
			{
				LOGI("reboot system,recorder exits");
				exit(0);
			}
        	
        	//判断是否在本地升级，升级时，这个进程要退出
        	LOGD("father check upgrade file if exist");
			if((0 == access(LOCAL_UPDATE_FLAG_FILE,F_OK)) 
				|| (0 == access(RECORDER_EXITS_FLAG_FILE,F_OK))
				|| (0 == access(REMOVE_UPDATE_FLAG_FILE,F_OK)))
			{
				LOGI("###father program exit###");
				exit(0);
			}
			
            processID = fork();
            if (processID > 0) // this is the parent and we have a child
            {
                sChildPID = processID;
                status = 0;
                while (status == 0) //loop on wait until status is != 0;
                {	
                 	pid = wait(&status);
                 	int exitStatus = (int) WEXITSTATUS(status);
                	printf("Child Process %d wait exited with pid=%d status=%d exit status=%d\n", processID, pid, status, exitStatus);
                	
    				if (WIFEXITED(status) && pid > 0 && status != 0) // child exited with status -2 restart or -1 don't restart 
    				{
    					//printf("child exited with status=%d\n", exitStatus);
    					
    					if ( exitStatus == -1) // child couldn't run don't try again
    					{
    						printf("child exited with -1 fatal error so parent is exiting too.\n");
    						exit (EXIT_FAILURE); 
    					}
    					break; // restart the child
    						
    				}
    				
    				if (WIFSIGNALED(status)) // child exited on an unhandled signal (maybe a bus error or seg fault)
    				{	
    					printf("child was signalled\n");
    					break; // restart the child
    				}

                 		
                	if (pid == -1 && status == 0) // parent woken up by a handled signal
                   	{
    				    printf("handled signal continue waiting\n");
                   		continue;
                   	}
                   	
                 	if (pid > 0 && status == 0)
                 	{
                 		//printf("child exited cleanly so parent is exiting\n");
                 		//exit(EXIT_SUCCESS);
                 		break;
                	}
                	
                	printf("child died for unknown reasons parent is exiting\n");
                	exit (EXIT_FAILURE);
                }
            }
            else if (processID == 0) // must be the child
    			break;
            else
            	exit(EXIT_FAILURE);
            	
            	
            //eek. If you auto-restart too fast, you might start the new one before the OS has
            //cleaned up from the old one, resulting in startup errors when you create the new
            //one. Waiting for a second seems to work
            sleep(1);
        }while (true);
    }

	LOGI("recorder_app");
    recorder_app();

    return 0;
}

