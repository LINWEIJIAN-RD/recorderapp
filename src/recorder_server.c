/**
* created by yys(Vincent.Yeh)
*/
#include <stdio.h>
#include "os_ini.h"
#include "mempool.h"
#include "msg_queue.h"
#include "event_handle.h"
#include "task_mgr.h"
#include "frame_buff.h"
#include "logServer.h"
#include "streamshmmgr.h"
#include "ipcnet_struct.h"
#include "ipcnet_msg_declare.h"
#include "sepcam_ipc_server.h"
#include "client_mgr.h"
#include "attach_rdwr_buff.h"
#include "sepcam_recorder.h"
#include "sepcam_ipc_internal.h"
#include "disk_mgr.h"
#include "recorder_params.h"
#include "sepcam_ipc_api.h"
#include "av_packet_ps.h"
#include "file_list.h"
#include "atomic.h"

#include "recorder_defs.h"

// added by lishun
#include "recorder_utils.h"
// RECORDER_UNIT g_rec_units_a_day[UNITS_A_DAY];
SEPCAM_PLAYBACK_STATUS_s g_playback_status;

extern SEPCAM_RECORDER_STATUS_s    g_recorder_status;
// end by lishun

//add by lwj 
VideoFileInfo_t gVideoFileInfo;
mutex_t videoFileInfoLock;
extern IPCNETAlarmRecordList_st g_alarmRecordList;
int gSDCardStatus = 0;
int g_recorder_log_flag = 0;
extern int g_recorder_log_flag;
//add end by lwj 
#define AV_RECORDER_SERV_NUM 3
static AV_RECORDER_SERVER_t gAVRecorderServ[AV_RECORDER_SERV_NUM];

AV_RECORDER_SERVER_t *get_av_recorder_serv(int recorder_id)
{
    if(recorder_id >= AV_RECORDER_SERV_NUM)
    {
        LOGE("recorder_id:%d is >= AV_RECORDER_SERV_NUM[%d]", recorder_id, AV_RECORDER_SERV_NUM);
        return NULL;
    }

    return &gAVRecorderServ[recorder_id];
}

// add by lwj
static void initVideoFileInfo()
{
    memset(&gVideoFileInfo, 0, sizeof(VideoFileInfo_t));
    mutex_init(&videoFileInfoLock);
}
// add end by lwj

static void init_av_recorder_serv()
{
    int i;
    for(i = 0; i < AV_RECORDER_SERV_NUM; i++)
    {
        memset(&gAVRecorderServ[i], 0, sizeof(AV_RECORDER_SERVER_t));
        mutex_init(&gAVRecorderServ[i].lock);
    }
}

static int create_av_recorder_shm(int i, char *stream_shm, int block)
{
    int ret;
    
    sprintf(stream_shm, "recorder_%d", i);
    if(block)
    {
        ret = create_block_ipcam_server_streamshm(stream_shm, 500*1024);
    }
    else
    {
        ret = create_ipcam_server_streamshm(stream_shm, 500*1024);
    }
    if(ret != 0)
    {
        LOGE("create_ipcam_server_streamshm:%s create FAIL", stream_shm);
        return FAIL;
    }
    return 0;
}

static int find_av_recorder_serv_valid(char *stream_shm, int block)
{
    int i;
    int ret = FAIL;

    stream_shm[0] = '\0';
    
    for(i = 0; i < AV_RECORDER_SERV_NUM; i++)
    {
        mutex_lock(&gAVRecorderServ[i].lock);
        
        if(!gAVRecorderServ[i].flag)
        {
            LOGI("found recorder serv index:%d", i);
            ret = create_av_recorder_shm(i, gAVRecorderServ[i].stream_shm, block);
            if(ret == 0)
            {
                gAVRecorderServ[i].flag = 1;
                strcpy(stream_shm, gAVRecorderServ[i].stream_shm);
                mutex_unlock(&gAVRecorderServ[i].lock);
                
                LOGI("index:%d create_av_recorder_shm:%s SUCCESS", i, gAVRecorderServ[i].stream_shm);
                ret = i;
                break;
            }
        }
        
        mutex_unlock(&gAVRecorderServ[i].lock);
    }

    return ret;
}


static int IPCMsgSetRecordCfg(void *out,IPCNetRecordCfg_st*pcfg);
int IPCMsgGetRecordCfg(IPCNetRecordCfg_st *pRecordCfg, IPCNetRecordGetCfg_st *pRecordGetCfg);
int IPCMsgGetAVRecoListInfoCfg(IPCNetAvRecordInfoResp_st *pAvRecordInfoResp, IPCNetAvRecordInfoReq_st *pAvRecordInfoReq);
int IPCMsgGetAVRecoListPageCfg(IPCNetAvRecListPageResp_t *pAvRecListPageResp, IPCNetAvRecListPageReq_t *AvRecListPageReq);
// int IPCServer_PlayRecordReq(IPCNET_RECORD_RESP_t *pRecordResp, IPCNET_RECORD_REQ_t *pRecordReq);
int IPCServer_PlayRecordReq(struct THR_TASK *ptask,uint32_t event_id,IN uint32_t session,IN char *in,IN size_t inlen);
int IPCMsgGetRecoDateByMonth(IPCNetRet_st *pRecordDateResp, IPCNetRecoMonth_st *pRecordReq);
// static int IPCMsgGetAVRecoListInfoCfg(THR_TASK_t *ptask,EVENT_HANDLE_LIST_t *handler_list,IN uint32_t session,IN char *in,IN size_t inLen,IN char *out,INOUT size_t *outLen);
// static int IPCMsgGetAVRecoListPageCfg(THR_TASK_t *ptask,EVENT_HANDLE_LIST_t *handler_list,IN uint32_t session,IN char *in,IN size_t inLen,IN char *out,INOUT size_t *outLen);
// static int IPCServer_PlayRecordReq(THR_TASK_t *ptask,EVENT_HANDLE_LIST_t *handler_list,IN uint32_t session,IN char *in,IN size_t inLen,IN char *out,INOUT size_t *outLen);
static int IPCServer_StartRecordReq(void *out, IPCNET_RECORD_OPT_t *opt);

void UtcToLocalTime(IPCNET_RECORD_REQ_t *playrecord_req_p, int *iDate, int *iTime);

IPCServerMsgProcessListItem_st gRecorderIPCMsgProcessList[] = 
{
	IPCNET_SERV_PROC_DECLARE(IPCNET_AV_RECO_CONF_SET_REQ, IPCNetRecordCfg_st,IPCNetNULL_st,IPCMsgSetRecordCfg),
	IPCNET_SERV_PROC_DECLARE(IPCNET_AV_RECO_CONF_GET_REQ, IPCNetRecordGetCfg_st, IPCNetRecordCfg_st, IPCMsgGetRecordCfg),
	

    IPCNET_SERV_PROC_DECLARE(IPCNET_AV_RECO_LIST_GET_REQ, IPCNetAvRecordInfoReq_st, IPCNetAvRecordInfoResp_st, IPCMsgGetAVRecoListInfoCfg),
    IPCNET_SERV_PROC_DECLARE(IPCNET_AV_RECO_LIST_PAGE_GET_REQ, IPCNetAvRecListPageReq_t, IPCNetAvRecListPageResp_t, IPCMsgGetAVRecoListPageCfg),
    IPCNET_SERV_PROC_DECLARE(IPCNET_GET_RECO_DATE_BY_MONTH_REQ,  IPCNetRecoMonth_st, IPCNetRet_st, IPCMsgGetRecoDateByMonth),
    // IPCNET_SERV_PROC_DECLARE(IPCNET_PLAY_RECORD_REQ,  IPCNET_RECORD_REQ_t, IPCNET_RECORD_RESP_t, IPCServer_PlayRecordReq),


    IPCNET_SERV_PROC_DECLARE(IPCNET_ALARM_REPORT_RESP, IPCNetAlarmMsgReport_st, IPCNetAlarmMsgReport_st, NULL),

	// IPCNET_RAW_SERV_PROC_DECLARE(IPCNET_AV_RECO_LIST_GET_REQ, IPCMsgGetAVRecoListInfoCfg),
	// IPCNET_RAW_SERV_PROC_DECLARE(IPCNET_AV_RECO_LIST_PAGE_GET_REQ, IPCMsgGetAVRecoListPageCfg),
    IPCNET_RAW_SERV_PROC_DECLARE(IPCNET_PLAY_RECORD_REQ, IPCServer_PlayRecordReq),

    IPCNET_SERV_PROC_DECLARE(IPCNET_RECORD_OPT_REQ, IPCNET_RECORD_OPT_t, IPCNetNULL_st, IPCServer_StartRecordReq),
    
    SEPCAM_IPC_MSG_PROCESS_END,
};

void broadcast_event()
{
    IPCNetAlarmMsgReport_st infoReport;
    infoReport.AlarmType = IPCNET_RECORDER_CLEAN;
    // strncpy(infoReport.Des, strinfo, sizeof(infoReport.Des));
    sepcam_ipc_server_broadcast(IPCNET_ALARM_REPORT_RESP, &infoReport, sizeof(IPCNetAlarmMsgReport_st));
}

void parse_string_time(IPCNetTimeSimple_t*it, char*str)
{
	char digstr[32];
	int i,j;
	memset(digstr,0,32);
	for(i=0,j=0;i<32&&str[i];i++){
		if(str[i]==':'){
			strncpy(digstr,str,i);
			it->h = atoi(digstr);
			j=i+1;
			break;
		}
	}
	memset(digstr,0,32);
	for(i=j;i<32&&str[i];i++){
		if(str[i]==':'){
			strncpy(digstr,&str[j],i-j);
			it->m = atoi(digstr);
			j=i+1;
			break;
		}
	}
	memset(digstr,0,32);
	//for(i=j;i<32&&str[i];i++){
	//	if(str[i]==':'){
			//strncpy(digstr,&str[j],i-j);
			strncpy(digstr,&str[j],2);
			it->s = atoi(digstr);
			//printf("s:%s j[%d]\n",digstr,j);
	//		break;
	//	}
	//}

	//printf("------------->[%s] h[%d]m[%d]s[%d]\n",str,it->h,it->m,it->s);
}

static int IPCMsgSetRecordCfg(void *out,IPCNetRecordCfg_st*pcfg)
{
	int ret = IPCNET_RET_OK;
	int i;
	int j;
	DEV_RECORD_CFG_t rec_cfg;
	int min = 0, max=0, RecMins=0;

	if(dev_get_record_cfg(pcfg->ViCh, &rec_cfg) != SUCCESS){
		return IPCNET_RET_REQ_ILLEGAL;
	}
	
	LOGI("ViCh=%d VeCh=%d AutoDel:%d RecMins:%d",
	    pcfg->ViCh, pcfg->VeCh,
	    pcfg->AutoDel, pcfg->RecMins);
	rec_cfg.vich = (T_U16)pcfg->ViCh;
	rec_cfg.vech = (T_U16)pcfg->VeCh;
	if(pcfg->AutoDel == FALSE)
	{        
        rec_cfg.auto_del = 0;
	}
	else
	{
        rec_cfg.auto_del = 1;
	}
    rec_cfg.rec_time = pcfg->RecMins;
	
    for(i = 0; i < (int)(sizeof(rec_cfg.record_time_policy_list)/sizeof(DEV_DEFEND_TIME_LIST_INFO_t)); i++)
    {
        DEV_DEFEND_TIME_LIST_INFO_t *ddtli= &rec_cfg.record_time_policy_list[i];
        IPCNetRecordTimeList_st *inrtl = &pcfg->RecTimeList[i];
        ddtli->en = inrtl->En;
        for(j = 0; j < (int)(sizeof(rec_cfg.record_time_policy_list[i].record_time_policy)/sizeof(DEV_DEFEND_TIME_INFO_t)); j++)
        {
            ddtli->record_time_policy[j].en = inrtl->RecTime[j].En;

            IPCNetTimeSimple_t intime;
            parse_string_time(&intime,inrtl->RecTime[j].St1);
            ddtli->record_time_policy[j].start1.hour = intime.h;
            ddtli->record_time_policy[j].start1.min= intime.m;
            ddtli->record_time_policy[j].start1.sec= intime.s;
            memset(&intime, 0, sizeof(IPCNetTimeSimple_t));
            parse_string_time(&intime,inrtl->RecTime[j].St2);
            ddtli->record_time_policy[j].start2.hour = intime.h;
            ddtli->record_time_policy[j].start2.min= intime.m;
            ddtli->record_time_policy[j].start2.sec= intime.s;
            memset(&intime, 0, sizeof(IPCNetTimeSimple_t));
            parse_string_time(&intime,inrtl->RecTime[j].Ed1);
            ddtli->record_time_policy[j].end1.hour = intime.h;
            ddtli->record_time_policy[j].end1.min= intime.m;
            ddtli->record_time_policy[j].end1.sec= intime.s;
            memset(&intime, 0, sizeof(IPCNetTimeSimple_t));
            parse_string_time(&intime,inrtl->RecTime[j].Ed2);
            ddtli->record_time_policy[j].end2.hour = intime.h;
            ddtli->record_time_policy[j].end2.min= intime.m;
            ddtli->record_time_policy[j].end2.sec= intime.s;
        }
    }

	// for(i = 0; i < 8; i++)
	// {
	// 	DEV_DEFEND_TIME_INFO_t *ddti= &rec_cfg.record_time_policy[i];
	// 	IPCNetRecordTiming_st *inrt = &pcfg->RecTime[i];

	// 	ddti->flag = inrt->En;

	// 	IPCNetTimeSimple_t intime;
	// 	parse_string_time(&intime,inrt->St1);
	// 	ddti->start1.hour = intime.h;
	// 	ddti->start1.min= intime.m;
	// 	ddti->start1.sec= intime.s;
	// 	parse_string_time(&intime,inrt->St2);
	// 	ddti->start2.hour = intime.h;
	// 	ddti->start2.min= intime.m;
	// 	ddti->start2.sec= intime.s;
	// 	parse_string_time(&intime,inrt->Ed1);
	// 	ddti->end1.hour = intime.h;
	// 	ddti->end1.min= intime.m;
	// 	ddti->end1.sec= intime.s;
	// 	parse_string_time(&intime,inrt->Ed2);
	// 	ddti->end2.hour = intime.h;
	// 	ddti->end2.min= intime.m;
	// 	ddti->end2.sec= intime.s;
	// }
	
    rec_cfg.enable = pcfg->enable;
    rec_cfg.recMode = pcfg->recMode;

	if(dev_set_record_cfg(&rec_cfg)!=SUCCESS){
		ret = IPCNET_RET_REQ_ILLEGAL;
	}
    else {
        // set record cfg success
        set_record_type(0, &g_recorder_status.recType);
        
#ifdef RECORDER_DEBUG
    for ( i = 0; i < (int)(sizeof(rec_cfg.record_time_policy_list)/sizeof(DEV_DEFEND_TIME_LIST_INFO_t)); i++)
    {
        for ( j = 0; j < (int)(sizeof(g_recorder_status.recType.recTimeList[i].timeInfo)/sizeof(TIME_INFO)); j++)
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

#if RECORDER_LOG
    if (g_recorder_log_flag)
    {
        Logfi("recorder set en[%d] mode[%d]", rec_cfg.enable, rec_cfg.recMode);
    }
#endif

    }
	return ret;
}
    
int IPCMsgGetRecordCfg(IPCNetRecordCfg_st *pRecordCfg, IPCNetRecordGetCfg_st *pRecordGetCfg)
{
    DEV_RECORD_CFG_t rec_cfg;
	int ret = IPCNET_RET_OK;
	int day;
	int sensor_index;
	IPCNetDiskInfo_st *diskInfo;
	int i, j=0;
	int min = 0, max = 0;

	sensor_index = pRecordGetCfg->ViCh;

	pRecordCfg->ReserveSize = MIN_RESERVED_SIZE;
	//pRecordCfg->RecycleMode = recorder->recycle_mode;
	//pRecordCfg->Duration = recorder->length;
	//pRecordCfg->PackageType = recorder->avi.package_type;
	pRecordCfg->Mode = 0;

	diskInfo = &pRecordCfg->DiskInfo;
	memset(diskInfo, 0, sizeof(IPCNetDiskInfo_st));
	ret = ipcam_get_device_config(IPCNET_GET_DISK_CFG_REQ, diskInfo);
	if(ret != SUCCESS)
	{
        LOGI("lwj test ipcam_get_device_config fail-++++++++++-");
	    return ret;
	}

	if(1 == pRecordGetCfg->RecType)
	{
	    strncpy(diskInfo->Path, get_alarm_record_path(), sizeof(diskInfo->Path));
	}
	else
	{
	    strncpy(diskInfo->Path, get_common_record_path(), sizeof(diskInfo->Path));
	}
	
	// LOGD("diskInfo->Path=%s",diskInfo->Path);
    dev_get_record_cfg(sensor_index, &rec_cfg);
    pRecordCfg->ViCh = sensor_index;
    pRecordCfg->VeCh = rec_cfg.vech;
    pRecordCfg->AutoDel = rec_cfg.auto_del;

    pRecordCfg->__pri_RecMinsOptionCount = rec_cfg.pkt_tm_opt_num;
    pRecordCfg->RecMins = rec_cfg.rec_time;
    // LOGI("__pri_RecMinsOptionCount:%d RecMins:%d", pRecordCfg->__pri_RecMinsOptionCount, pRecordCfg->RecMins);
    for(i = 0; i < pRecordCfg->__pri_RecMinsOptionCount; i++)
    {
        pRecordCfg->RecMinsOption[i] = (T_S32)rec_cfg.pkt_tm_options[i];
        // LOGI("RecMinsOption[%d]:%d", i, pRecordCfg->RecMinsOption[i]);
    }
    
    for(i = 0; i < (int)(sizeof(rec_cfg.record_time_policy_list)/sizeof(DEV_DEFEND_TIME_LIST_INFO_t)); i++)
    {
        pRecordCfg->RecTimeList[i].En = rec_cfg.record_time_policy_list[i].en;
        for(day = 0; day < (int)(sizeof(rec_cfg.record_time_policy_list[i].record_time_policy)/sizeof(DEV_DEFEND_TIME_INFO_t)); day++)
        {
            pRecordCfg->RecTimeList[i].RecTime[day].En = rec_cfg.record_time_policy_list[i].record_time_policy[day].en;
            sprintf(pRecordCfg->RecTimeList[i].RecTime[day].St1, "%02u:%02u:%02u",
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].start1.hour,
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].start1.min,
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].start1.sec);
            sprintf(pRecordCfg->RecTimeList[i].RecTime[day].Ed1, "%02u:%02u:%02u",
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].end1.hour,
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].end1.min,
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].end1.sec);
            sprintf(pRecordCfg->RecTimeList[i].RecTime[day].St2, "%02u:%02u:%02u",
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].start2.hour,
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].start2.min,
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].start2.sec);
            sprintf(pRecordCfg->RecTimeList[i].RecTime[day].Ed2, "%02u:%02u:%02u",
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].end2.hour,
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].end2.min,
                    rec_cfg.record_time_policy_list[i].record_time_policy[day].end2.sec);
        }
    }
    // for(day = 0; day < 8; day++)
    // {
    //     pRecordCfg->RecTime[day].En = rec_cfg.record_time_policy[day].flag;
    //     sprintf(pRecordCfg->RecTime[day].St1, "%02u:%02u:%02u",
    //             rec_cfg.record_time_policy[day].start1.hour,
    //             rec_cfg.record_time_policy[day].start1.min,
    //             rec_cfg.record_time_policy[day].start1.sec);
    //     sprintf(pRecordCfg->RecTime[day].Ed1, "%02u:%02u:%02u",
    //             rec_cfg.record_time_policy[day].end1.hour,
    //             rec_cfg.record_time_policy[day].end1.min,
    //             rec_cfg.record_time_policy[day].end1.sec);
    //     sprintf(pRecordCfg->RecTime[day].St2, "%02u:%02u:%02u",
    //             rec_cfg.record_time_policy[day].start2.hour,
    //             rec_cfg.record_time_policy[day].start2.min,
    //             rec_cfg.record_time_policy[day].start2.sec);
    //     sprintf(pRecordCfg->RecTime[day].Ed2, "%02u:%02u:%02u",
    //             rec_cfg.record_time_policy[day].end2.hour,
    //             rec_cfg.record_time_policy[day].end2.min,
    //             rec_cfg.record_time_policy[day].end2.sec);
    // }
    pRecordCfg->enable = rec_cfg.enable;
    pRecordCfg->recMode = rec_cfg.recMode;
    pRecordCfg->sdCardStatus = gSDCardStatus;
	return ret;
}

static int IPCServer_StartRecordReq(void *out, IPCNET_RECORD_OPT_t *opt)
{
    LOGI("IPCServer_StartRecordReq opt%s Duration:%d", opt->Opt, opt->Duration);
    notify_recorder_task(0, LOCAL_MSG_RECORD_START, opt, sizeof(IPCNET_RECORD_OPT_t));
    return 0;
}

static int cal_record_durtime(char *filename)
{
    char *pos;
    int yymm, dd, start_time, end_time;
    int start_secs, end_secs;
    
    return get_pes_file_durtime(filename);
    
    if((pos = strstr(filename, get_common_record_path())))
    {
        pos += strlen(get_common_record_path());
    }
    else if((pos = strstr(filename, get_alarm_record_path())))
    {
        pos += strlen(get_alarm_record_path());
    }
    else
    {
        return 0;
    }

    sscanf(pos, "%d/%d/%d-%d.", &yymm, &dd, &start_time, &end_time);
    LOGI("pos:%s %d-%d, %d-%d", pos, yymm, dd, start_time, end_time);
    
    if(end_time < start_time)
    //跨天丿
    {
        end_time += 240000;
    }

    start_secs = start_time/10000*60*60 + (start_time%10000)/100*60 + start_time%100;
    end_secs = end_time/10000*60*60 + (end_time%10000)/100*60 + end_time%100;
    LOGI("start_time: %d:%d:%d %d, %d, %d",
        start_time/10000, (start_time%10000)/100, start_time%100,
        start_time/10000*60*60, (start_time%10000)/100*60, start_time%100);
    LOGI("end_secs: %d:%d:%d %d, %d, %d",
        end_secs/10000, (end_secs%10000)/100, end_secs%100,
        end_secs/10000*60*60, (end_secs%10000)/100*60, end_secs%100);
    LOGI("dutime:%d start_secs:%d end_secs:%d", (end_secs - start_secs), start_secs, end_secs);
    return (end_secs - start_secs);
}

/*
REQ:
{"lp":{
    "p": path,  //usual 201709/28
    "s": start_index,
    "c": count,
    
    //新增 20170829
	"si": sensor_index, //默认0
    "re": recursive_num, //默认
    "m":  mode,//0:不包括文件夹 1：包括文件夹 //默认
}}
*/

static int timeOffset(int utcDayTime)
{
    int outTime = utcDayTime;
    outTime -= 8*10000;
    if (outTime<0)
    {
       outTime+=240000;
    }
    return outTime;
}

#if 0 

static void sortStruct(IPCNetAvRecFileInfo_t *a, int len)//a为数组地址，l为数组长度。
{
    int i, j;
    IPCNetAvRecFileInfo_t tmp;
    char startStr1[8];
    char startStr2[8];
    int cs1, cs2;
    for(i = 0; i < len - 1; i ++)
        for(j = i+1; j < len; j ++)
        {
            strncpy(startStr1,a[i].n,6);
            cs1 = atoi(startStr1);
            strncpy(startStr2,a[j].n,6);
            cs2 = atoi(startStr2);
            // printf("-------[lwj test]sortStruct cs1:%d cs2:%d\n",cs1, cs2);
            if(cs1 > cs2)//如前面的比后面的大，则交换。
            {
                
                printf("-------[lwj test]change\n");
                memcpy(&tmp, &a[i], sizeof(IPCNetAvRecFileInfo_t));
                memcpy(&a[i], &a[j], sizeof(IPCNetAvRecFileInfo_t));
                memcpy(&a[j], &tmp, sizeof(IPCNetAvRecFileInfo_t));
            }
        }
}

int mergeFileArray(VideoFileInfo_t *FileInfo, int size, int sStart, int eStart, IPCNetAvRecFileInfo_t *outInfo)
{
	int cnt = FileInfo->fileTotal;
    int startFile = cnt;
    int endFile = 0;
    if(cnt <= 0)
    {
        LOGI("fileTotal:[%d]",FileInfo->fileTotal);
        return -1;
    }
	// for (int i = 0; i < cnt; i++)
	// {
	// 	LOGI("i=%d [%d-%d] ",i, FileInfo->fileList[i].stTime, FileInfo->fileList[i].eTime);
	// }
    LOGI("cnt=%d size=%d startTime=%d endTime=%d\n", cnt, size, sStart, eStart);
	int j = 0;
	
    sprintf(&(outInfo[j].n[0]),"%06d", FileInfo->fileList[0].stTime);
    int lastVideoEndTiem = FileInfo->fileList[0].eTime;
    for (int i = 1; i < cnt; i++)
	{
        // if((sStart <= FileInfo->fileList[i].stTime && eStart >= FileInfo->fileList[i].stTime)
            // || (sStart <= FileInfo->fileList[i].eTime && eStart >= FileInfo->fileList[i].eTime))
        if((sStart <= FileInfo->fileList[i].stTime) && ( FileInfo->fileList[i].eTime<=eStart))
        {
            if((FileInfo->fileList[i].stTime - lastVideoEndTiem) > 1)
            {
                // printf("fileList[cur]:[%d-%d] lastVideo[%d-%d]\n", FileInfo->fileList[i].stTime, FileInfo->fileList[i].eTime,
                //         FileInfo->fileList[i-1].stTime, FileInfo->fileList[i-1].eTime);
                sprintf(&(outInfo[j].n[6]),"-%06d.pes",lastVideoEndTiem);
                j++;
                if (j >= size-1)
                {
                    break;
                }
                
                sprintf(&(outInfo[j].n[0]),"%06d", FileInfo->fileList[i].stTime);
            }
            lastVideoEndTiem = FileInfo->fileList[i].eTime;
        }
	}

    sprintf(&(outInfo[j].n[6]),"-%06d.pes",FileInfo->fileList[cnt-1].eTime);
	
    return j+1;
}

int IPCMsgGetAVRecoListPageCfg(IPCNetAvRecListPageResp_t *pAvRecListPageResp, IPCNetAvRecListPageReq_t *AvRecListPageReq)
{
    char path[80] = {0};
	int sensor_index = 0;
	int recursive = 1;
	int event_type = 0;
	int mode = 1;
	int ret = IPCNET_RET_OK;

    AvRecListPageReq->p;
    sensor_index = AvRecListPageReq->si;

	if(AvRecListPageReq->m >= 0 && AvRecListPageReq->m <= 1)
	{
	    mode = AvRecListPageReq->m;
	}
	if(AvRecListPageReq->re >= 0)
	{
	    recursive = AvRecListPageReq->re;
	}
    event_type = AvRecListPageReq->et;

	if(AvRecListPageReq->p[0] == '/')
	{
	    strcpy(path, AvRecListPageReq->p);
	}
	else
	{
        if(event_type)
            sprintf(path, "%s/%s", get_alarm_record_path(), AvRecListPageReq->p);
        else
            sprintf(path, "%s/%s", get_common_record_path(), AvRecListPageReq->p);
	}

    IPCNetAvRecFileInfo_t temp[20];
    memset(temp, 0, sizeof(temp));

    struct tm *time_p;
    struct tm result;
    time_t tm;
    int curDate;
    int num = 0;
    time(&tm);
    tm += 8*3600;
    time_p = localtime_r(&tm, &result);
    curDate = (time_p->tm_year+1900)*100*100 + (time_p->tm_mon + 1)*100 + time_p->tm_mday;
    LOGI("cur date=%d\n",curDate);
    if (AvRecListPageReq->et == 1)
    {
        if(AvRecListPageReq->de == curDate)
        {
            LOGI("get cur date file\n\n");
            mutex_lock(&videoFileInfoLock);
            // syncFileListToArray(&gVideoFileInfo, 0, AvRecListPageReq->de);
            syncFileListToArray(&gVideoFileInfo, 0, 0);
            num = mergeFileArray(&gVideoFileInfo, 20, AvRecListPageReq->st, AvRecListPageReq->e, temp);
            mutex_unlock(&videoFileInfoLock);
        }
        else
        {
            LOGI("get other date file date =%d\n\n", AvRecListPageReq->de);
            // atomic_read(g);
            VideoFileInfo_t *videoFileInfo = getVideolistForOtherDate(AvRecListPageReq->de);
            if (NULL != videoFileInfo)
            {
                num = mergeFileArray(videoFileInfo, 20, AvRecListPageReq->st, AvRecListPageReq->e, temp);
                my_free(videoFileInfo);
            }
            else
            {
                num = -1;
            }
            // syncFileListToArray(&videoFileInfo, 0, AvRecListPageReq->de);
        }
        //lwj test
        #if 0
        if(AvRecListPageReq->de > curDate)
        {
            LOGI("get cur date file\n\n");
            mutex_lock(&videoFileInfoLock);
            syncFileListToArray(&gVideoFileInfo, 0, AvRecListPageReq->de);
            num = mergeFileArray(&gVideoFileInfo, 20, AvRecListPageReq->st, AvRecListPageReq->e, temp);
            mutex_unlock(&videoFileInfoLock);
        }
        #endif
    }
    else
    {
        // #define LWJ_ALARM_RECORDER_TEST
        #ifdef LWJ_ALARM_RECORDER_TEST
        LOGI("alarm file index =%d",AvRecListPageReq->s);
        temp[0].t = 0;
        temp[0].sl = 0;
        temp[0].sh = 0;
        sprintf(temp[0].n, "141358-141600.pes");
        num = 1;
        #endif

        #if 1
            LOGI("alarm file index =%d",AvRecListPageReq->s);
            num = getAlarmInfoToArray(20,AvRecListPageReq->s, AvRecListPageReq->de, temp);
            if (num < 20)
            {
                LOGI("get next alarm page info index =%d",AvRecListPageReq->s+num);
                num += getAlarmInfoToArray(20-num, AvRecListPageReq->s+num, AvRecListPageReq->de, &temp[num-1]);
            }
            LOGI("alarm info num %d",num);
        #endif
    }
    

    if(num < 0)
    {
        LOGI("get data null");  
        num = 0;
    }

    for (size_t i = 0; i < num; i++)
    {
        strcpy(pAvRecListPageResp->l[i].n,temp[i].n);
        pAvRecListPageResp->l[i].t = temp[i].t;
        pAvRecListPageResp->l[i].sl = temp[i].sl;
        pAvRecListPageResp->l[i].sh = temp[i].sh;
        // LOGI("i=%d n=%s\n",i,pAvRecListPageResp->l[i].n);
    }
    pAvRecListPageResp->t = DT_REG;
    strcpy(pAvRecListPageResp->n, AvRecListPageReq->p);
    pAvRecListPageResp->__pri_lCount = num;

        
    return IPCNET_RET_OK;
    
}
#else
int IPCMsgGetAVRecoListPageCfg(IPCNetAvRecListPageResp_t *pAvRecListPageResp, IPCNetAvRecListPageReq_t *pAvRecListPageReq)
{
    int ret;
    int ret_n;
    MyDate date_s;
    MyTime time_s, time_e;

    // date_s.Year = floor(pAvRecListPageReq->ds / 10000.0f);
    // date_s.Mon = floor((pAvRecListPageReq->ds % 10000) / 100.0f);
    // date_s.Day = floor(pAvRecListPageReq->ds % 100);
    

    // time_s.Hour = floor(pAvRecListPageReq->st / 10000.0f);
    // time_s.Min = floor((pAvRecListPageReq->st % 10000) / 100.0f);
    // time_s.Sec = floor(pAvRecListPageReq->st % 100);

    // time_e.Hour = floor(pAvRecListPageReq->e / 10000.0f);
    // time_e.Min = floor((pAvRecListPageReq->e % 10000) / 100.0f);
    // time_e.Sec = floor(pAvRecListPageReq->e % 100);
    int_to_date(pAvRecListPageReq->de, &date_s);
    int_to_time(pAvRecListPageReq->st, &time_s);
    int_to_time(pAvRecListPageReq->e, &time_e);

    //reset g_playback_status;
    g_playback_status.bLoaded = false;
    memset((void*)&g_playback_status.recUnits, 0, UNITS_A_DAY);
    g_playback_status.date_s = date_s;

    LOGI("date_s: %04d-%02d-%02d", date_s.Year, date_s.Mon, date_s.Day);
    LOGI("time_s: %02d:%02d:%02d", time_s.Hour, time_s.Min, time_s.Sec);
    LOGI("time_e: %02d:%02d:%02d", time_e.Hour, time_e.Min, time_e.Sec);

    switch(pAvRecListPageReq->et) {
        case 0:
            // get_all_list_in_a_day(&g_playback_status.recUnits, &date_s, &time_s, &time_e, IPCNET_AV_REC_NUM, pAvRecListPageResp->l);
            break;
        case 1:
            if(!g_playback_status.bLoaded) {
                ret = load_rec_units_a_day(get_rec_root(), &date_s, &g_playback_status.recUnits);
                if(ret < 0) {
                    LOGE("load_rec_units_a_day failed");
                    return -1;
                }
                else {
                    g_playback_status.bLoaded = true;
                }
            }
            ret_n = get_record_list_in_a_day(&g_playback_status.recUnits, &date_s, &time_s, &time_e, IPCNET_AV_REC_NUM, pAvRecListPageResp->l);
            pAvRecListPageResp->t = DT_REG;
            pAvRecListPageResp->__pri_lCount = ret_n;
            break;
        case 2:
            break;
        case 3:
            if(!g_playback_status.bLoaded) {
                ret = load_rec_units_a_day(get_rec_root(), &date_s, &g_playback_status.recUnits);
                if(ret < 0) {
                    LOGE("load_rec_units_a_day failed");
                    return -1;
                }
                else {
                    g_playback_status.bLoaded = true;
                }
            }
            ret_n = get_alarm_list_in_a_day(&g_playback_status.recUnits, &date_s, &time_s, &time_e, IPCNET_AV_REC_NUM, pAvRecListPageResp->l);
            pAvRecListPageResp->t = DT_REG;
            pAvRecListPageResp->__pri_lCount = ret_n;
            break;
        case 4:
            break;
        default:
            break;
    }
    // if(0 == pAvRecListPageReq->et)      // common record;
    // {
    //     ret_n = get_record_list_in_a_day(g_rec_units_a_day, &date_s, &time_s, &time_e, IPCNET_AV_REC_NUM, pAvRecListPageResp->l);
    //     pAvRecListPageResp->t = DT_REG;
    //     pAvRecListPageResp->__pri_lCount = ret_n;
    // }

    // else                                // alarm record;
    // {
    //     ret_n = get_alarm_list_in_a_day(g_rec_units_a_day, &date_s, &time_s, &time_e, IPCNET_AV_REC_NUM, pAvRecListPageResp->l);
    //     pAvRecListPageResp->t = DT_REG;
    //     pAvRecListPageResp->__pri_lCount = ret_n;
    // }
    
    return IPCNET_RET_OK;
}
#endif

int IPCMsgGetAVRecoListInfoCfg(IPCNetAvRecordInfoResp_st *pAvRecordInfoResp, IPCNetAvRecordInfoReq_st *pAvRecordInfoReq)
{
    return 0;

    LOGI("-------------[lwj test]IPCMsgGetAVRecoListInfoCfg\n");
    disk_info_t disk;
	unsigned total=0, free = 0;
	int start_date = 0, end_date = 0;
	char path[64] = {0};
	int event_type = 0;
	char retstr[256];
	int num = 0;
	int ret = IPCNET_RET_OK;
	
	int sensor_index = pAvRecordInfoReq->si;

	get_folder_free_space(get_disk_root_path(),&disk);
	total = disk.size>>20;
	free = disk.free>>20;

	LOGI("jpath->valuestring=%s",pAvRecordInfoReq->p);

	if(!strlen(pAvRecordInfoReq->p)){
	    LOGW("PATH is null");
        start_date = pAvRecordInfoReq->ds;
        LOGI("start_date:%d", start_date);
        if(pAvRecordInfoReq->de == 0)
        {
            end_date = start_date;
        }
        else
        {
            end_date = pAvRecordInfoReq->de;
        }
        LOGI("end_date:%d", end_date);
	}
	
	if(pAvRecordInfoReq->et <= 1){
	    event_type = pAvRecordInfoReq->et;
	}

    if(start_date)
    {
        int tmp_num;
        int tmp_start = pAvRecordInfoReq->st;
        int tmp_end;
        while(start_date <= end_date)
        {
            if(event_type)
            {
                sprintf(path, "%s/%d/%02d", get_alarm_record_path(), start_date/100, start_date%100);
            }
            else
            {
                sprintf(path, "%s/%d/%02d", get_common_record_path(), start_date/100, start_date%100); 
            }
            LOGI("PATH:%s", path);

            if(start_date == end_date)
            {
                tmp_end = pAvRecordInfoReq->e;
            }
            else
            {
                tmp_end = 235959;
            }

            tmp_num = get_dir_file_num(path, 0, tmp_start, tmp_end);
        	if(tmp_num < 0){
        		//ret = IPCNET_RET_REQ_ILLEGAL;
        		//goto error;
        		LOGI("get dir file num error!num < 0");
        		tmp_num = 0;
        	}
        	num += tmp_num;

        	start_date++;
        	if(start_date%100 > 31)
        	{
        	    start_date = (start_date/100 + 1) * 100 + 1;
        	    if((start_date/100)%100 > 12)
        	    {
        	        start_date = ((start_date/10000 + 1) * 100 + 1) * 100 + 1;
        	    }
        	}

        	tmp_start = 000000;
        }
    }
    else
    {
    	num = get_dir_file_num(pAvRecordInfoReq->p,pAvRecordInfoReq->m,pAvRecordInfoReq->st,
                pAvRecordInfoReq->e);
    	if(num < 0){
    		//ret = IPCNET_RET_REQ_ILLEGAL;
    		//goto error;
    		LOGI("get dir file num error!num < 0");
    		num = 0;
    	}
    }
    pAvRecordInfoResp->n = num;
    pAvRecordInfoResp->t = total;
    pAvRecordInfoResp->u = free;
	sprintf(retstr,"{\"li\":{\"n\":%d,\"t\":%u,\"u\":%u},\"ret\":%d}",num,total,free,IPCNET_RET_OK);
	LOGI("%s\n",retstr);

error:
	return ret;
    // return IPCNET_RET_OK;
}

/*
REQ:
{"lir":{
	"si": sensor_index,
	"m": mode, //0:不包括文件夹 1：包括文件夹
	"p": path,  //mnt/s0/media/sensor0
	"st": start, //hhmmss
	"e": end, //hhmmss
}}

RESP:
{"li":{
	"n": num,
	"t": total_space,
	"u": free_space,
	}
	"ret": result,
}
*/

int IPCMsgGetRecoDateByMonth(IPCNetRet_st *pRecordDateResp, IPCNetRecoMonth_st *pRecoMonthReq)
{
    char yearMonth[32] = {0};
    int dateBit = 0;
    char filePath[128];
    snprintf(yearMonth, sizeof(yearMonth), "%04d%02d", pRecoMonthReq->year, pRecoMonthReq->month);
    for (int i = 1; i <= 31; i++)
    {
        snprintf(filePath, sizeof(filePath), "%s%s/%02d", DEFAULT_COMMOM_REC_PATH, yearMonth, i);
        LOGI("file path :%s", filePath);
        if(0 == access(filePath, F_OK))
        {
            dateBit |= (0x00000001<<(i-1));
            LOGI("dateBit[%08X]", dateBit);
        }
    }
    pRecordDateResp->ret = dateBit;
    return IPCNET_RET_OK;
}
#if 0
int IPCServer_PlayRecordReq(struct THR_TASK *ptask,uint32_t event_id,IN uint32_t session,IN char *in,IN size_t inlen)
{
    
    LOGI("***************[lwj test]IPCServer_PlayRecordReq]\n");
    // return IPCNET_RET_RESOURCE_ERROR;
    LOGI("***************[lwj test]IPCServer_PlayRecordReq inlen:%d in:%s]\n", inlen, in);
    // ATTACH_INFO_t *attach_info_p = (ATTACH_INFO_t *)handler_list->event.pmsg;
    IPCNET_RECORD_REQ_t *playrecord_req_p = JSON_TO_CSTRUCT_FUNC(IPCNET_RECORD_REQ_t, in);
    IPCNET_RECORD_RESP_t playrecord_resp;
    AV_RECORDER_TIME_t RequstTime;
    char* json_resp = NULL;
    IPCNET_RECORD_COMMAND_e command;
    int fd = -1;
    int recorder_id;
    int playback_flag, playback_status;
    int dur_secs = 0;
    int ret = IPCNET_RET_OK;
    char rec_file[128] = {0};

    if(!playrecord_req_p)
    {
        LOGE("parse:%s FAIL", in);
        return IPCNET_RET_FORMAT_FAIL;
    }
    
    command = playrecord_req_p->Command;
    LOGI("Command:%d", command);
    if(command != IPCNET_CMD_RECORD_PLAY)
    {
        recorder_id = playrecord_req_p->recorder_id;
        
        if(recorder_id < 0 || recorder_id > AV_RECORDER_SERV_NUM)
        {
            LOGE("recorder_id:%d err, AV_RECORDER_SERV_NUM[%d]", recorder_id, AV_RECORDER_SERV_NUM);
            return IPCNET_RET_REQ_ILLEGAL;
        }
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        playback_flag = gAVRecorderServ[recorder_id].flag;
        playback_status = gAVRecorderServ[recorder_id].status;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);

        if(!playback_flag)
        {
            LOGE("err: recorder_id:%d flag == 0", playback_flag);
            return IPCNET_RET_RESOURCE_ERROR;
        }
    }
    
    if(command == IPCNET_CMD_RECORD_PLAY)
    {
        // playrecord_req_p->Args[0] = 1;
        LOGI("IPCNET_CMD_RECORD_PLAY (playType=%d) ViCh=%d time[%04hd-%02d-%02d %02d:%02d:%02d],Filename:%s", playrecord_req_p->Args[0],
        	playrecord_req_p->ViCh,playrecord_req_p->Date.Year,playrecord_req_p->Date.Mon,
        	playrecord_req_p->Date.Day,playrecord_req_p->Time.Hour,playrecord_req_p->Time.Min,
        	playrecord_req_p->Time.Sec,playrecord_req_p->Filename);


        int reqDate = 0;
        int iTime = 0;
        UtcToLocalTime(playrecord_req_p, &reqDate, &iTime);

        int seekSec;
        char vedioFilename[128];
        LOGI("[lwj test]start recoder play date=%d time=%d", reqDate, iTime);
        
        // int len = sprintf(vedioFilename, "%s%d%02d/%02d/", get_common_record_path(), 
        //         playrecord_req_p->Date.Year, playrecord_req_p->Date.Mon, playrecord_req_p->Date.Day);
        if (playrecord_req_p->Args[0] == 1)
        {
            // reqDate = playrecord_req_p->Date.Year*100*100+playrecord_req_p->Date.Mon*100+playrecord_req_p->Date.Day;
            // int hour = playrecord_req_p->Time.Hour+8;
            // hour = (hour > 23)?(hour-24):hour;
            // iTime = hour*100*100+playrecord_req_p->Time.Min*100+playrecord_req_p->Time.Sec;
            // LOGI("[reqDate=%d, iTime=%d]", reqDate, iTime);
            UtcToLocalTime(playrecord_req_p, &reqDate, &iTime);
        }

        int dateIndex = -1;
        VideoFileInfo_t *pFileInfo = NULL;
        pFileInfo = getVideolistForDate(reqDate, &dateIndex);
        
        int playingFN = -1;
        int fileSeekSec = vedioFileSeek2(pFileInfo, iTime, vedioFilename, &seekSec, &playingFN);
        if (pFileInfo)
        {
            my_free(pFileInfo);
        }
        
        if (fileSeekSec == -1)
        {
            LOGI("vedioFileSeek fail");
            return IPCNET_RET_RESOURCE_ERROR;
        }
        LOGI("seekSec=%d filename:%s fileSeekSec:%d\n", seekSec, vedioFilename, fileSeekSec);
            
        LOGI("open requst rec file:%s", vedioFilename);
        fd = open(vedioFilename, O_RDONLY);
        if(fd < 0)
        {
            LOGE("fopen:%s fail", vedioFilename);
            return IPCNET_RET_RESOURCE_ERROR;
        }

        int block = 1;//playrecord_req_p->Args[0];
        recorder_id = find_av_recorder_serv_valid(playrecord_resp.stream_shm, block);
        if(recorder_id < 0)
        {
            LOGE("find_av_recorder_serv_valid fail");
            close(fd);
            return IPCNET_RET_RESOURCE_ERROR;
        }
        playrecord_resp.recorder_id = recorder_id;
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].fd = fd;
        gAVRecorderServ[recorder_id].status = IPCNET_CMD_RECORD_PLAY;
        gAVRecorderServ[recorder_id].event_id = event_id;
        // gAVRecorderServ[recorder_id].dur_secs = dur_secs;
        gAVRecorderServ[recorder_id].block = block;
        //add by lwj
        gAVRecorderServ[recorder_id].seek_secs = seekSec;
        // gAVRecorderServ[recorder_id].dur_secs = fileSeekSec;
        gAVRecorderServ[recorder_id].seek_flag = 1;
        gAVRecorderServ[recorder_id].recorder_playing_fn = playingFN;
        gAVRecorderServ[recorder_id].recorder_date_index = dateIndex;
        //add end by lwj
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
        LOGI("recorder_id:%d event_id:0x%x fd:%d",
            recorder_id, event_id, fd);

        create_replay_recorder_task(recorder_id);

        json_resp = CSTRUCT_TO_JSON_FUNC(IPCNET_RECORD_RESP_t, &playrecord_resp);
        if(!json_resp)
        {
            LOGE("CSTRUCT_TO_JSON_FUNC playrecord_resp FAIL");
            // *outlen = 0;
            // out[0] = '\0';
        }
        else
        {
            LOGI("playrecord_resp:%s", json_resp);
            // *outlen = strlen(json_resp) + 1;
            // memcpy(out, json_resp, *outlen);
        }  
        
        sepcam_ipc_server_send_response2(ptask, event_id, IPCNET_PLAY_RECORD_RESP, IPCNET_RET_OK, json_resp);

    }
    else if(command == IPCNET_CMD_RECORD_STOP)
    {
        LOGI("IPCNET_CMD_RECORD_STOP recorder_id:%d", recorder_id);
        recorder_id = playrecord_req_p->recorder_id;
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].status = IPCNET_CMD_RECORD_STOP;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
    }
    else if(command == IPCNET_CMD_RECORD_PAUSE)
    {
        LOGI("IPCNET_CMD_RECORD_PAUSE recorder_id:%d", recorder_id);
        recorder_id = playrecord_req_p->recorder_id;
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].status = IPCNET_CMD_RECORD_PAUSE;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
    }
    else if(command == IPCNET_CMD_RECORD_REPLAY)
    {
        LOGI("IPCNET_CMD_RECORD_REPLAY");
        recorder_id = playrecord_req_p->recorder_id;
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].status = IPCNET_CMD_RECORD_PLAY;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
    }
    else if(command == IPCNET_CMD_RECORD_SPEED)
    {
        int play_speed = 1, play_slow = 1;
        recorder_id = playrecord_req_p->recorder_id;

        if(playrecord_req_p->__pri_ArgsCount >= 2)
        {
            play_slow = playrecord_req_p->Args[1];
            LOGI("play_slow:%d", play_speed);
        }
        if(playrecord_req_p->__pri_ArgsCount >= 1)
        {
            play_speed = playrecord_req_p->Args[0];
            LOGI("play_speed:%d", play_speed);
        }
        if(playrecord_req_p->__pri_ArgsCount == 0)
        {
            LOGW("command:IPCNET_CMD_RECORD_SPEED, set default paly_speed");
        }
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].speed_flag = 1;
        gAVRecorderServ[recorder_id].play_speed = play_speed;
        gAVRecorderServ[recorder_id].play_slow = play_slow;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
    }
    else if(IPCNET_CMD_RECORD_SEEK == command)
    {
        LOGI("IPCNET_CMD_RECORD_SEEK------------------------");
        int seekSec;
        recorder_id = playrecord_req_p->recorder_id;
        int fd = -1;
        int dateIndex = -1;
        int playingFN;
        VideoFileInfo_t *pFileInfo = NULL;
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        fd = gAVRecorderServ[recorder_id].fd;
        // dateIndex = gAVRecorderServ[recorder_id].recorder_date_index;
        playingFN = gAVRecorderServ[recorder_id].recorder_playing_fn;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);

        int reqDate, itime;
        UtcToLocalTime(playrecord_req_p, &reqDate, &itime);

        char vedioFilename[128];
        // int len = sprintf(vedioFilename, "%s%d%02d/%02d/", get_common_record_path(), 
        //         result.tm_year + 1900, result.tm_mon + 1, result.tm_mday);
        int len = sprintf(vedioFilename, "%s%d/%02d/", get_common_record_path(), 
                reqDate/100, reqDate%100);
        LOGI("dateIndex:%d [%d-%d]", dateIndex, reqDate, itime);
        // pFileInfo = getCurVideolist(dateIndex);
        // int dateIndex = 0;
        pFileInfo = getVideolistForDate(reqDate, &dateIndex);
        if (NULL == pFileInfo)
        {
            LOGI("pFileInfo NULL---------------------");
            return IPCNET_RET_RESOURCE_ERROR;
        }
        int fileSeekSec = vedioFileSeek(pFileInfo, itime, vedioFilename+len, &seekSec, &playingFN);
        my_free(pFileInfo);
        if(fileSeekSec < 0)
        {
            LOGI("vedioFileSeek fail------------------------");
            return IPCNET_RET_RESOURCE_ERROR;
        }
        close(fd);
        LOGI("open requst rec file:%s", vedioFilename);
        fd = open(vedioFilename, O_RDONLY);
        if(fd < 0)
        {
            LOGE("fopen:%s fail", playrecord_req_p->Filename);
            return IPCNET_RET_RESOURCE_ERROR;
        }
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].fd = fd;
        gAVRecorderServ[recorder_id].seek_secs = seekSec;
        // gAVRecorderServ[recorder_id].dur_secs = fileSeekSec;
        gAVRecorderServ[recorder_id].seek_flag = 1;
        gAVRecorderServ[recorder_id].status = IPCNET_CMD_RECORD_SEEK;
        gAVRecorderServ[recorder_id].recorder_playing_fn = playingFN;
        gAVRecorderServ[recorder_id].recorder_date_index = dateIndex;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
    }
    
    //[lwj]使能以下代码，程序会蹦
    // free_jstruct(playrecord_req_p);
    // if(json_resp)
    // {
    //     free_jstruct(json_resp);
    // }
    return ret;
}
#else
int IPCServer_PlayRecordReq(struct THR_TASK *ptask,uint32_t event_id,IN uint32_t session,IN char *in,IN size_t inlen) {
   
    LOGI("***************[lwj test]IPCServer_PlayRecordReq]\n");
    // return IPCNET_RET_RESOURCE_ERROR;
    LOGI("***************[lwj test]IPCServer_PlayRecordReq inlen:%d in:%s]\n", inlen, in);
    // ATTACH_INFO_t *attach_info_p = (ATTACH_INFO_t *)handler_list->event.pmsg;
    IPCNET_RECORD_REQ_t *playrecord_req_p = JSON_TO_CSTRUCT_FUNC(IPCNET_RECORD_REQ_t, in);
    IPCNET_RECORD_RESP_t playrecord_resp;
    AV_RECORDER_TIME_t RequstTime;
    char* json_resp = NULL;
    IPCNET_RECORD_COMMAND_e command;
    int fd = -1;
    int recorder_id;
    int playback_flag, playback_status;
    // int dur_secs = 0;
    int ret = IPCNET_RET_OK;
    char rec_file[128] = {0};

// added by lishun for seek;
    MyDate date_s;
    MyTime time_s;
    char recDir[MAX_PATH];
    char recName[MAX_NAME];
    char recFilePath[MAX_PATH];
    char recIdxPath[MAX_PATH];
    int seekSec;
    int offset;
    int reqDate = 0;
    int iTime = 0;
// end by lishun

    if(!playrecord_req_p)
    {
        LOGE("parse:%s FAIL", in);
        return IPCNET_RET_FORMAT_FAIL;
    }
    
    command = playrecord_req_p->Command;
    LOGI("Command:%d", command);
    if(command != IPCNET_CMD_RECORD_PLAY)
    {
        recorder_id = playrecord_req_p->recorder_id;
        
        if(recorder_id < 0 || recorder_id > AV_RECORDER_SERV_NUM)
        {
            LOGE("recorder_id:%d err, AV_RECORDER_SERV_NUM[%d]", recorder_id, AV_RECORDER_SERV_NUM);
            return IPCNET_RET_REQ_ILLEGAL;
        }
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        playback_flag = gAVRecorderServ[recorder_id].flag;
        playback_status = gAVRecorderServ[recorder_id].status;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);

        if(!playback_flag)
        {
            LOGE("err: recorder_id:%d flag == 0", playback_flag);
            return IPCNET_RET_RESOURCE_ERROR;
        }
    }

    // get disk info: {
    // IPCNetDiskInfo_st diskInfo;
    // memset(&diskInfo, 0, sizeof(IPCNetDiskInfo_st));
    // ret = ipcam_get_device_config(IPCNET_GET_DISK_CFG_REQ, &diskInfo);
    // if(ret != IPCNET_RET_OK)
    // {
    //     LOGE("IPCNET_GET_DISK_CFG_REQ FAIL err:%d", ret);
    //     return FAIL;
    // }
    // }
    
    if(command == IPCNET_CMD_RECORD_PLAY)
    {
        // playrecord_req_p->Args[0] = 1;
        LOGI("IPCNET_CMD_RECORD_PLAY (playType=%d) ViCh=%d time[%04hd-%02d-%02d %02d:%02d:%02d],Filename:%s", playrecord_req_p->Args[0],
        	playrecord_req_p->ViCh,playrecord_req_p->Date.Year,playrecord_req_p->Date.Mon,
        	playrecord_req_p->Date.Day,playrecord_req_p->Time.Hour,playrecord_req_p->Time.Min,
        	playrecord_req_p->Time.Sec,playrecord_req_p->Filename);


        UtcToLocalTime(playrecord_req_p, &reqDate, &iTime);

        // char vedioFilename[128];
        // LOGI("[lwj test]start recoder play date=%d time=%d", reqDate, iTime);
        
        // int len = sprintf(vedioFilename, "%s%d%02d/%02d/", get_common_record_path(), 
        //         playrecord_req_p->Date.Year, playrecord_req_p->Date.Mon, playrecord_req_p->Date.Day);
        if (playrecord_req_p->Args[0] == 1)
        {
            // reqDate = playrecord_req_p->Date.Year*100*100+playrecord_req_p->Date.Mon*100+playrecord_req_p->Date.Day;
            // int hour = playrecord_req_p->Time.Hour+8;
            // hour = (hour > 23)?(hour-24):hour;
            // iTime = hour*100*100+playrecord_req_p->Time.Min*100+playrecord_req_p->Time.Sec;
            // LOGI("[reqDate=%d, iTime=%d]", reqDate, iTime);
            UtcToLocalTime(playrecord_req_p, &reqDate, &iTime);
        }

        int_to_date(reqDate, &date_s);
        int_to_time(iTime, &time_s);
        strcpy(recDir, get_record_dir_by_date(get_rec_root(), &date_s, true));
        strcpy(recName, get_record_name_by_time(&time_s));
        sprintf(recFilePath, "%s/%s.pes", recDir, recName);
        // sprintf(recIdxPath, "%s/%s.idx", recDir, recName);

        // load rec units in a day;
        // if(!g_playback_status.bLoaded) {
            // 先查列表不然offset可能不对,可以改成每次都load可能会耗时
            ret = load_rec_units_a_day(get_rec_root(), &date_s, &g_playback_status.recUnits);
            if(ret < 0) {
                LOGE("load_rec_units_a_day failed");
                return -1;
            }
            else {
                g_playback_status.bLoaded = true;
            }
        // }


        LOGI("open requst rec file:%s", recFilePath);
        fd = open(recFilePath, O_RDONLY);
        if(fd < 0)
        {
            LOGE("fopen:%s fail", recFilePath);
            return IPCNET_RET_RESOURCE_ERROR;
        }

        seekSec = time_s.Hour*3600 + time_s.Min*60 + time_s.Sec;
        offset = get_offset_by_time(&time_s, &g_playback_status.recUnits);
        if(offset < 0 ) {
            return IPCNET_RET_RESOURCE_ERROR;
        }

        int block = 1;//playrecord_req_p->Args[0];
        recorder_id = find_av_recorder_serv_valid(playrecord_resp.stream_shm, block);
        if(recorder_id < 0)
        {
            LOGE("find_av_recorder_serv_valid fail");
            close(fd);
            return IPCNET_RET_RESOURCE_ERROR;
        }
        playrecord_resp.recorder_id = recorder_id;

        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].fd = fd;
        gAVRecorderServ[recorder_id].status = IPCNET_CMD_RECORD_PLAY;
        gAVRecorderServ[recorder_id].event_id = event_id;
        // gAVRecorderServ[recorder_id].dur_secs = dur_secs;
        gAVRecorderServ[recorder_id].block = block;
        //add by lwj
        gAVRecorderServ[recorder_id].seek_flag = 1;
        gAVRecorderServ[recorder_id].seek_secs = seekSec;
        gAVRecorderServ[recorder_id].offset = offset;

        gAVRecorderServ[recorder_id].date_s = date_s;
        gAVRecorderServ[recorder_id].time_s = time_s;
        // gAVRecorderServ[recorder_id].recorder_playing_fn = playingFN;
        // gAVRecorderServ[recorder_id].recorder_date_index = dateIndex;
        //add end by lwj
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
        LOGI("recorder_id:%d event_id:0x%x fd:%d",
            recorder_id, event_id, fd);
        LOGI("[lwj test]:%d offset:[%d] time[%d-%d-%d]",
            gAVRecorderServ[recorder_id].seek_secs, gAVRecorderServ[recorder_id].offset, 
            time_s.Hour, time_s.Min,time_s.Sec);

        create_replay_recorder_task(recorder_id);

        json_resp = CSTRUCT_TO_JSON_FUNC(IPCNET_RECORD_RESP_t, &playrecord_resp);
        if(!json_resp)
        {
            LOGE("CSTRUCT_TO_JSON_FUNC playrecord_resp FAIL");
            // *outlen = 0;
            // out[0] = '\0';
        }
        else
        {
            LOGI("playrecord_resp:%s", json_resp);
            // *outlen = strlen(json_resp) + 1;
            // memcpy(out, json_resp, *outlen);
        }  
        
        sepcam_ipc_server_send_response2(ptask, event_id, IPCNET_PLAY_RECORD_RESP, IPCNET_RET_OK, json_resp);

    }
    else if(command == IPCNET_CMD_RECORD_STOP)
    {
        LOGI("IPCNET_CMD_RECORD_STOP recorder_id:%d", recorder_id);
        recorder_id = playrecord_req_p->recorder_id;
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].status = IPCNET_CMD_RECORD_STOP;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
    }
    else if(command == IPCNET_CMD_RECORD_SPEED)
    {
        int play_speed = 1, play_slow = 1;
        recorder_id = playrecord_req_p->recorder_id;

        if(playrecord_req_p->__pri_ArgsCount >= 2)
        {
            play_slow = playrecord_req_p->Args[1];
            LOGI("play_slow:%d", play_speed);
        }
        if(playrecord_req_p->__pri_ArgsCount >= 1)
        {
            play_speed = playrecord_req_p->Args[0];
            LOGI("play_speed:%d", play_speed);
        }
        if(playrecord_req_p->__pri_ArgsCount == 0)
        {
            LOGW("command:IPCNET_CMD_RECORD_SPEED, set default paly_speed");
        }
        
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].speed_flag = 1;
        gAVRecorderServ[recorder_id].play_speed = play_speed;
        gAVRecorderServ[recorder_id].play_slow = play_slow;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
    }
    else if(IPCNET_CMD_RECORD_SEEK == command)
    {
        LOGI("IPCNET_CMD_RECORD_SEEK------------------------");
        int seekSec;
        recorder_id = playrecord_req_p->recorder_id;
        int fd = -1;
        int dateIndex = -1;
        // int playingFN;
        // VideoFileInfo_t *pFileInfo = NULL;
        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        fd = gAVRecorderServ[recorder_id].fd;
        // dateIndex = gAVRecorderServ[recorder_id].recorder_date_index;
        // playingFN = gAVRecorderServ[recorder_id].recorder_playing_fn;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);

        int reqDate, iTime;
        UtcToLocalTime(playrecord_req_p, &reqDate, &iTime);

        close(fd);

        int_to_date(reqDate, &date_s);
        int_to_time(iTime, &time_s);
        strcpy(recDir, get_record_dir_by_date(get_rec_root(), &date_s, true));
        strcpy(recName, get_record_name_by_time(&time_s));
        sprintf(recFilePath, "%s/%s.pes", recDir, recName);
        sprintf(recIdxPath, "%s/%s.idx", recDir, recName);

        LOGI("open requst rec file:%s", recFilePath);
        fd = open(recFilePath, O_RDONLY);
        if(fd < 0)
        {
            LOGE("fopen:%s fail", playrecord_req_p->Filename);
            return IPCNET_RET_RESOURCE_ERROR;
        }

        seekSec = time_s.Hour*3600 + time_s.Min*60 + time_s.Sec;
        offset = get_offset_by_time(&time_s, &g_playback_status.recUnits);
        if(offset < 0 ) {
            return IPCNET_RET_RESOURCE_ERROR;
        }

        mutex_lock(&gAVRecorderServ[recorder_id].lock);
        gAVRecorderServ[recorder_id].fd = fd;
        gAVRecorderServ[recorder_id].seek_secs = seekSec;
        gAVRecorderServ[recorder_id].offset = offset;
        gAVRecorderServ[recorder_id].seek_flag = 1;
        gAVRecorderServ[recorder_id].status = IPCNET_CMD_RECORD_SEEK;

        gAVRecorderServ[recorder_id].date_s = date_s;
        gAVRecorderServ[recorder_id].time_s = time_s;
        mutex_unlock(&gAVRecorderServ[recorder_id].lock);
    }
    
    //[lwj]使能以下代码，程序会蹦
    // free_jstruct(playrecord_req_p);
    // if(json_resp)
    // {
    //     free_jstruct(json_resp);
    // }
    return ret; 
}
#endif

void recorder_server_event_destroy_cb(THR_TASK_t *ptask,uint32_t event_id)
{
    int i;
    
    LOGI("event_id:0x%x", event_id);
    
    for(i = 0; i < AV_RECORDER_SERV_NUM; i++)
    {
        mutex_lock(&gAVRecorderServ[i].lock);
        LOGI("event_id:0x%x gAVRecorderServ i:%d flag:%d event_id:0x%x",
            event_id, i, gAVRecorderServ[i].flag, gAVRecorderServ[i].event_id);
        if(gAVRecorderServ[i].flag && gAVRecorderServ[i].event_id == event_id)
        {
            LOGI("IPCNET_CMD_RECORD_STOP gAVRecorderServ i:%d flag:%d event_id:0x%x",
                i, gAVRecorderServ[i].flag, gAVRecorderServ[i].event_id);
            gAVRecorderServ[i].status = IPCNET_CMD_RECORD_STOP;
        }
        mutex_unlock(&gAVRecorderServ[i].lock);
    }
    
    return;
}

int init_recorder_server(char *process_name)
{
    LOGI("init_av_recorder_serv");
    init_av_recorder_serv();
    // add by lwj
    initVideoFileInfo();
    // add end by lwj
    
	LOGI("init_sepcam_ipc_server");
    init_sepcam_ipc_server(gRecorderIPCMsgProcessList, recorder_server_event_destroy_cb);
    LOGI("start_sepcam_ipc_server,process_name=%s", process_name);
    start_sepcam_ipc_server(process_name,NULL);
    return 0;
}



//add by lwj
int syncAlarmInfoToArray(IPCNETAlarmRecordList_st *alarmRecordList, int pageindx, int date)
{
    // sizeof(IPCNETAlarmRecordList_st);
    // alarmRecordList->PageNum
    return 0;
}

#if 0
int getAlarmInfoToArray(int size, int index, int date, IPCNetAvRecFileInfo_t *outInfo)
{
    char path[128];
    sprintf(path, "%s/%d/%02d", get_common_record_path(), date/100, date%100);
    int cnt = 0;
    int page = index/100;
    int startIdx = index - page*100;

    struct tm *time_p;
    struct tm result;
    time_t tm;
    int curDate;
    time(&tm);
    tm += 8*3600;
    time_p = localtime_r(&tm, &result);
    curDate = (time_p->tm_year+1900)*100*100 + (time_p->tm_mon + 1)*100 + time_p->tm_mday;
    

    IPCNETAlarmRecordList_st alarmList;
    if (date == curDate && page == g_alarmRecordList.PageNum) //date == g_alarmRecordList.date &&
    {
        LOGI("cur page");
        memcpy(&alarmList, &g_alarmRecordList, sizeof(IPCNETAlarmRecordList_st));
    }
    else
    {
        LOGI("other page");
        int ret = load_alarm_record_list(date, page, &alarmList);
        if(ret == -1)
        {
            LOGI("json error");
            return -1;
            // ret = load_alarm_record_list(date, page+1, &alarmList);
        }
        else if(ret == -2)
        {
            LOGI("get file null");
            return -1;
        }
    }
    
    LOGI("-------date[%d], index=%d page=%d startIdx=%d-------", date, index, page, startIdx);
    for (int i = startIdx; i < alarmList.AlarmNum; i++)
    {
        // if((sTime <= alarmList.AlarmArr[i].StartTime) && ( alarmList.AlarmArr[i].EndTime<=etime))
        // {
            // LOGI("---------------alarm info cnt=%d[%d-%d]",cnt, alarmList.AlarmArr[i].StartTime, alarmList.AlarmArr[i].EndTime);
            sprintf(outInfo[cnt].n,"%06d-%06d.pes", alarmList.AlarmArr[i].StartTime, alarmList.AlarmArr[i].EndTime);
            cnt++;
            if(cnt >= size)
                break;
        // }
    }

    return cnt;   
}
#endif

extern int g_timeZone;
void UtcToLocalTime(IPCNET_RECORD_REQ_t *playrecord_req_p, int *iDate, int *iTime)
{   
    struct tm time_st;
    struct tm result;
    time_t tm;
    memset(&time_st,0,sizeof(time_st));  
    time_st.tm_year = playrecord_req_p->Date.Year-1900;
    time_st.tm_mon = playrecord_req_p->Date.Mon-1;
    time_st.tm_mday = playrecord_req_p->Date.Day;
    time_st.tm_hour = playrecord_req_p->Time.Hour;
    time_st.tm_min = playrecord_req_p->Time.Min;
    time_st.tm_sec = playrecord_req_p->Time.Sec;
    tm = mktime(&time_st);  
    tm += g_timeZone*3600;
    localtime_r(&tm, &result);
    LOGI("change utc=%d\n", tm);

    *iDate = (result.tm_year + 1900)*10000 + (result.tm_mon + 1)*100 + result.tm_mday;
    *iTime = result.tm_hour *100*100 + result.tm_min*100 + result.tm_sec;
}
//add end by lwj



