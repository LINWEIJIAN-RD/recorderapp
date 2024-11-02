#include "recorder_utils.h"
#include "fcntl.h"
#include "logServer.h"

#include "event_handle.h"
#include "stream_frame_info.h"
#include "sepcam_recorder.h"
#include "disk_mgr.h"
#include "sepcam_ipc_api.h"
#include "recorder_params.h"
#include "recorder_es2pes.h"
#include "streamshmmgr.h"
#include "sepcam_ipc_internal.h"

#include "mempool.h"
#include "dirent.h"


#define CHINA_TIME_ZONE 8
// #define g_timeZone 8

#define UNIT_BYTES 6

#define RESERVED_SIZE_MB 300

bool                 g_disk_valid = FALSE;
// extern DEV_RECORD_CFG_t     g_record_cfg[MAX_VI_CHANNEL_NUM];
uint64_t             g_clock_time;
uint32_t             g_recorder_eventid;
extern int g_recorder_log_flag;
int g_timeZone = -101;

SEPCAM_RECORDER_STATUS_s    g_recorder_status = {
    .alarmStat.alarmed = 0
};

typedef struct
{
    const int vench;
    int get_index;
    uint16_t frame_index;
}CHECK_FRAME_INDEX_t;

static CHECK_FRAME_INDEX_t g_check_index[2] = {
    {0, 0, 0},
    {1, 0, 0},
};

void syncTimeZoneFromIpc()
{
    // LOGI("g_timeZone[%d]", g_timeZone);
    if (g_timeZone > 12 || g_timeZone < -12)
    {
        IPCNetTimeCfg_st StimeInfo;
        if (IPCNET_RET_OK == ipcam_get_device_config(IPCNET_GET_TIME_FORM_IPC_REQ, &StimeInfo)){
            g_timeZone = StimeInfo.TmOff;
        }
    }
}

static unsigned char get_alarm_status() {
    return g_recorder_status.alarmStat.alarmed;
}

static unsigned int get_time_stamp_in_secs(MyTime *time_s) {
    return time_s->Hour*3600 + time_s->Min*60 + time_s->Sec;
}

static int __p_local_time() {
    time_t t = time(NULL);
    struct tm *local = localtime(&t);
    LOGI("当前时间: %d-%d-%d %d:%d:%d\n", 
        local->tm_year + 1900, 
        local->tm_mon + 1, 
        local->tm_mday, 
        local->tm_hour, 
        local->tm_min, 
        local->tm_sec);
    return 0;
}

// added by lwj
void rec_flush_stream_shm()
{
    flush_stream_frame_info(SEPCAM_RECORDER_IPC_NAME, STEAMSHM_VENC0_NAME);
    flush_stream_frame_info(SEPCAM_RECORDER_IPC_NAME, STEAMSHM_VENC1_NAME);
    flush_stream_frame_info(SEPCAM_RECORDER_IPC_NAME, STEAMSHM_AENC_NAME);
}

static int check_record_time(IPCNetTimeInfo_st *curTime)
{
    int i, j;
    int curTimeStamp;
    int startTimeStamp;
    int endTimeStamp;
    int ret = -1;
    for(i = 0; i < (int)(sizeof(g_recorder_status.recType.recTimeList)/sizeof(RECORDER_TIME_LIST)); i++)
    {
    #ifdef RECORDER_DEBUG
        LOGI("list_en:%d, week_day:%d en:%d, recTime_s:%u, recTime_cur:%u, recTime_e:%u",g_recorder_status.recType.recTimeList[i].en, curTime->Date.WDay,
                            g_recorder_status.recType.recTimeList[i].timeInfo[curTime->Date.WDay].en,
                            get_time_stamp_in_secs(&g_recorder_status.recType.recTimeList[i].timeInfo[curTime->Date.WDay].recTime_s),
                            get_time_stamp_in_secs(&curTime->Time),
                            get_time_stamp_in_secs(&g_recorder_status.recType.recTimeList[i].timeInfo[curTime->Date.WDay].recTime_e));
    #endif
        if (0 != g_recorder_status.recType.recTimeList[i].en)
        {
            if (0 != g_recorder_status.recType.recTimeList[i].timeInfo[curTime->Date.WDay].en)
            {
                curTimeStamp = get_time_stamp_in_secs(&curTime->Time);
                startTimeStamp = get_time_stamp_in_secs(&g_recorder_status.recType.recTimeList[i].timeInfo[curTime->Date.WDay].recTime_s);
                endTimeStamp = get_time_stamp_in_secs(&g_recorder_status.recType.recTimeList[i].timeInfo[curTime->Date.WDay].recTime_e);

                if (startTimeStamp < endTimeStamp)
                {
                    if(curTimeStamp >= startTimeStamp && curTimeStamp <= endTimeStamp)
                    {
                        ret = 0;
                        break;
                    }
                    else 
                    {
                        // 不在录像时间段内
                    }
                }
                else if (startTimeStamp > endTimeStamp) //录像时间段延续到第二天
                {
                    MyTime recTime;
                    recTime.Hour = 23;
                    recTime.Min = 59;
                    recTime.Sec = 59;
                    if(curTimeStamp >= startTimeStamp && curTimeStamp <= get_time_stamp_in_secs(&recTime))
                    {
                        ret = 0;
                        break;
                    }
                    else 
                    {
                        // 不在录像时间段内
                    }
                }
                else    //if (startTimeStamp == endTimeStamp)                
                {
                    // 不在录像时间段内
                }
            }
            else
            {
                // 判断是不是有从昨天延续到今天的录像时间段
                int yday = (curTime->Date.WDay == 0)?6:(curTime->Date.WDay-1);
                if (0 != g_recorder_status.recType.recTimeList[i].timeInfo[yday].en)
                {
                    curTimeStamp = get_time_stamp_in_secs(&curTime->Time);
                    startTimeStamp = get_time_stamp_in_secs(&g_recorder_status.recType.recTimeList[i].timeInfo[yday].recTime_s);
                    endTimeStamp = get_time_stamp_in_secs(&g_recorder_status.recType.recTimeList[i].timeInfo[yday].recTime_e);
                    if (startTimeStamp > endTimeStamp)
                    {
                        MyTime recTime;
                        recTime.Hour = 00;
                        recTime.Min = 00;
                        recTime.Sec = 00;
                        if(curTimeStamp >= get_time_stamp_in_secs(&recTime) && curTimeStamp <= endTimeStamp)
                        {
                            ret = 0;
                            break;
                        }
                        else 
                        {
                            // 不在录像时间段内
                        }
                    }
                    else
                    {
                        // 不在录像时间段内
                    }                    
                }
                else
                {
                    // 不在录像时间段内
                }
            }
        }
        else
        {
            // 不在录像时间段内
        }
    }
    return ret;
}
// end by lwj

static int time_to_idx_in_a_file_idx(RECORD_REC_FILE_INDEX *pFileIdx) {     // pTime_s: start time of rec file; pTime_c: cur time; ret: [1, 60]
    int curIdx, timeStamp_s, timeStamp_c;
    timeStamp_c = get_time_stamp_in_secs(&pFileIdx->unitTime_s);
    timeStamp_s = get_time_stamp_in_secs(&pFileIdx->fileTime_s);

    int diff = timeStamp_c - timeStamp_s;
    if( diff > 600 || diff < 0) {
        LOGE("timeStamp_c - timeStamp_s >= 600");
        return -1;
    }
    LOGI("timeStamp_c[%d] timeStamp_s[%d]", timeStamp_c, timeStamp_s);
    curIdx = floor(diff / 10.0f);
    // added by lwj
    curIdx = (curIdx < 1)?1:curIdx;
    // end by lwj
    return curIdx - 1;
}

char* get_record_name_by_time(MyTime *pTime) {       // name without .idx or .pes
    static char name[MAX_NAME];
    int min_s;
    if(NULL != pTime) {
        min_s = pTime->Min - pTime->Min % 10;;
        sprintf(name, "%02d-%02d-00", pTime->Hour, min_s);
    }
    else {
        IPCNetTimeInfo_st timeInfo;
        get_local_time(g_timeZone, &timeInfo);

        min_s = timeInfo.Time.Min - timeInfo.Time.Min % 10;;
        sprintf(name, "%02d-%02d-00", timeInfo.Time.Hour, min_s);
    }
    return name;
}

int notify_recorder_task(uint32_t local_module,uint32_t cmd_type, void *msg, int32_t msg_len)
{
    MSG_NODE_t local_msg;
    local_msg.src_mid = local_module;
	local_msg.dst_mid = RECORD_TASK_ID;
	local_msg.type = cmd_type;
	local_msg.msg_len = msg_len;
	local_msg.msg = msg;
    return send_msg(&local_msg);
}

static void parse_str_time(MyTime* mt, char* str)
{
	char digstr[32];
	int i,j;
	memset(digstr,0,32);
	for(i=0,j=0;i<32&&str[i];i++){
		if(str[i]==':'){
			strncpy(digstr,str,i);
			mt->Hour = (unsigned char)atoi(digstr);
			j=i+1;
			break;
		}
	}
	memset(digstr,0,32);
	for(i=j;i<32&&str[i];i++){
		if(str[i]==':'){
			strncpy(digstr,&str[j],i-j);
			mt->Min = (unsigned char)atoi(digstr);
			j=i+1;
			break;
		}
	}
	memset(digstr,0,32);
	//for(i=j;i<32&&str[i];i++){
	//	if(str[i]==':'){
			//strncpy(digstr,&str[j],i-j);
			strncpy(digstr,&str[j],2);
			mt->Sec = (unsigned char)atoi(digstr);
			//printf("s:%s j[%d]\n",digstr,j);
	//		break;
	//	}
	//}

}

static void default_file_start_time_by_local_time(MyTime *pStartTime) {
    if(NULL == pStartTime) {
        return;
    }

    IPCNetTimeInfo_st timeInfo;
    get_local_time(g_timeZone, &timeInfo);

    pStartTime->Hour = timeInfo.Time.Hour;
    pStartTime->Min = timeInfo.Time.Min - timeInfo.Time.Min % 10;
    pStartTime->Sec = 0;
}

static int get_rec_unit_idx_by_time(MyTime *pUnitTime) {
    MyTime            startTime;
    default_file_start_time_by_local_time(&startTime);
    if(NULL == pUnitTime) {
        IPCNetTimeInfo_st timeInfo;

        get_local_time(g_timeZone, &timeInfo);
        

        int mins = timeInfo.Time.Min - startTime.Min;
        int secs = timeInfo.Time.Sec;

        int idx = floor( (mins*60 + secs) / 10.0 );
        return idx;
    }
    else {
        int mins = pUnitTime->Min - startTime.Min;
        int secs = pUnitTime->Sec;

        int idx = floor( (mins*60 + secs) / 10.0 );
        return idx;
    }

}

static int get_day_night_curIdx_by_local_time(IPCNetTimeInfo_st *pTimeInfo) {
    int i = pTimeInfo->Time.Hour;
    int j = floor(pTimeInfo->Time.Min / 10.0);      // 10 mins
    int curIdx = i*6 + j;
    return curIdx;
}

// static int get_rec_file_curIdx_by_local_time(IPCNetTimeInfo_st *pTimeInfo) {
//     int i = pTimeInfo->Time.Min % 10;
//     int offset_by_secs = i*60 + pTimeInfo->Time.Sec;
//     int curIdx = floor(offset_by_secs / 10.0);            // 10 secs
//     return curIdx;
// }

// static void reset_day_night_idx_by_local_time(RECORD_DAY_NIGHT_INDEX *pDayNightIdx) {
//     IPCNetTimeInfo_st timeInfo;
//     get_local_time(g_timeZone, &timeInfo);
    
//     pDayNightIdx->year = timeInfo.Date.Year - 2000;
//     pDayNightIdx->mon = timeInfo.Date.Mon;
//     pDayNightIdx->day = timeInfo.Date.Day;
//     // pDayNightIdx->curIdx = get_day_night_curIdx_by_local_time(&timeInfo);
// }

static void reset_rec_file_idx_by_time(MyTime *pTime) {
    if(NULL != pTime) {
        g_recorder_status.aRecFileIdx.fileTime_s.Hour = pTime->Hour;
        g_recorder_status.aRecFileIdx.fileTime_s.Min = pTime->Min - pTime->Min % 10;
        g_recorder_status.aRecFileIdx.fileTime_s.Sec = 0;

        // g_recorder_status.aRecFileIdx.unitTime_s = g_recorder_status.aRecFileIdx.fileTime_s;
        g_recorder_status.aRecFileIdx.unitTime_s.Hour = pTime->Hour;
        g_recorder_status.aRecFileIdx.unitTime_s.Min = pTime->Min;
        g_recorder_status.aRecFileIdx.unitTime_s.Sec = pTime->Sec - pTime->Sec % 10;
    }
    else {      // reset by local time;
        IPCNetTimeInfo_st timeInfo;
        get_local_time(g_timeZone, &timeInfo);

        g_recorder_status.aRecFileIdx.fileTime_s.Hour = timeInfo.Time.Hour;
        g_recorder_status.aRecFileIdx.fileTime_s.Min = timeInfo.Time.Min - timeInfo.Time.Min % 10;
        g_recorder_status.aRecFileIdx.fileTime_s.Sec = 0;

        // g_recorder_status.aRecFileIdx.unitTime_s = g_recorder_status.aRecFileIdx.fileTime_s;
        
        g_recorder_status.aRecFileIdx.unitTime_s.Hour = timeInfo.Time.Hour;
        g_recorder_status.aRecFileIdx.unitTime_s.Min = timeInfo.Time.Min;
        g_recorder_status.aRecFileIdx.unitTime_s.Sec = timeInfo.Time.Sec - timeInfo.Time.Sec % 10;
    }

    g_recorder_status.aRecFileIdx.curFileLen = 0;
    // g_recorder_status.aRecFileIdx.curIdx = 0;
    
    memset(g_recorder_status.aRecFileIdx.recMinUnits, 0, MAX_UNITS*sizeof(RECORDER_UNIT));

// set recMinUnits[0]: {
    // g_recorder_status.aRecFileIdx.recMinUnits[0].alarmed = get_alarm_status();
    g_recorder_status.aRecFileIdx.recMinUnits[0].offset = 0;
    #if 0   //added by lwj
    g_recorder_status.aRecFileIdx.recMinUnits[0].type = g_recorder_status.recType.type; 
    #endif
// }
}

static int mkdirs(const char *sPathName,int mode)  
{  
	char DirName[256];
	int i,len;
	
	strcpy(DirName, sPathName);
	
	len = strlen(DirName);  
	if(DirName[len-1]!='/')  
		strcat(DirName,"/");  

	len = strlen(DirName);  

	for(i=1; i<len; i++)  
	{  
		if(DirName[i]=='/')  
		{  
			DirName[i] = 0;  
			if(access(DirName,F_OK) < 0)  
			{  
				if(mkdir(DirName,mode) < 0)  
				{   
					perror("mkdir error");   
					return   -1;
				}  
			}  
			DirName[i] = '/';  
		}
	}  

	return   0;  
}

static int write_frame_to_file(STREAM_FRAME_INFO_t frameinfo, char *enc_buf)
{
    int is_video = PLAYLOAD_IS_VIDEO(frameinfo.payload);
    int ret;
    
    if(is_video)
    {
        if(g_recorder_status.vstart_timestamp == 0)
        {
            g_recorder_status.vstart_timestamp = frameinfo.timestamp;
        }
        
        LOGV("video frame_type[%c]:%d payload:%d frame No:%d frame size:%d, time:%lld(ms)",
            frameinfo.frame_type > 4 ? 'I' : 'P', frameinfo.frame_type, frameinfo.payload,
            frameinfo.frame_no, frameinfo.frame_size, frameinfo.timestamp);
    }
    else
    {
        // if(g_recorder_status.astart_timestamp == 0)
        // {
        //     g_recorder_status.astart_timestamp = frameinfo.timestamp;
        // }
        
        // frameinfo.timestamp -= g_recorder_status.astart_timestamp;
    }

    frameinfo.timestamp = (frameinfo.timestamp+(g_timeZone*3600*1000))%(24*3600*1000);
    LOGV("save time:%lld(ms)", frameinfo.timestamp);

    ret = wirte_es2pes_packet(g_recorder_status.rec_fd, &frameinfo,
        g_recorder_status.enc_buf, frameinfo.frame_size, g_recorder_status.wr_buf);
    // added by lwj
    // computer current size of the written file after save vframe;
    g_recorder_status.aRecFileIdx.curFileLen += (PES_HEAD_LEN + frameinfo.frame_size);
    // LOGI("frame_size[%d] --> offset[%d]", frameinfo.frame_size, g_recorder_status.aRecFileIdx.curFileLen);
    // end by lwj
    return ret;
}

time_t get_local_time_in_secs(int timeZone) {
    time_t tm;
    
    time(&tm);
    tm += timeZone*3600;

    return tm;
}

void get_local_time(int timeZone, IPCNetTimeInfo_st *pTimeInfo) {
    struct tm result;
    time_t tm;
    
    time(&tm);
    tm += timeZone*3600;
    
    localtime_r(&tm, &result);
    pTimeInfo->Date.Year = result.tm_year + 1900;
    pTimeInfo->Date.Mon = result.tm_mon + 1;
    pTimeInfo->Date.Day = result.tm_mday;
    pTimeInfo->Date.WDay = result.tm_wday;
    pTimeInfo->Time.Hour = result.tm_hour;
    pTimeInfo->Time.Min = result.tm_min;
    pTimeInfo->Time.Sec = result.tm_sec;
    // LOGI("get_local_time: %hd-%d-%d w:%d %d:%d:%d",
    //     pTimeInfo->Date.Year, pTimeInfo->Date.Mon, pTimeInfo->Date.Day, pTimeInfo->Date.WDay,
    //     pTimeInfo->Time.Hour, pTimeInfo->Time.Min, pTimeInfo->Time.Sec);
}

int save_rec_units(const char *idxPath) {
    // unsigned char *buf;
    // buf = (unsigned char*)malloc(MAX_UNITS*UNIT_BYTES);
    unsigned char buf[MAX_UNITS*UNIT_BYTES] = {0};
    for(int i=0; i<MAX_UNITS; i++) {
        buf[i*UNIT_BYTES] = g_recorder_status.aRecFileIdx.recMinUnits[i].type;
        buf[i*UNIT_BYTES+1] = g_recorder_status.aRecFileIdx.recMinUnits[i].alarmed;
        buf[i*UNIT_BYTES+2] = (unsigned char)(g_recorder_status.aRecFileIdx.recMinUnits[i].offset) & 0xFF;
        buf[i*UNIT_BYTES+3] = (unsigned char)(g_recorder_status.aRecFileIdx.recMinUnits[i].offset >> 8) & 0xFF;
        buf[i*UNIT_BYTES+4] = (unsigned char)(g_recorder_status.aRecFileIdx.recMinUnits[i].offset >> 16) & 0xFF;
        buf[i*UNIT_BYTES+5] = (unsigned char)(g_recorder_status.aRecFileIdx.recMinUnits[i].offset >> 24) & 0xFF;
        LOGI("[lwj test]offset i=%02d %02d:%02d [%d][%d][%d]", i, i/6, (i%6)*10, 
            g_recorder_status.aRecFileIdx.recMinUnits[i].type, 
            g_recorder_status.aRecFileIdx.recMinUnits[i].alarmed,
            g_recorder_status.aRecFileIdx.recMinUnits[i].offset);
        // LOGI("buf [%d] [%d] [%d] [%d]",buf[i*UNIT_BYTES+2],buf[i*UNIT_BYTES+3],buf[i*UNIT_BYTES+4],buf[i*UNIT_BYTES+5]);
    }

    int fd = open(idxPath, O_CREAT|O_RDWR|O_TRUNC|O_SYNC, 0666);
    if(fd < 0) {
        LOGE("create idx file failed");
        return -1;
    }

    int ret = write(fd, buf, MAX_UNITS*UNIT_BYTES);
    if(ret <= 0) {
        LOGE("save idx file failed");
        return -1;
    }

    ret = close(fd);
    if(ret != 0) {
        LOGE("save idx file failed");
        return -1;
    }
    
    sync();
    return 0;
}

int load_rec_units(const char *idxPath, RECORDER_UNIT  *recUnits) {
    // unsigned char *buf;
    // buf = (unsigned char*)malloc(MAX_UNITS*UNIT_BYTES);
    unsigned char buf[MAX_UNITS*UNIT_BYTES] = {0};

    int fd = open(idxPath, O_RDONLY);
    if(fd < 0) {
        return -1;
    }

    int ret = read(fd, buf, MAX_UNITS*UNIT_BYTES);
    if(ret <= 0) {
        LOGE("load idx file failed");
        return -1;
    }

    close(fd);

    for(int i=0; i<MAX_UNITS; i++) {
        recUnits[i].type = buf[i*UNIT_BYTES];
        recUnits[i].alarmed = buf[i*UNIT_BYTES+1];
        recUnits[i].offset = buf[i*UNIT_BYTES+2] +
                               (buf[i*UNIT_BYTES+3]<<8) +
                               (buf[i*UNIT_BYTES+4]<<16) +
                               (buf[i*UNIT_BYTES+5]<<24);
    }

    return 0;
}

void upload_disk_valid()
{
	IPCNetDiskInfo_st diskInfo;
	memset(&diskInfo, 0, sizeof(IPCNetDiskInfo_st));
	if(IPCNET_RET_OK != ipcam_get_device_config(IPCNET_GET_DISK_CFG_REQ, &diskInfo))
	{
        Logfi("IPCNET_GET_DISK_CFG_REQ FAIL");
	    return ;
	}
    LOGI("dev[%s]", diskInfo.Dev);
	g_disk_valid = diskInfo.isValid;
}

int __save_record_frame(int vench, struct THR_TASK * ptask, EVENT_HANDLE_LIST_t * handler_list)
{
    STREAM_FRAME_INFO_t frameinfo;
    uint64_t now_timestamp;
    static int getVFrameIndex = 0, getAFrameIndex = 0;
    int noFrame = 1;
    int ret;
    
#if RECORDER_LOG
    static time_t vench_t[2] = {0};
    static time_t vench_last_t[2] = {0};
    int time_out_sec = 30;
    static int state[2] = {0};  //0:正常 -1:异常
#endif

    if(g_recorder_status.rec_fd < 0)
    {
        LOGI("!!!!!ERR fd is invalid");
        return FAIL;
    }
    
    ret = ipcam_get_avenc_stream(vench, &frameinfo, g_recorder_status.enc_buf, g_recorder_status.buf_len, &getVFrameIndex);
    // LOGI("ret[%d] frame_size[%d]", ret, frameinfo.frame_size);
    // ret = ipcam_get_avenc_stream(2, &frameinfo, g_recorder_status.enc_buf, g_recorder_status.buf_len, &getVFrameIndex);
    // LOGI("ret[%d] frame_size[%d]", ret, frameinfo.frame_size);
    if(ret == SHM_RDWR_BUF_SIZE_ERR)
    {
        my_free(g_recorder_status.enc_buf);
        g_recorder_status.buf_len = 0;

        g_recorder_status.enc_buf = my_malloc(frameinfo.frame_size + 1024);
        g_recorder_status.buf_len = frameinfo.frame_size + 1024;
        LOGI("ret[%d]", ret);
        ret = ipcam_get_avenc_stream(vench, &frameinfo, g_recorder_status.enc_buf, g_recorder_status.buf_len, &getVFrameIndex);
    }

    if(ret > 0)
    {
#if RECORDER_LOG
        if (g_recorder_log_flag)
        {
            if(time(NULL)-vench_t[vench] > time_out_sec)
            {
                Logfi("recorder start ch[%d]", vench);
                update_logs_mgr();
            }
            vench_t[vench] = time(NULL);

            if(state[vench] == 1)
            {
                state[vench] = 0;
                Logfi("normal ch[%d]", vench);
                update_logs_mgr();
                vench_last_t[vench] = time(NULL);
            }
        }
        time_t timep;        
        time (&timep); 

        LOGI("now time[%s] ipcam_get_avenc_stream streamch[lwj]:%d vench:%d payload:%d frame_type:%d frame_no:%d timestamp:%lld frame_size:%d",
            asctime( gmtime(&timep) ),vench, frameinfo.vench, frameinfo.payload, frameinfo.frame_type,
            frameinfo.frame_no, frameinfo.timestamp, frameinfo.frame_size);
#endif

        noFrame = 0;

        if(PLAYLOAD_IS_VIDEO(frameinfo.payload))
        {
            if(g_recorder_status.vstart_timestamp == 0)
            {
                g_check_index[frameinfo.vench].frame_index = frameinfo.frame_no;
            }
            else
            {
                g_check_index[frameinfo.vench].frame_index++;
                if(g_check_index[frameinfo.vench].frame_index != frameinfo.frame_no)
                {
                    // LOGE("err!!!frame_index:%d frame_no:%d",
                    //     check_index->frame_index, frameinfo.frame_no);
                    g_check_index[frameinfo.vench].frame_index = frameinfo.frame_no;
                    // Logfe("err!!!frame_index");
                }
            }

            ret = write_frame_to_file(frameinfo, g_recorder_status.enc_buf);
            if(ret != SUCCESS)
            {
                LOGE("wirte_es2pes_packet fail");
                return FAIL;
            }
        }
    }
    else if (ret == 0)
    {
        LOGV("ipcam_get_vencstream NOTHING");
#if RECORDER_LOG
        if (g_recorder_log_flag)
        {
            if(0 == vench_t[vench])
            {
                Logfi("recorder start ch[%d]", vench);
                vench_t[vench] = time(NULL);
            }
            if(state[vench] == 0 && time(NULL)-vench_t[vench] > time_out_sec)
            {
                state[vench] = 1;
                Logfi("no frame timeout %dsec ch[%d]", time_out_sec, vench);
                vench_t[vench] = time(NULL);
                update_logs_mgr();
            }
        }
#endif
    }
    else if (ret < 0)
    {
#if RECORDER_LOG
        Logfi("ipcam_get_vencstream ch[%d] FAIL", vench);
        update_logs_mgr();
#endif
        LOGE("ipcam_get_vencstream FAIL");
    }
    
    // // computer current size of the written file after save vframe;
    // g_recorder_status.aRecFileIdx.curFileLen += (PES_HEAD_LEN + frameinfo.frame_size);

#if 1
    ret = ipcam_get_aencstream(&frameinfo, g_recorder_status.enc_buf, g_recorder_status.buf_len, &getAFrameIndex);
    if(ret > 0)
    {
        noFrame = 0;

        if(g_recorder_status.vstart_timestamp) //视频开始有数据了才开始录音频
        {
            ret = write_frame_to_file(frameinfo, g_recorder_status.enc_buf);
            if(ret != SUCCESS)
            {
                LOGE("wirte_es2pes_packet fail");
                return FAIL;
            }
        }

    }
    else if (ret == 0)
    {
        LOGV("ipcam_get_aencstream NOTHING");
    }
    else if (ret < 0)
    {
        LOGE("ipcam_get_aencstream FAIL");
    }
#endif 

// // computer current size of the written file after save aframe;
//     g_recorder_status.aRecFileIdx.curFileLen += (PES_HEAD_LEN + frameinfo.frame_size);
    // LOGE("noFrame[%d]", noFrame);
    // usleep(35*1000);    //文件写入很快在G379上会出现get audio和video都为空的情况，导致录像不断启停

    return noFrame ? RECORDER_GET_NO_FRAME : 0;
}

static int save_record_frame(struct THR_TASK * ptask, EVENT_HANDLE_LIST_t * handler_list)
{
    #define VEN_CH_MAX 2
    int times = 0;
    int ret[VEN_CH_MAX] = {0};    //change by lwj

    // calculate the end time of file and unit: {
    MyTime fileTime_e, unitTime_e;

    fileTime_e.Hour = g_recorder_status.aRecFileIdx.fileTime_s.Hour;
    fileTime_e.Min = g_recorder_status.aRecFileIdx.fileTime_s.Min + REC_FILE_LEN_BY_MINS;
    if(fileTime_e.Min >= 60) {
        fileTime_e.Hour += 1;
        fileTime_e.Min = 0;
        if (24 == fileTime_e.Hour)
        {
            fileTime_e.Hour = 0; 
        }
        
    }
    fileTime_e.Sec = 0;

    unitTime_e.Hour = g_recorder_status.aRecFileIdx.unitTime_s.Hour;
    unitTime_e.Min = g_recorder_status.aRecFileIdx.unitTime_s.Min;
    unitTime_e.Sec = g_recorder_status.aRecFileIdx.unitTime_s.Sec + REC_UNIT_LEN_BY_SECS;
    if(unitTime_e.Sec >= 60) {
        unitTime_e.Min += 1;
        if(unitTime_e.Min >= 60) {
            unitTime_e.Hour += 1;
            unitTime_e.Min = 0;
            if (24 == unitTime_e.Hour)
            {
                unitTime_e.Hour = 0; 
            }
        }
        unitTime_e.Sec = 0;
    }

    // LOGI("[lwj test] fileTime_s[%02d-%02d-%02d] unitTime_s[%02d-%02d-%02d]",
    //     g_recorder_status.aRecFileIdx.fileTime_s.Hour, g_recorder_status.aRecFileIdx.fileTime_s.Min, g_recorder_status.aRecFileIdx.fileTime_s.Sec,
    //     g_recorder_status.aRecFileIdx.unitTime_s.Hour, g_recorder_status.aRecFileIdx.unitTime_s.Min, g_recorder_status.aRecFileIdx.unitTime_s.Sec);  
    // LOGI("[lwj test] fileTime_e[%02d-%02d-%02d] unitTime_e[%02d-%02d-%02d]",
    //     fileTime_e.Hour, fileTime_e.Min, fileTime_e.Sec,
    //     unitTime_e.Hour, unitTime_e.Min, unitTime_e.Sec);  
    
    do
    {
        if(1 == get_reboot_flg())
        {
    		LOGI("rebooting now,not recorder");
    		return -1;
        }

        if(1 == get_mmc_format_status())
        {
    		LOGI("MMC FORMATTING,wait!");
    		return -1;
        }

        for (int vench = 0; vench < VEN_CH_MAX; vench++)
        {
            ret[vench] = __save_record_frame(vench, ptask, handler_list);
        }

        // if(g_recorder_status.rec_fd < 0){
        //     check_and_start_record(ptask, handler_list);
        //     break;
        // }

        
        IPCNetTimeInfo_st timeInfo;
        get_local_time(g_timeZone, &timeInfo);
        // LOGI("[lwj test] curtime[%02d-%02d-%02d]", timeInfo.Time.Hour, timeInfo.Time.Min, timeInfo.Time.Sec);
        // 检查当前时间，是否到unit结束; 需要更新unit
        if(get_time_stamp_in_secs(&timeInfo.Time) >= get_time_stamp_in_secs(&unitTime_e)) {  // units end; 
            // LOGI("[lwj test] fileTime_e[%02d-%02d-%02d] unitTime_e[%02d-%02d-%02d]",
            //     fileTime_e.Hour, fileTime_e.Min, fileTime_e.Sec,
            //     unitTime_e.Hour, unitTime_e.Min, unitTime_e.Sec);
            if (0 == get_time_stamp_in_secs(&unitTime_e))   //跨天结束24:00:00
            {
                if(get_time_stamp_in_secs(&timeInfo.Time) < 10)
                {
                    g_recorder_status.aRecFileIdx.unitTime_s.Hour = unitTime_e.Hour;
                    g_recorder_status.aRecFileIdx.unitTime_s.Min = unitTime_e.Min;
                    g_recorder_status.aRecFileIdx.unitTime_s.Sec = unitTime_e.Sec;

                    int idx = 59;
                    if(idx >= 0) {
                        g_recorder_status.aRecFileIdx.recMinUnits[idx].alarmed = get_alarm_status();
                        g_recorder_status.aRecFileIdx.recMinUnits[idx].offset = g_recorder_status.aRecFileIdx.curFileLen;
                        g_recorder_status.aRecFileIdx.recMinUnits[idx].type =  g_recorder_status.recType.type;
        #ifdef RECORDER_DEBUG
                        LOGI("unitTime_s - %02d:%02d:%02d", unitTime_e.Hour, unitTime_e.Min, unitTime_e.Sec);
                        LOGI("info index[%d] alarm[%d] offset[%d] type[0x%x]", idx, g_recorder_status.aRecFileIdx.recMinUnits[idx].alarmed, 
                            g_recorder_status.aRecFileIdx.recMinUnits[idx].offset,
                            g_recorder_status.aRecFileIdx.recMinUnits[idx].type);
        #endif
                        unitTime_e.Hour = g_recorder_status.aRecFileIdx.unitTime_s.Hour;
                        unitTime_e.Min = g_recorder_status.aRecFileIdx.unitTime_s.Min;
                        unitTime_e.Sec = g_recorder_status.aRecFileIdx.unitTime_s.Sec + REC_UNIT_LEN_BY_SECS;
                        if(unitTime_e.Sec >= 60) {
                            unitTime_e.Min += 1;
                            if(unitTime_e.Min >= 60) { 
                                unitTime_e.Hour += 1;
                                unitTime_e.Min = 0;
                            }
                            unitTime_e.Sec = 0;
                        }
                    }
                }
            }
            else
            { 
                g_recorder_status.aRecFileIdx.unitTime_s.Hour = unitTime_e.Hour;
                g_recorder_status.aRecFileIdx.unitTime_s.Min = unitTime_e.Min;
                g_recorder_status.aRecFileIdx.unitTime_s.Sec = unitTime_e.Sec;

                int idx = time_to_idx_in_a_file_idx(&g_recorder_status.aRecFileIdx);
                if(idx >= 0) {
                    g_recorder_status.aRecFileIdx.recMinUnits[idx].alarmed = get_alarm_status();
                    g_recorder_status.aRecFileIdx.recMinUnits[idx].offset = g_recorder_status.aRecFileIdx.curFileLen;
                    g_recorder_status.aRecFileIdx.recMinUnits[idx].type =  g_recorder_status.recType.type;
    #ifdef RECORDER_DEBUG
                    LOGI("unitTime_s - %02d:%02d:%02d", unitTime_e.Hour, unitTime_e.Min, unitTime_e.Sec);
                    LOGI("info index[%d] alarm[%d] offset[%d] type[0x%x]", idx, g_recorder_status.aRecFileIdx.recMinUnits[idx].alarmed, 
                        g_recorder_status.aRecFileIdx.recMinUnits[idx].offset,
                        g_recorder_status.aRecFileIdx.recMinUnits[idx].type);
    #endif
                    unitTime_e.Hour = g_recorder_status.aRecFileIdx.unitTime_s.Hour;
                    unitTime_e.Min = g_recorder_status.aRecFileIdx.unitTime_s.Min;
                    unitTime_e.Sec = g_recorder_status.aRecFileIdx.unitTime_s.Sec + REC_UNIT_LEN_BY_SECS;
                    if(unitTime_e.Sec >= 60) {
                        // added by lwj
                        // 一分钟保存一次
                        char recIdxPath[MAX_PATH];
                        sprintf(recIdxPath, "%s/%s.idx", g_recorder_status.rec_dir, g_recorder_status.rec_name);
                        LOGI("open idx file path:%s", recIdxPath);
                        save_rec_units(recIdxPath);
                        __p_local_time();
                        // end by lwj

                        unitTime_e.Min += 1;
                        if(unitTime_e.Min >= 60) {
                            unitTime_e.Hour += 1;
                            unitTime_e.Min = 0;
                        }
                        unitTime_e.Sec = 0;
                    }
                }
            }
            
        }

        // 检查当前时间，10 mins? 是否需要关闭文件，并且重置g_record_status。
        if(get_time_stamp_in_secs(&timeInfo.Time) >= get_time_stamp_in_secs(&fileTime_e)) {
            // LOGI("fileTime_e:%02d:%02d:%02d; timeInfo.Time:%02d:%02d:%02d", fileTime_e.Hour, fileTime_e.Min, fileTime_e.Sec,
            //                                                                 timeInfo.Time.Hour, timeInfo.Time.Min, timeInfo.Time.Sec);
            // LOGI("more than 10 mins; stop recording");

            if (0 == get_time_stamp_in_secs(&fileTime_e))   //跨天结束24:00:00
            {
                if(get_time_stamp_in_secs(&timeInfo.Time) < 10)
                {
                    if(stop_rec(ptask, handler_list) < 0)
                    {
                        close(g_recorder_status.rec_fd);
                        sync();
                    }
                }
            }
            else
            {
                if(stop_rec(ptask, handler_list) < 0)
                {
                    close(g_recorder_status.rec_fd);
                    sync();
                }
            }
            break;
        }

        if(g_recorder_status.rec_fd >= 0) {
            RECORD_STATUS sta = check_and_stop_record(ptask, handler_list);
            if(ERROR == sta) {
                close(g_recorder_status.rec_fd);
                sync();
            }
            else if(NO_RECORD == sta) {
                break;
            }
            else {
                // do nothing;
            }
        }

        for (int i = 0; i < VEN_CH_MAX; i++)
        {
            LOGV("vench:%d ret:%d times:%d",i, ret[i], times);
        }
        
    }while(times++ < 100 && (ret[0] == 0 || ret[1] == 0));

    // return ret;
    return 0;//文件写入很快在G379上会出现get audio和video都为空的情况，导致录像不断启停
}

char* get_record_dir_by_date(char *root_dir, MyDate *pDate, bool bCreat) {        // pDate is NULL, get cur time to generate the rec dir; bCreat: no dir, create it
    static char *recDir[MAX_PATH];
    MyDate date;

    if(NULL == root_dir) {
        LOGE("ERROR: root_dir is NULL");
        return NULL;
    }

    if(pDate == NULL) {
        IPCNetTimeInfo_st timeInfo;
        get_local_time(g_timeZone, &timeInfo);
        date.Year = timeInfo.Date.Year;
        date.Mon = timeInfo.Date.Mon;
        date.Day = timeInfo.Date.Day;
    }
    else {
        date.Year = pDate->Year;
        date.Mon = pDate->Mon;
        date.Day = pDate->Day;
    }

    sprintf(recDir, "%s%04hd%02d/%02d", root_dir, date.Year, date.Mon, date.Day);
    
	if(access(recDir,F_OK) < 0)
	{
        if(bCreat) {
            if(mkdirs(recDir,0755) < 0) {
                LOGE("mkdirs fail: %s", recDir);
                return NULL;
            }
        }
        else {
            return NULL;
        }
	}
    
    return recDir;
}

// static void set_rec_file_idx_by_units(RECORDER_UNIT *recMinUnits) {
//     // added by lwj
//     IPCNetTimeInfo_st timeInfo;
//     get_local_time(g_timeZone, &timeInfo);

//     g_recorder_status.aRecFileIdx.fileTime_s.Hour = timeInfo.Time.Hour;
//     g_recorder_status.aRecFileIdx.fileTime_s.Min = timeInfo.Time.Min - timeInfo.Time.Min % 10;
//     g_recorder_status.aRecFileIdx.fileTime_s.Sec = 0;

//     // g_recorder_status.aRecFileIdx.unitTime_s = g_recorder_status.aRecFileIdx.fileTime_s;
    
//     g_recorder_status.aRecFileIdx.unitTime_s.Hour = timeInfo.Time.Hour;
//     g_recorder_status.aRecFileIdx.unitTime_s.Min = timeInfo.Time.Min;
//     g_recorder_status.aRecFileIdx.unitTime_s.Sec = timeInfo.Time.Sec - timeInfo.Time.Sec % 10;

//     // g_recorder_status.aRecFileIdx.curFileLen = 0;
//     // g_recorder_status.aRecFileIdx.curIdx = 0;
//     // end by lwj
// }

static void *get_file_time_s(IN MyTime *pTime, OUT MyTime *pTime_s) {
    // MyTime time_t;
    IPCNetTimeInfo_st timeInfo;
    if(NULL == pTime) {
        get_local_time(g_timeZone, &timeInfo);
    }
    else 
    {
        timeInfo.Time.Hour = pTime->Hour;
        timeInfo.Time.Min = pTime->Min;
        timeInfo.Time.Sec = pTime->Sec;
    }

    pTime_s->Hour = timeInfo.Time.Hour;
    pTime_s->Min = timeInfo.Time.Min - timeInfo.Time.Min % 10;
    pTime_s->Sec = 0;
}

static void *get_unit_time_s(IN MyTime *pTime, OUT MyTime *pTime_s) {
    MyTime time_t;
    IPCNetTimeInfo_st timeInfo;
    if(NULL == pTime) {
        get_local_time(g_timeZone, &timeInfo);
    }
    else 
    {
        timeInfo.Time.Hour = pTime->Hour;
        timeInfo.Time.Min = pTime->Min;
        timeInfo.Time.Sec = pTime->Sec;
    }

    pTime_s->Hour = timeInfo.Time.Hour;
    pTime_s->Min = timeInfo.Time.Min;
    pTime_s->Sec = timeInfo.Time.Sec - timeInfo.Time.Sec % 10;
}

static int init_file_idx(char *idxPath, int recfileSize) {
    if(NULL == idxPath) {
        reset_rec_file_idx_by_time(NULL);
    }
    else {
        // 打开idx文件，如果没有文件，则创建
        int ret = load_rec_units(idxPath, g_recorder_status.aRecFileIdx.recMinUnits);
        if(ret < 0) {
            LOGE("load idx file fail: %s", idxPath);
            reset_rec_file_idx_by_time(NULL);
        }
        else {      // set recUnits by idx file
            LOGI("[lwj test] set_rec_file_idx_by_units path:%s", idxPath);
            
            // set_rec_file_idx_by_units(g_recorder_status.aRecFileIdx.recMinUnits);
            g_recorder_status.aRecFileIdx.curFileLen = recfileSize;
            get_file_time_s(NULL, &g_recorder_status.aRecFileIdx.fileTime_s);
            get_unit_time_s(NULL, &g_recorder_status.aRecFileIdx.unitTime_s);
            int idx = time_to_idx_in_a_file_idx(&g_recorder_status.aRecFileIdx);
    #if 0
        // 使能后会导致事件录像返回的录像时间段多了一个units的时间
            LOGI("[lwj test] idx=%d", idx);
            g_recorder_status.aRecFileIdx.recMinUnits[idx].alarmed = get_alarm_status();
            g_recorder_status.aRecFileIdx.recMinUnits[idx].offset = g_recorder_status.aRecFileIdx.curFileLen;
            g_recorder_status.aRecFileIdx.recMinUnits[idx].type = g_recorder_status.recType.type;
    #endif
        #ifdef RECORDER_DEBUG
            LOGI("[lwj test] fileTime_s[%02d-%02d-%02d] unitTime_s[%02d-%02d-%02d]",
                g_recorder_status.aRecFileIdx.fileTime_s.Hour, g_recorder_status.aRecFileIdx.fileTime_s.Min, g_recorder_status.aRecFileIdx.fileTime_s.Sec,
                g_recorder_status.aRecFileIdx.unitTime_s.Hour, g_recorder_status.aRecFileIdx.unitTime_s.Min, g_recorder_status.aRecFileIdx.unitTime_s.Sec);
        #endif
        }
        //////////// 需要处理。
    }
}

static int open_record_files(char *root_dir)
{
    char recFilePath[MAX_PATH];
    char recIdxPath[MAX_PATH];
	char curRecDir[MAX_PATH];
    struct stat fStat;
    int  ret;

    IPCNetTimeInfo_st timeInfo;
    get_local_time(g_timeZone, &timeInfo);

    sprintf(curRecDir, "%s%04hd%02d/%02d", root_dir, timeInfo.Date.Year, timeInfo.Date.Mon, timeInfo.Date.Day);
    
	if(access(curRecDir,F_OK) < 0)
	{
		if(mkdirs(curRecDir,0755) < 0) {
            LOGE("mkdirs fail: %s", curRecDir);
        #if RECORDER_LOG2
            Logfi("mkdirs fail: %s", curRecDir);
        #endif
			return INVAILD_HANDLE_VALUE;
        }
	}

    // update recName and recDir;
    // strcpy(g_recorder_status.rec_dir, curRecDir);
    // strcpy(g_recorder_status.aDayNightIdx.recName, get_record_name_by_local_time(time_cfg_ptr));

    char *recName =  get_record_name_by_time(&timeInfo.Time);
    sprintf(recFilePath, "%s/%s.pes", curRecDir, recName);
    sprintf(recIdxPath, "%s/%s.idx", curRecDir, recName);
    LOGI("open record file path:%s, idx filePath: %s", recFilePath, recIdxPath);
    
	if(0 == access(recFilePath, F_OK))//存在
	{
	    // LOGI("%s has exist and open it", recFilePath);
        g_recorder_status.rec_fd = open_pes_record(recFilePath);
        if(g_recorder_status.rec_fd < 0) {
            LOGE("open_pes_record failed: %s", recFilePath);
        #if RECORDER_LOG2
            Logfi("open_pes_record failed: %s", recFilePath);
        #endif
			return INVAILD_HANDLE_VALUE;
        }

        ret = fstat(g_recorder_status.rec_fd, &fStat);
        if(ret < 0) {
            LOGE("fstat failed: %s", recFilePath);
        #if RECORDER_LOG2
            Logfi("fstat failed: %s", recFilePath);
        #endif
            close(g_recorder_status.rec_fd);
            return INVAILD_HANDLE_VALUE;                        /// ???????????????
        }

        init_file_idx(recIdxPath, fStat.st_size);

        // // 打开idx文件，如果没有文件，则创建
        // ret = load_rec_units(recIdxPath, g_recorder_status.aRecFileIdx.recMinUnits);
        // if(ret < 0) {
        //     LOGE("load idx file fail: %s", recIdxPath);
        //     reset_rec_file_idx_by_time(&timeInfo.Time);
        // }
        // else {
        //     LOGI("[lwj test] set_rec_file_idx_by_units");
        //     set_rec_file_idx_by_units(g_recorder_status.aRecFileIdx.recMinUnits);
        //     g_recorder_status.aRecFileIdx.curFileLen = fStat.st_size;
        //     int idx = time_to_idx_in_a_file_idx(&g_recorder_status.aRecFileIdx);
        //     g_recorder_status.aRecFileIdx.recMinUnits[idx].alarmed = get_alarm_status();
        //     g_recorder_status.aRecFileIdx.recMinUnits[idx].offset = g_recorder_status.aRecFileIdx.curFileLen;
        //     g_recorder_status.aRecFileIdx.recMinUnits[idx].type = g_recorder_status.recType.type;
        // }
        // //////////// 需要处理。



        // g_recorder_status.aRecFileIdx.fileTime_s.Hour = timeInfo.Time.Hour;
        // g_recorder_status.aRecFileIdx.fileTime_s.Min = timeInfo.Time.Min - timeInfo.Time.Min % 10;
        // g_recorder_status.aRecFileIdx.fileTime_s.Sec = 0;

        // g_recorder_status.aRecFileIdx.unitTime_s.Hour = timeInfo.Time.Hour;
        // g_recorder_status.aRecFileIdx.unitTime_s.Min = timeInfo.Time.Min;
        // g_recorder_status.aRecFileIdx.unitTime_s.Sec = timeInfo.Time.Sec - timeInfo.Time.Sec % 10;
	}
	else {      // create record files;
        // reset rec_file_idx before create a new record pes file;
        reset_rec_file_idx_by_time(&timeInfo.Time);
	    g_recorder_status.rec_fd = create_pes_record(recFilePath);
        // initRecUnits
        if(0 == access(recIdxPath, F_OK))   // 存在
        {
            char cmd[MAX_CMD];
            sprintf(cmd, "rm -rf %s", recIdxPath);
            Logfi("rm %s[%s]", __FUNCTION__, cmd);
            system(cmd);  
        }
    }

    strcpy(g_recorder_status.rec_dir, curRecDir);
    strcpy(g_recorder_status.rec_name, recName);

    return 0;
}

static int close_record_files(char *root_dir) {
    char recFilePath[MAX_PATH];
    char recIdxPath[MAX_PATH];
	char curRecDir[MAX_PATH];
    struct stat fStat;
    int     ret;

    IPCNetTimeInfo_st timeInfo;
    get_local_time(g_timeZone, &timeInfo);
    sprintf(curRecDir, "%s%04hd%02d/%02d", root_dir, timeInfo.Date.Year, timeInfo.Date.Mon, timeInfo.Date.Day);
	if(access(curRecDir,F_OK) < 0)
	{
		if(mkdirs(curRecDir,0755) < 0) {
            LOGE("mkdirs fail: %s", curRecDir);
			return INVAILD_HANDLE_VALUE;
        }
	}

    // close pes file;
    if(g_recorder_status.rec_fd >= 0) {
        stop_pes_record(g_recorder_status.rec_fd);
        sync();   
        g_recorder_status.rec_fd = INVAILD_HANDLE_VALUE;
    }

    // save idx file
    sprintf(recIdxPath, "%s/%s.idx", g_recorder_status.rec_dir, g_recorder_status.rec_name);
    // LOGI("open idx file path:%s", recIdxPath);
    save_rec_units(recIdxPath);
    __p_local_time();
}

int start_rec(THR_TASK_t *ptask, EVENT_HANDLE_LIST_t* handler_list)
{
    IPCNetTimeInfo_st time_cfg;
    int ret = 0;
    IPCNetRecordGetCfg_st record_req;
    IPCNetRecordCfg_st record_cfg;

    if(1 == get_reboot_flg())
    {
		LOGI("rebooting now,not recorder");
		return FAIL;
    }

    if(1 == get_mmc_format_status())
    {
		LOGI("MMC FORMATTING,wait!");
		return FAIL;
    }

    // reserved_size_mb = get_reserved_size(record_cfg->VeCh, rec_times);
	// if(need_recycle_disk(reserved_size_mb))
    if(need_recycle_disk(RESERVED_SIZE_MB))
	{
        broadcast_event();
		ret = recycle_disk_del_day_dir(RESERVED_SIZE_MB);
	    if(ret != 0)	
	    {
			LOGI("check_and_recycle_disk ret=%d",ret);
	        reset_event_timeout(ptask, handler_list, 3*1000);
			return 0;
	    }
    }

    // memset(&record_req, 0, sizeof(record_req));
    // record_req.ViCh = 0;
    // record_req.RecType = 0; //change by lwj
    // if(!g_disk_valid)
    // {
    //     LOGD("g_disk_valid is[%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
    //     return 0;
    // }
    // get_recorder_cfg(&record_req, &record_cfg);


    ret = open_record_files(get_rec_root());
    if(ret < 0)
    {
#if RECORDER_LOG2
        Logfi("open_record_files fail");
#endif
        LOGE("open_record_files fail");
        return FAIL;
    }
    
    //added by lwj
    __p_local_time();
    unsigned char t = time(NULL)%10;
    t <<= 4;
    g_recorder_status.alarmStat.alarmed &= 0x0F;
    t = t|g_recorder_status.alarmStat.alarmed;
    LOGI("alarm_offset_t[%x]", t);
    g_recorder_status.alarmStat.alarmed = t;
    
    if(0 == g_recorder_status.steam_shm_init)
    {
        #if 1   
        sepcam_av_enc_init(ALL_CHANNEL);
        #else
		sepcam_av_enc_init(record_cfg->VeCh);
        #endif
		g_recorder_status.steam_shm_init = 1;
    }
    else
    { 
        rec_flush_stream_shm();
    }
    IPCNetVideoIFrame_st ipcnet_iframe;
    ipcnet_iframe.ViCh = 0;
    ipcnet_iframe.VenCh = 0;
    ipcam_set_device_config(IPCNET_VIDEO_IFRAME_REQ, &ipcnet_iframe);

    ipcnet_iframe.ViCh = 0;
    ipcnet_iframe.VenCh = 1;
    ipcam_set_device_config(IPCNET_VIDEO_IFRAME_REQ, &ipcnet_iframe);
    // end by lwj

    init_rdwr_buff(g_recorder_status.wr_buf, 0);    
    reset_event_timeout(ptask, handler_list, 10);
    
    return 0;
}

int stop_rec(THR_TASK_t *ptask, EVENT_HANDLE_LIST_t* handler_list)
{
    int ret = 0;
    IPCNetRecordGetCfg_st record_req;
    IPCNetRecordCfg_st record_cfg;

    // memset(&record_req, 0, sizeof(record_req));
    // record_req.ViCh = 0;
    // record_req.RecType = 0; //change by lwj
    // if(!g_disk_valid)
    // {
    //     LOGD("g_disk_valid is[%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
    //     reset_event_timeout(ptask, handler_list, CHECK_RECORD_TASK_TIMESOUT);
    //     return 0;
    // }
    // get_recorder_cfg(&record_req, &record_cfg);

    if(g_recorder_status.rec_fd >= 0)
    {
        record_async_data_buff(g_recorder_status.rec_fd, g_recorder_status.wr_buf);

        ret = close_record_files(get_rec_root());
        if(ret < 0)
        {
            LOGE("close_record_files fail");
            reset_event_timeout(ptask, handler_list, CHECK_RECORD_TASK_TIMESOUT);
            return FAIL;
        }

        // stop_pes_record(g_recorder_status.rec_fd);
        // sync();
        
        // g_recorder_status.rec_fd = INVAILD_HANDLE_VALUE;

        // char recIdxPath[MAX_PATH];
        // sprintf(recIdxPath, "%s/%s.idx", g_recorder_status.rec_dir, g_recorder_status.aDayNightIdx.recName);
        // LOGI("save idx file path:%s", recIdxPath);
        // save_rec_units(recIdxPath);
        
    }
    reset_event_timeout(ptask, handler_list, CHECK_RECORD_TASK_TIMESOUT);
    return 0;
}

int IPCMsgGetRecordCfg(IPCNetRecordCfg_st *pRecordCfg, IPCNetRecordGetCfg_st *pRecordGetCfg);

int get_recorder_cfg(IPCNetRecordGetCfg_st *pRecordGetCfg, IPCNetRecordCfg_st *pRecordCfg)
{
    return IPCMsgGetRecordCfg(pRecordCfg, pRecordGetCfg);
}

static int opt_start_record(THR_TASK_t *ptask)
{
    EVENT_HANDLE_LIST_t* handler_list = get_event_handler(ptask, g_recorder_eventid);
    IPCNetRecordGetCfg_st record_req;
    IPCNetRecordCfg_st record_cfg;
    IPCNetTimeInfo_st time_cfg;

    int record_times = 0;
    int ret;
    int time_type = get_recorder_time_type(0);

    if(g_recorder_status.rec_fd >= 0)
    {
        LOGI("end_record common record");
        stop_rec(ptask, handler_list);
    }
    
    if(handler_list)
    {
        get_local_time(g_timeZone, &time_cfg);
        
        // memset(&record_req, 0, sizeof(record_req));
        // record_req.ViCh = 0;
        // record_req.RecType = 0; //change by lwj
        // if(!g_disk_valid)
        // {
		// 	LOGD("g_disk_valid is[%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
		// 	return 0;
        // }
        // get_recorder_cfg(&record_req, &record_cfg);

        ret = start_rec(ptask, handler_list);
        if(ret != SUCCESS)
        {
            LOGE("start_record fail");
            return FAIL;
        }
    }
    return 0;
}

static int opt_stop_record(THR_TASK_t *ptask)
{
    IPCNetRecordGetCfg_st record_req;
    IPCNetRecordCfg_st record_cfg;

    EVENT_HANDLE_LIST_t* handler_list = get_event_handler(ptask, g_recorder_eventid);
    if(g_recorder_status.rec_fd >= 0)
    {
        memset(&record_req, 0, sizeof(record_req));
        record_req.ViCh = 0;
        record_req.RecType = 0; //change by lwj

        if(!g_disk_valid)
        {
			LOGD("g_disk_valid is[%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
			return 0;
        }

        get_recorder_cfg(&record_req, &record_cfg);

        LOGI("end_record common record");
        stop_rec(ptask, handler_list);
    }
    return 0;
}

// added by lwj
// static int on_timecheck(THR_TASK_t *ptask, EVENT_HANDLE_LIST_t* handler_list)
// {
//     static int flag = 0;
//     time_t cur = time(NULL);
//     cur += 8*3600;
//     // cur = cur%(24*3600);
//     // LOGI("time cur[%u-%u-%u]", cur/3600, ((cur)%3600)/60, (cur)%60);
//     if ((cur%(24*3600) < 2) && (flag))
//     {
//         //第二天
//         LOGI("need dealt info");
//         opt_stop_record(ptask);
//         // self_check_for_sd(RECORDER_PATH);
//         flag = 0;
//     }
//     else
//     {
//         flag = 1;
//     }
//     // chackAndSaveAlarmInfo();
//     return 0;
// }
// end by lwj

static int on_recorder_timeout(THR_TASK_t *ptask, EVENT_HANDLE_LIST_t* handler_list)
{
    int ret;
	LOGV("g_recorder_status.rec_fd=%d,reboot_flg=%d,mmc_format=%d timeout_val:%u",
		g_recorder_status.rec_fd, get_reboot_flg(), get_mmc_format_status(), handler_list->event.timeout_val);	

    if(1 == get_reboot_flg())
    {
		Logfi("rebooting now,not recorder");
		return 0;
    }

    if(1 == get_mmc_format_status())
    {
		Logfi("MMC FORMATTING,wait!");
		return 0;
    }

    // added by lwj
    if (time(NULL) < 1664553601 || (g_timeZone > 12 || g_timeZone < -12)) //2022-10-01 00:00:01 1664553601
    {
		LOGI("Time is out of sync,wait!");
        return 0;
    }
    // end by lwj
    
    if(g_recorder_status.rec_fd < 0) {
        RECORD_STATUS sta = check_and_start_record(ptask, handler_list);

        if(ERROR == sta) {
            return -1;
        }
        else if(RECORDING == sta) {
            ret = save_record_frame(ptask, handler_list);
            if(ret < 0)
            {
                LOGE("save_record_frame failed; stop recording");
                stop_rec(ptask, handler_list);
            }
        }
        else {
            // do nothing;
        }
    }
    else {
        ret = save_record_frame(ptask, handler_list);
        if(ret < 0)
        {
            LOGE("save_record_frame failed; stop recording");
            stop_rec(ptask, handler_list);
        }
    }

    return 0;
}

void update_clocktime()
{
	struct timespec ts;
    uint64_t sec_tmp,usec_tmp;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
	{
        sec_tmp = ts.tv_sec;
        usec_tmp = ts.tv_nsec/1000;
        LOGV("clock_gettime ts:%ld,%ld sec_tmp:%lld usec_tmp:%lld\n",
            ts.tv_sec, ts.tv_nsec, sec_tmp, usec_tmp);
	    g_clock_time = sec_tmp*1000 + usec_tmp/1000;
	}
	else
	{
	    LOGE("update_clocktime fail");
	}
}

RECORD_STATUS check_and_start_record(struct THR_TASK * ptask, EVENT_HANDLE_LIST_t * handler_list) 
{
    IPCNetRecordGetCfg_st record_req;
    IPCNetRecordCfg_st record_cfg;
    MyTime *pLastTime, *pCurTime;
    RECORD_STATUS ret;

    int curTimeStamp;

    IPCNetTimeInfo_st timeInfo;
    get_local_time(g_timeZone, &timeInfo);

#ifdef RECORDER_DEBUG
    // LOGI("enter check_and_start_record");
#endif

    // 每十秒确认下SD卡的状态
    {
        static time_t uploadTime = 0;
        if (time(NULL) - uploadTime > 10)
        {
            uploadTime = time(NULL);
            upload_disk_valid();
        }
    }
    
    if(!g_disk_valid)
    {
		LOGD("g_disk_valid is [%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
#if RECORDER_LOG2 
        {
            static int tt = 0;
            if (time(NULL) - tt > 60)
            {
                tt = time(NULL);
                Logfi("g_disk_valid is [%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
            }
        }
#endif
		return ERROR;
    }

    if(g_recorder_status.rec_fd >= 0){
        LOGI("g_recorder_status.rec_fd >= 0");
#if RECORDER_LOG2 
        {
            static int tt = 0;
            if (time(NULL) - tt > 60)
            {
                tt = time(NULL);
                Logfi("g_recorder_status.rec_fd >= 0");
            }
        }
#endif
        return RECORDING;
    }
    
#if 0 
    // get record config: {
    memset(&record_req, 0, sizeof(record_req));
    record_req.ViCh = 0;
    record_req.RecType = 0;
    get_recorder_cfg(&record_req, &record_cfg);
    // }

    if(0 == record_cfg.RecTime[0].En) {
        g_recorder_status.recType.type = RECORDER_COLSED;
        return NO_RECORD;
    }
    else {     // 1 == record_cfg.RecTime[0].En
        if(0 == record_cfg.recMode) {       // common record
            g_recorder_status.recType.type = RECORDER_COMMON;
            parse_str_time(&(g_recorder_status.recType.recTime_s), record_cfg.RecTime[0].St1);
            parse_str_time(&(g_recorder_status.recType.recTime_e), record_cfg.RecTime[0].Ed1);

            int curTimeStamp = get_time_stamp_in_secs(&timeInfo.Time);
            if(curTimeStamp >= get_time_stamp_in_secs(&g_recorder_status.recType.recTime_s) && 
                    curTimeStamp <= get_time_stamp_in_secs(&g_recorder_status.recType.recTime_e))
            {
                if(start_rec(ptask, handler_list) < 0) {    
                    LOGE("start_rec failed");
                    return ERROR;
                }
                else {
                    // start record success
                    return RECORDING;
                }
            }
            else {
                return NO_RECORD;
            }
        }
        else {      // alarm record;
            g_recorder_status.recType.type = RECORDER_ALARM;
            
            pLastTime = &g_recorder_status.alarmStat.lastAlarmTime;
            pCurTime = &timeInfo.Time;
            if(g_recorder_status.alarmStat.alarmed && 
                    (get_time_stamp_in_secs(pLastTime)+10 >= get_time_stamp_in_secs(pCurTime)))  // the lasttime alarm is valid, need start record
            {
                if(start_rec(ptask, handler_list) < 0) {    
                    LOGE("start_rec failed");
                    return ERROR;
                }
                else {
                    // start record success
                    return RECORDING;
                }
            }
            else {
                return NO_RECORD;
            }
        }
    }
#else
// #ifdef RECORDER_DEBUG
//     LOGI("type:%d, %02d:%02d:%02d - %02d:%02d:%02d", g_recorder_status.recType.type,
//                                                      g_recorder_status.recType.recTime_s.Hour,
//                                                      g_recorder_status.recType.recTime_s.Min,
//                                                      g_recorder_status.recType.recTime_s.Sec,
//                                                      g_recorder_status.recType.recTime_e.Hour,
//                                                      g_recorder_status.recType.recTime_e.Min,
//                                                      g_recorder_status.recType.recTime_e.Sec);
// #endif
    switch(g_recorder_status.recType.type) {
        case RECORDER_COLSED:
        #ifdef RECORDER_DEBUG
            LOGI("check start, recorder colse");
        #endif
        #if RECORDER_LOG2
            {
                static int tt = 0;
                if (time(NULL) - tt > 60)
                {
                    tt = time(NULL);
                    Logfi("rec disable");
                }
            }
        #endif
            ret = NO_RECORD;
            break;
        case RECORDER_COMMON:
        #ifdef RECORDER_DEBUG
            LOGI("check start, recorder conmmon ");
        #endif
            if (0 == check_record_time(&timeInfo))
            {
                if(start_rec(ptask, handler_list) < 0) {   
                    LOGE("start_rec failed");
                #if RECORDER_LOG2
                    Logfi("start_rec failed");
                #endif
                    ret = ERROR;
                }
                else {
                    // start record success
                    ret = RECORDING;
                }
            }
            else {
            #if RECORDER_LOG2
                {
                    static int tt = 0;
                    if (time(NULL) - tt > 60)
                    {
                        tt = time(NULL);
                        Logfi("check_record_time  not require video recording");
                    }
                }
            #endif
                ret = NO_RECORD;
            }
            
            // curTimeStamp = get_time_stamp_in_secs(&timeInfo.Time);
            // if(curTimeStamp >= get_time_stamp_in_secs(&g_recorder_status.recType.recTime_s) && 
            //         curTimeStamp <= get_time_stamp_in_secs(&g_recorder_status.recType.recTime_e))
            // {
            //     if(start_rec(ptask, handler_list) < 0) {    
            //         LOGE("start_rec failed");
            //         ret = ERROR;
            //     }
            //     else {
            //         // start record success
            //         ret = RECORDING;
            //     }
            // }
            // else {
            //     ret = NO_RECORD;
            // }
            break;
        case RECORDER_ALARM:
        #ifdef RECORDER_DEBUG
            LOGI("check start, recorder alarm");
        #endif
            pLastTime = &g_recorder_status.alarmStat.lastAlarmTime;
            pCurTime = &timeInfo.Time;
            if (0 == check_record_time(&timeInfo))
            {
                if(g_recorder_status.alarmStat.alarmed && 
                        (get_time_stamp_in_secs(pLastTime)+10 >= get_time_stamp_in_secs(pCurTime)))  // the lasttime alarm is valid, need start record
                {
                    if(start_rec(ptask, handler_list) < 0) {    
                        LOGE("start_rec failed");
                        ret = ERROR;
                    }
                    else {
                        // start record success
                        ret = RECORDING;
                    }
                }
                else {
                    ret = NO_RECORD;
                }
            }
            else
            {
                ret = NO_RECORD;
            }
        #if RECORDER_LOG2
            {
                static int tt = 0;
                if (time(NULL) - tt > 60)
                {
                    tt = time(NULL);
                    Logfi("alarm rec disable");
                }
            }
        #endif
            break;
        default:
            ret = NO_RECORD;
            break;
    }
#endif
    return ret;
}

RECORD_STATUS check_and_stop_record(struct THR_TASK * ptask, EVENT_HANDLE_LIST_t * handler_list) 
{
    IPCNetRecordGetCfg_st record_req;
    IPCNetRecordCfg_st record_cfg;
    MyTime *pLastTime, *pCurTime;
    RECORD_STATUS   ret;

    IPCNetTimeInfo_st timeInfo;
    get_local_time(g_timeZone, &timeInfo);
    
    if(!g_disk_valid)
    {
		LOGD("g_disk_valid is [%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
		return ERROR;
    }

    if(g_recorder_status.rec_fd < 0){
        LOGI("g_recorder_status.rec_fd < 0");
        return NO_RECORD;
    }
    
#if 0
    // get record config: {
    memset(&record_req, 0, sizeof(record_req));
    record_req.ViCh = 0;
    record_req.RecType = 0;
    get_recorder_cfg(&record_req, &record_cfg);
    // }

    // check if the record stop or not;
    if(0 == record_cfg.RecTime[0].En) {
        if(stop_rec(ptask, handler_list) < 0)  // stop record success;
        {
            LOGE("stop_rec failed");
            return ERROR;
        }
        else {
            return NO_RECORD;
        }
    }
    else{     // 1 == record_cfg.RecTime[0].En
        if(0 == record_cfg.recMode) {       //common record
            g_recorder_status.recType.type = RECORDER_COMMON;
            parse_str_time(&(g_recorder_status.recType.recTime_s), record_cfg.RecTime[0].St1);
            parse_str_time(&(g_recorder_status.recType.recTime_e), record_cfg.RecTime[0].Ed1);
            // LOGI("g_recorder_status.recType.recTime_s: %02d-%02d-%02d, %d", 
            //         g_recorder_status.recType.recTime_s.Hour, 
            //         g_recorder_status.recType.recTime_s.Min,
            //         g_recorder_status.recType.recTime_s.Sec,
            //         get_time_stamp_in_secs(&g_recorder_status.recType.recTime_s));
            // LOGI("g_recorder_status.recType.recTime_e: %02d-%02d-%02d, %d", 
            //         g_recorder_status.recType.recTime_e.Hour, 
            //         g_recorder_status.recType.recTime_e.Min,
            //         g_recorder_status.recType.recTime_e.Sec,
            //         get_time_stamp_in_secs(&g_recorder_status.recType.recTime_e));

            // LOGI("timeInfo.Time: %02d-%02d-%02d, %d", 
            //         timeInfo.Time.Hour, 
            //         timeInfo.Time.Min,
            //         timeInfo.Time.Sec,
            //         get_time_stamp_in_secs(&timeInfo.Time));

            // check record times
            int curTimeStamp = get_time_stamp_in_secs(&timeInfo.Time);
            if(curTimeStamp < get_time_stamp_in_secs(&g_recorder_status.recType.recTime_s) || 
                    curTimeStamp > get_time_stamp_in_secs(&g_recorder_status.recType.recTime_e))
            {
                if(stop_rec(ptask, handler_list) < 0) {
                    LOGE("stop_rec failed");
                    return ERROR;
                }
                else {
                    // start record success
                    return NO_RECORD;
                }
            }
            else {
                return RECORDING;
            }
        }
        else {                  //alarm record
            g_recorder_status.recType.type = RECORDER_ALARM;
            
            pLastTime = &g_recorder_status.alarmStat.lastAlarmTime;
            pCurTime = &timeInfo.Time;
            if(g_recorder_status.alarmStat.alarmed && 
                    (get_time_stamp_in_secs(pLastTime) + 10 >= get_time_stamp_in_secs(pCurTime)))  // the lasttime alarm is valid;
            {
                return RECORDING;
            }
            else {
                if(stop_rec(ptask, handler_list) < 0) {    
                    LOGE("stop_rec failed");
                    return ERROR;
                }
                else {
                    // stop record success
                    return NO_RECORD;
                }
            }
        }
    }
#else
    int curTimeStamp;

    switch(g_recorder_status.recType.type) {
        case RECORDER_COLSED:
        #ifdef RECORDER_DEBUG
            LOGI("check stop ,close recorder");
        #endif
            if(stop_rec(ptask, handler_list) < 0)  // stop record success;
            {
                LOGE("stop_rec failed");
                ret = ERROR;
            }
            else {
                ret = NO_RECORD;
            }
            break;
        case RECORDER_COMMON:
        #ifdef RECORDER_DEBUG
            LOGI("check stop ,common recorder");
        #endif
            curTimeStamp = get_time_stamp_in_secs(&timeInfo.Time);
            // if(curTimeStamp < get_time_stamp_in_secs(&g_recorder_status.recType.recTime_s) || 
            //         curTimeStamp > get_time_stamp_in_secs(&g_recorder_status.recType.recTime_e))
            if (0 != check_record_time(&timeInfo))
            {
                // 不在录像时间段内
                if(stop_rec(ptask, handler_list) < 0) {    
                    LOGE("stop_rec failed");
                    ret = ERROR;
                }
                else {
                    // start record success
                    ret = RECORDING;
                }
            }
            else {
                // 在录像时间段内
                ret = RECORDING;
            }
            break;
        case RECORDER_ALARM:
        #ifdef RECORDER_DEBUG
            LOGI("check stop ,alarm recorder");
        #endif
            pLastTime = &g_recorder_status.alarmStat.lastAlarmTime;
            pCurTime = &timeInfo.Time;
            if (0 != check_record_time(&timeInfo))
            {
                // 不在录像时间段内
                if(stop_rec(ptask, handler_list) < 0) {    
                    LOGE("stop_rec failed");
                    return ERROR;
                }
                else {
                    // stop record success
                    return NO_RECORD;
                }                
            }
            else
            {
                // 在录像时间段内
                if(g_recorder_status.alarmStat.alarmed && 
                        (get_time_stamp_in_secs(pLastTime)+20 >= get_time_stamp_in_secs(pCurTime)))  // the lasttime alarm is valid, need start record
                {
                    ret = RECORDING;
                }
                else {
                    if(stop_rec(ptask, handler_list) < 0) {    
                        LOGE("stop_rec failed");
                        return ERROR;
                    }
                    else {
                        // stop record success
                        return NO_RECORD;
                    }
                }
            }
            break;
        default:
            ret = NO_RECORD;
            break;
    }
#endif
    return ret;
}

static int on_update_clock_timeout(THR_TASK_t *ptask, EVENT_HANDLE_LIST_t* handler_list)
{
    update_clocktime();
    return 0;
}

static int on_recorder_handler_msg(THR_TASK_t *ptask, MQ_LIST_t *pmq)
{
    int32_t msg_type = pmq->msg_node.type;
    static unsigned int cnt = 0;
    if(msg_type == LOCAL_MSG_ALARM_DETECT_E)
    {
        IPCNetTimeInfo_st timeInfo;
        get_local_time(g_timeZone, &timeInfo);

        IPCNetAlarmMsgReport_st *alarm_msg = (IPCNetAlarmMsgReport_st *)pmq->msg_node.msg;
		LOGI("LOCAL_MSG_ALARM_DETECT_E");
		if(alarm_msg->bStatus)
		{
            cnt++;
            mutex_lock(&g_recorder_status.sLock);
        	g_recorder_status.alarmStat.alarmed = 1;
            g_recorder_status.alarmStat.lastAlarmTime.Hour = timeInfo.Time.Hour;
            g_recorder_status.alarmStat.lastAlarmTime.Min = timeInfo.Time.Min;
            g_recorder_status.alarmStat.lastAlarmTime.Sec = timeInfo.Time.Sec;
            mutex_unlock(&g_recorder_status.sLock);
        }
        else 
        {
            int state = 0;
            if(cnt > 0)
            {
                // 延长报警标志时间
                cnt = 0;
                state = 1;
            }
            mutex_lock(&g_recorder_status.sLock);
            g_recorder_status.alarmStat.alarmed = state?(state|g_recorder_status.alarmStat.alarmed):0;
            mutex_unlock(&g_recorder_status.sLock);
        }
    }
    else if(msg_type == LOCAL_MSG_STOP_RECORDER)
    {
        EVENT_HANDLE_LIST_t *handler_list;
		LOGI("AV_RECORDER_MSG_STOP_RECORDER");
		handler_list = get_event_handler(ptask, g_recorder_eventid);
		if(handler_list)
		{
            LOGI("msg_type == LOCAL_MSG_STOP_RECORDER; stop recording");
        	stop_rec(ptask, handler_list);
        }
    }
    // else if(msg_type == LOCAL_MSG_RECORDER_ON_OFF)
    // {
    //     check_start_alarm_record(ptask);
    // }
    // else if(msg_type == LOCAL_MSG_RECORD_START)
    // {
    //     IPCNET_RECORD_OPT_t *opt = (IPCNET_RECORD_OPT_t*)pmq->msg_node.msg;
    //     if(!strncmp(opt->Opt, IPCNET_RECORD_OPT_START, sizeof(opt->Opt)))
    //     {
    //         // opt_start_record(ptask, opt->Duration);
    //         opt_start_record(ptask);
    //     }
    //     else if(!strncmp(opt->Opt, IPCNET_RECORD_OPT_END, sizeof(opt->Opt)))
    //     {
    //         opt_stop_record(ptask);
    //     }
    // }
    // else if(LOCAL_MSG_REC_CHANGE_TIMEZONE == msg_type)
    // {
    //     int diff_time = *(int*)pmq->msg_node.msg;
    //     LOGI("LOCAL_MSG_REC_CHANGE_TIME diff:%d", diff_time);
    //     // diff_gmt = diff_time;
    //     //get_local_time();
    // }
    
    return 0;
}

static int recorder_init(THR_TASK_t *ptask)
{
    EVENT_HANDLE_LIST_t handler_list;
    bool ret;

    update_clocktime();
    
    EVENT_HANDLER_LIST_INIT(handler_list,INVAILD_HANDLE_VALUE, EVENT_TIMEOUT);
    handler_list.event.timeout = on_recorder_timeout;
    handler_list.event.timeout_val = CHECK_RECORD_TASK_TIMESOUT;
    ret = register_event_handle(ptask,&handler_list);
    if(!ret)
    {
        LOGF("register_event_handle on_common_recorder_timeout fail");
        Logfi("register_event_handle on_common_recorder_timeout fail");
        exit(-1);
    }
    
    EVENT_HANDLER_LIST_INIT(handler_list,INVAILD_HANDLE_VALUE, EVENT_TIMEOUT);
    handler_list.event.timeout = on_update_clock_timeout;
    // handler_list.event.timeout_val = 100;
    handler_list.event.timeout_val = 1000;
    ret = register_event_handle(ptask,&handler_list);
    if(!ret)
    {
        LOGF("register_event_handle on_update_clock_timeout fail");
        Logfi("register_event_handle on_update_clock_timeout fail");
        exit(-1);
    }
    
    g_recorder_eventid = handler_list.event.event_id;
    LOGI("g_recorder_eventid=%d",g_recorder_eventid);
    
    register_msg_handler(ptask, on_recorder_handler_msg);
    return 0;
}


int regitster_recorder_task()
{
    // IPCNetTimeInfo_st time_cfg;
	IPCNetDiskInfo_st diskInfo;
    int ret;

    int vecCh = 0;

    // get_local_time(g_timeZone, &time_cfg);
    
    // for(int i = 0; i < MAX_VI_CHANNEL_NUM; i++)
    // {
    //     dev_get_record_cfg(i, &g_record_cfg[i]);
    // }
    
    // get disk info: {
	memset(&diskInfo, 0, sizeof(IPCNetDiskInfo_st));
	ret = ipcam_get_device_config(IPCNET_GET_DISK_CFG_REQ, &diskInfo);
	if(ret != IPCNET_RET_OK)
	{
        Logfi("IPCNET_GET_DISK_CFG_REQ FAIL err:%d", ret);
	    LOGE("IPCNET_GET_DISK_CFG_REQ FAIL err:%d", ret);
	    return FAIL;
	}
    Logfi("dev[%s]", diskInfo.Dev);
	g_disk_valid = diskInfo.isValid;
    // }
    
    // init g_recorder_status： {
    memset(&g_recorder_status, 0, sizeof(g_recorder_status));
    g_recorder_status.rec_fd = INVAILD_HANDLE_VALUE;
    g_recorder_status.enc_buf = my_malloc(20*1024);
    g_recorder_status.steam_shm_init = 0;
    // added by lwj
    mutex_init(&g_recorder_status.sLock);
    // end by lwj
    // added by lishun
    LOGI("*****************************************************************enter regitster_recorder_task");
    set_record_type(vecCh, &g_recorder_status.recType);
    // end by lishun

    if(!g_recorder_status.enc_buf)
    {
        LOGE("malloc fail");
        return FAIL;
    }
    g_recorder_status.buf_len = (20*1024);

    g_recorder_status.wr_buf = alloc_rdwr_buff(RECORD_NWRITE_LEN);
    if(!g_recorder_status.wr_buf)
    {
        LOGE("alloc_rdwr_buff fail");
        return FAIL;
    }
    init_rdwr_buff(g_recorder_status.wr_buf, 0);
    // }

    LOGI("register_task recorder_init");
    register_task(RECORD_TASK_ID, NULL, recorder_init, 0);
    return 0;
}

int load_rec_units_a_day(char *root_dir, MyDate *pDate, RECORDER_UNIT recUnits[]) {
    char idx_path[MAX_PATH];

    if(NULL == root_dir){
        return -1;
    }

    // load idx files;
    char *rec_dir = get_record_dir_by_date(root_dir, pDate, false);
    LOGI("rec_dir is %s", rec_dir);
    if(NULL == rec_dir) {
        return -1;
    }
    else {
        memset((void*)recUnits, 0, sizeof(RECORDER_UNIT)*UNITS_A_DAY);
        for(int i=0; i<24; i++) {
            for(int j=0; j<6; j++) {
                sprintf(idx_path, "%s/%02d-%02d-00.idx", rec_dir, i, j*10);
                // LOGI("idx_path is %s", idx_path);
                load_rec_units(idx_path, recUnits+(i*6+j)*60);
            }
        }
    }
    // added by lwj
    IPCNetTimeInfo_st timeInfo;
    get_local_time(g_timeZone, &timeInfo);
    if ((timeInfo.Date.Year == pDate->Year) && (timeInfo.Date.Mon == pDate->Mon) &&(timeInfo.Date.Day == pDate->Day))
    {
        MyTime tmpFileTime_s = g_recorder_status.aRecFileIdx.fileTime_s; 
        unsigned int fileNum= floor(tmpFileTime_s.Min/10.0f);
        memcpy(recUnits+(tmpFileTime_s.Hour*6+fileNum)*60, g_recorder_status.aRecFileIdx.recMinUnits, sizeof(RECORDER_UNIT)*MAX_UNITS);
    }
    // end by lwj

    return 0;
}

void get_a_day_time_by_unit_idx(int idx, MyTime *pTime) {     // ret: yyyymmss;
    pTime->Hour = floor(idx/360.0f);
    pTime->Min = floor((idx % 360) / 6.0f);
    pTime->Sec = (idx % 6) * 10;
}

int get_a_day_unit_idx_by_time(MyTime *pTime) {
    if(pTime != NULL) {
        return pTime->Hour*360 + pTime->Min*6 + floor(pTime->Sec / 10.0f);
    }
    else {
        return -1;
    }
}

int get_all_list_in_a_day(RECORDER_UNIT *recUnitsADay, MyDate *date_s, MyTime *time_s, MyTime *time_e, int retN, IPCNetAvRecFileInfo_t *recInfos) {
    int i, j = 0, n;
    bool bSet_s = false;
    bool bSet_e = false;

    int date = date_s->Year * 10000 + date_s->Mon * 100 + date_s->Day;
    i = time_s->Hour * 360 + time_s->Min * 6 + floor((time_s->Sec+1) / 10.0f);
    if(23 == time_e->Hour && 59 == time_e->Min && 59 == time_e->Sec) {  // end of a day;
        n = UNITS_A_DAY;
    }
    else {
        LOGE("time_e is not 23:59:59");
        return -1;
    }
    LOGI("[lwj test]---------------date[%d] i[%d]", date, i);
    int curType = -1;
    while(i < n) {
        if(0 != recUnitsADay[i].type) {
            if (curType != recUnitsADay[i].type)
            {
                // curType = recUnitsADay[i].type;
                if(!bSet_s && curType < 0) 
                {
                    MyTime time_s;
                    recInfos[j].t = DT_REG;
                    recInfos[j].ds = date;
                    get_a_day_time_by_unit_idx(i, &time_s);
                    // recInfos[j].st = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec + (recUnitsADay[i].alarmed>>4);
                    recInfos[j].st = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                    LOGI("recInfos.st: %d", recInfos[j].st);
                    bSet_s = true;
                    curType = recUnitsADay[i].type;
                } 
                else if(bSet_s && curType > 0) 
                {
                    MyTime time_s;
                    get_a_day_time_by_unit_idx(i, &time_s);
                    recInfos[j].e = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                    LOGI("recInfos.e: %d", recInfos[j].e);
                    bSet_e = true;
                    curType = recUnitsADay[i].type;
                }
            }
            
            #if 0
            if (0 == recUnitsADay[i].alarmed)
            {
                if(!bSet_s) {
                    MyTime time_s;
                    recInfos[j].t = DT_REG;
                    recInfos[j].ds = date;
                    get_a_day_time_by_unit_idx(i, &time_s);
                    recInfos[j].st = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                    LOGI("recInfos.st: %d", recInfos[j].st);
                    bSet_s = true;
                }
            }
            else
            {
                if(bSet_s && !bSet_e) {
                    MyTime time_s;
                    get_a_day_time_by_unit_idx(i, &time_s);
                    recInfos[j].e = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                    LOGI("recInfos.e: %d", recInfos[j].e);
                    bSet_e = true;
                }
            }
            #else
            #endif
        }
        else {
            if(bSet_s && !bSet_e) {
                MyTime time_s;
                get_a_day_time_by_unit_idx(i, &time_s);
                // LOGI("time_e - %02d:%02d:%02d", time_s.Hour, time_s.Min, time_s.Sec);
                recInfos[j].e = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                LOGI("recInfos.e: %d", recInfos[j].e);
                bSet_e = true;
                curType = -1;
            }
        }

        if(bSet_s && bSet_e) {
            bSet_s = false;
            bSet_e = false;
            j++;

            if(j >= retN) {         // get enough recInfos;
                break;
            }

            if (curType>0)
            {
                MyTime time_s;
                recInfos[j].t = DT_REG;
                recInfos[j].ds = date;
                get_a_day_time_by_unit_idx(i, &time_s);
                recInfos[j].st = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                LOGI("recInfos.st: %d", recInfos[j].st);
                bSet_s = true;
            }
        }
         
        i++;
    }

    if(bSet_s && !bSet_e) {
        recInfos[j].e = 235959;
        j++;
    }

    return j;
}

int get_record_list_in_a_day(RECORDER_UNIT *recUnitsADay, MyDate *date_s, MyTime *time_s, MyTime *time_e, int retN, IPCNetAvRecFileInfo_t *recInfos) {
    int i, j = 0, n;
    bool bSet_s = false;
    bool bSet_e = false;

    int date = date_s->Year * 10000 + date_s->Mon * 100 + date_s->Day;
    i = time_s->Hour * 360 + time_s->Min * 6 + floor((time_s->Sec+1) / 10.0f);
    if(23 == time_e->Hour && 59 == time_e->Min && 59 == time_e->Sec) {  // end of a day;
        n = UNITS_A_DAY;
    }
    else {
        LOGE("time_e is not 23:59:59");
        return -1;
    }
    LOGI("[lwj test]---------------date[%d] i[%d]", date, i);
    while(i < n) {
        if(0 != recUnitsADay[i].type) {
            #if 1
            if (0 == (recUnitsADay[i].alarmed & 0x0F))
            {
                if(!bSet_s) {
                    MyTime time_s;
                    recInfos[j].t = DT_REG;
                    recInfos[j].ds = date;
                    get_a_day_time_by_unit_idx(i, &time_s);
                    LOGI("Hour %d, min:%d, sec:%d, offset:%d", time_s.Hour,time_s.Min, time_s.Sec, (recUnitsADay[i].alarmed>>4)%10);
                    recInfos[j].st = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec + (recUnitsADay[i].alarmed>>4)%10;
                    LOGI("recInfos.st: %d", recInfos[j].st);
                    bSet_s = true;
                }
            }
            else
            {
                if(bSet_s && !bSet_e) {
                    MyTime time_s;
                    get_a_day_time_by_unit_idx(i, &time_s);
                    recInfos[j].e = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                    LOGI("recInfos.e: %d", recInfos[j].e);
                    bSet_e = true;
                }
            }
            #else
            // if(!bSet_s) {
            //     MyTime time_s;
            //     recInfos[j].t = DT_REG;
            //     recInfos[j].ds = date;
            //     get_a_day_time_by_unit_idx(i, &time_s);
            //     // LOGI("time_s - %02d:%02d:%02d", time_s.Hour, time_s.Min, time_s.Sec);
            //     recInfos[j].st = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
            //     LOGI("recInfos.st: %d", recInfos[j].st);
            //     bSet_s = true;
            // }
            #endif
        }
        else {
            if(bSet_s && !bSet_e) {
                MyTime time_s;
                get_a_day_time_by_unit_idx(i, &time_s);
                // LOGI("time_e - %02d:%02d:%02d", time_s.Hour, time_s.Min, time_s.Sec);
                recInfos[j].e = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                LOGI("recInfos.e: %d", recInfos[j].e);
                bSet_e = true;
            }
        }

        if(bSet_s && bSet_e) {
            bSet_s = false;
            bSet_e = false;
            j++;

            if(j >= retN) {         // get enough recInfos;
                break;
            }
        }
         
        i++;
    }

    if(bSet_s && !bSet_e) {
        recInfos[j].e = 235959;
        j++;
    }

    return j;
}

int get_alarm_list_in_a_day(RECORDER_UNIT *recUnitsADay, MyDate *date_s, MyTime *time_s, MyTime *time_e, int retN, IPCNetAvRecFileInfo_t *recInfos) {
    int i, j = 0, n;
    bool bSet_s = false;
    bool bSet_e = false;

    int date = date_s->Year * 10000 + date_s->Mon * 100 + date_s->Day;
    i = time_s->Hour * 360 + time_s->Min * 6 + floor((time_s->Sec+1) / 10.0f);
    if(23 == time_e->Hour && 59 == time_e->Min && 59 == time_e->Sec) {  // end of a day;
        n = UNITS_A_DAY;
    }
    else {
        LOGE("time_e is not 23:59:59");
        return -1;
    }

    while(i < n) {
        if(0 != recUnitsADay[i].type) {
            if (0 != (recUnitsADay[i].alarmed & 0x0F))
            {
                if(!bSet_s) {
                    MyTime time_s;
                    recInfos[j].t = DT_REG;
                    recInfos[j].ds = date;
                    get_a_day_time_by_unit_idx(i, &time_s);
                    recInfos[j].st = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec + (recUnitsADay[i].alarmed>>4)%10;
                    LOGI("alarmed: 0x%x", recUnitsADay[i].alarmed);
                    LOGI("recInfos.st: %d", recInfos[j].st);
                    bSet_s = true;
                }
            }
            else
            {
                if(bSet_s && !bSet_e) {
                    MyTime time_s;
                    get_a_day_time_by_unit_idx(i, &time_s);
                    recInfos[j].e = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                    LOGI("recInfos.e: %d", recInfos[j].e);
                    bSet_e = true;
                }
            }
        }
        else {
            if(bSet_s && !bSet_e) {
                MyTime time_s;
                get_a_day_time_by_unit_idx(i, &time_s);
                recInfos[j].e = time_s.Hour*10000 + time_s.Min*100 + time_s.Sec;
                LOGI("recInfos.e: %d", recInfos[j].e);
                bSet_e = true;
            }
        }

        if(bSet_s && bSet_e) {
            bSet_s = false;
            bSet_e = false;
            j++;

            if(j >= retN) {         // get enough recInfos;
                break;
            }
        }
         
        i++;
    }

    if(bSet_s && !bSet_e) {
        recInfos[j].e = 235959;
    }

    return j;
}

void int_to_time(int time_i, MyTime *time_s) {
    time_s->Hour = floor(time_i / 10000.0f);
    time_s->Min = floor((time_i % 10000) / 100.0f);
    time_s->Sec = floor(time_i % 100);
    return time_s;
}

int time_to_int(MyTime *time_s) {
    return time_s->Hour*10000 + time_s->Min*100 + time_s->Sec;
}

void int_to_date(int date_i, MyDate *date_s) {
    date_s->Year = floor(date_i / 10000.0f);
    date_s->Mon = floor((date_i % 10000) / 100.0f);
    date_s->Day = floor(date_i % 100);
    return date_s;
}

int date_to_int(MyDate *date_s) {
    return date_s->Year*10000 + date_s->Mon*100 + date_s->Day;
}

int get_offset_by_time(MyTime *pTime, RECORDER_UNIT *recUnitsInADay) {      // ret: -1 - not found, > 0 - the value of offset
    int idx = get_a_day_unit_idx_by_time(pTime);
    return recUnitsInADay[idx].offset;
}

void get_file_time_by_time(MyTime *pTime, MyTime *pTime_s) {
    MyTime time_t;
    if(NULL == pTime) {
        IPCNetTimeInfo_st timeInfo;
        get_local_time(g_timeZone, &timeInfo);
        time_t.Hour = timeInfo.Time.Hour;
        time_t.Min = timeInfo.Time.Min;
        time_t.Sec = timeInfo.Time.Sec;
    }
    else {
        time_t.Hour = pTime->Hour;
        time_t.Min = pTime->Min;
        time_t.Sec = pTime->Sec;
    }

    pTime_s->Hour = time_t.Hour;
    pTime_s->Min = time_t.Min - time_t.Min % 10;
    pTime_s->Sec = 0;
}

int get_next_file_time_by_time(MyTime *pTime, MyTime *pTime_s)  // ret: 0-current day, 1-next day, -1-error;
{
    get_file_time_by_time(pTime, pTime_s);
    pTime_s->Sec = 0;
    pTime_s->Min += 10;
    if(pTime_s->Min >= 60) {    // next hour
        pTime_s->Min = 0;
        pTime_s->Hour += 1;
        if(pTime_s->Hour >= 24) {   // next day
            pTime_s->Hour = 0;
            return 1;
        }
    }
    return 0;
}

char *get_rec_root() {
    if(!g_disk_valid) {
        LOGD("g_disk_valid is[%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
        return NULL;
    }

    static char rootDir[MAX_PATH];
    sprintf(rootDir, "%s", DEFAULT_COMMOM_REC_PATH);

    // IPCNetRecordGetCfg_st record_req;
    // IPCNetRecordCfg_st record_cfg;

    // memset(&record_req, 0, sizeof(record_req));
    // record_req.ViCh = 0;
    // record_req.RecType = 0; //change by lwj
    // if(!g_disk_valid)
    // {
    //     LOGD("g_disk_valid is[%s],NO MMC CARD in device", g_disk_valid ? "TRUE" : "FALSE");
    //     return NULL;
    // }
    // get_recorder_cfg(&record_req, &record_cfg);
    // strcpy(rootDir, record_cfg.DiskInfo.Path);
    

    return rootDir;
}

void set_record_type( int vencCh, RECORDER_TYPE *recType) {
    DEV_RECORD_CFG_t recCfg;
    dev_get_record_cfg(vencCh, &recCfg);

#ifdef RECORDER_DEBUG
    for ( int i = 0; i < (int)(sizeof(recCfg.record_time_policy_list)/sizeof(DEV_DEFEND_TIME_LIST_INFO_t)); i++)
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
    mutex_lock(&g_recorder_status.sLock);
    if(0 == recCfg.enable) {
        recType->type = RECORDER_COLSED;
    }
    else {
        if ((0 == recCfg.recMode))// common record;
        {
            recType->type = RECORDER_COMMON;
        }
        else// alarm record;
        {
            recType->type = RECORDER_ALARM;
        }
        
        for ( int i = 0; i < (int)(sizeof(recCfg.record_time_policy_list)/sizeof(DEV_DEFEND_TIME_LIST_INFO_t)); i++)
        {
            recType->recTimeList[i].en = recCfg.record_time_policy_list[i].en;
            for ( int j = 1; j < (int)(sizeof(recCfg.record_time_policy_list[i].record_time_policy)/sizeof(DEV_DEFEND_TIME_INFO_t)); j++)
            {
                recType->recTimeList[i].timeInfo[j-1].en = recCfg.record_time_policy_list[i].record_time_policy[j].en;
                recType->recTimeList[i].timeInfo[j-1].recTime_s.Hour = recCfg.record_time_policy_list[i].record_time_policy[j].start1.hour;
                recType->recTimeList[i].timeInfo[j-1].recTime_s.Min = recCfg.record_time_policy_list[i].record_time_policy[j].start1.min;
                recType->recTimeList[i].timeInfo[j-1].recTime_s.Sec = recCfg.record_time_policy_list[i].record_time_policy[j].start1.sec;

                recType->recTimeList[i].timeInfo[j-1].recTime_e.Hour = recCfg.record_time_policy_list[i].record_time_policy[j].end1.hour;
                recType->recTimeList[i].timeInfo[j-1].recTime_e.Min = recCfg.record_time_policy_list[i].record_time_policy[j].end1.min;
                recType->recTimeList[i].timeInfo[j-1].recTime_e.Sec = recCfg.record_time_policy_list[i].record_time_policy[j].end1.sec;
                // LOGI("type:%d, list:%d, lisEn:%d, dayEn:%d, %02d:%02d:%02d - %02d:%02d:%02d", g_recorder_status.recType.type, i, 
                //                                                             g_recorder_status.recType.recTimeList[i].en,
                //                                                             g_recorder_status.recType.recTimeList[i].timeInfo[j].en,
                //                                                             g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_s.Hour,
                //                                                             g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_s.Min,
                //                                                             g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_s.Sec,
                //                                                             g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_e.Hour,
                //                                                             g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_e.Min,
                //                                                             g_recorder_status.recType.recTimeList[i].timeInfo[j].recTime_e.Sec);
            }
        }
    }
    mutex_unlock(&g_recorder_status.sLock);

}