#include "os_ini.h"
#include "IniFile.h"
#include "recorder_params.h"
#include "msg_queue.h"
#include "task_mgr.h"
#include "logServer.h"
#include "sepcam_recorder.h"

#include "inifile_helper.h"
#include "ini_param_port.h"

// added by lishun
#define CFG_VER_STRING_TAG "version"
// end by lishun


static DEV_RECORD_CFG_t g_rec_cfg[MAX_VI_CHANNEL_NUM];
static const T_U8 DEFAULT_PKT_TM_OPT[] = {1, 2, 3, 4, 5, 6};
// static const T_U8 DEFAULT_PKT_TM_OPT[] = {5, 10, 15, 20, 25, 30};
//mutex_t param_mutex;

#define DEFAULT_CONFIG_PATH "/system/web/systemconfig/"
#define VENDOR_CONFIGS_PATH "/system/vendor/configs/"

#define DEFAULT_RECORD_CFG_FILE "record_cfg.ini"

// #define CONFIG_PATH "/rom/config/"
// #define CONFIG_PATH "/customer/tutkipc/var/config"

static const char g_record_cfg_file_default[64] = DEFAULT_CONFIG_PATH DEFAULT_RECORD_CFG_FILE;
static const char g_record_cfg_file_vendor[64] = VENDOR_CONFIGS_PATH DEFAULT_RECORD_CFG_FILE;
static char g_record_cfg_file[64];
extern int g_recorder_log_flag;

static RESULT_t save_record_info();

#define CHECK_VIDEO_IN_CHANNEL(ViCh) \
do{  \
    if((ViCh) < 0 || (ViCh) >= MAX_VI_CHANNEL_NUM)   \
    {   \
        LOGE( "ViCh:%d error, max vi:%d", \
                (ViCh), MAX_VI_CHANNEL_NUM);  \
        return DEV_PARAM_INVALID;    \
    }   \
}while(0);


#define RECORD_CFG_DEFEND_INDEX_TIME(ViCh, LIST, WEEK, INDEX) \
    INI_VALUE_BYTE("record_time_info"#ViCh, \
        "list"#LIST"week"#WEEK"_start"#INDEX"_hh",&g_rec_cfg[ViCh].record_time_policy_list[LIST].record_time_policy[WEEK].start##INDEX.hour), \
    INI_VALUE_BYTE("record_time_info"#ViCh, \
        "list"#LIST"week"#WEEK"_end"#INDEX"_hh",&g_rec_cfg[ViCh].record_time_policy_list[LIST].record_time_policy[WEEK].end##INDEX.hour), \
    \
    INI_VALUE_BYTE("record_time_info"#ViCh, \
        "list"#LIST"week"#WEEK"_start"#INDEX"_min",&g_rec_cfg[ViCh].record_time_policy_list[LIST].record_time_policy[WEEK].start##INDEX.min), \
    INI_VALUE_BYTE("record_time_info"#ViCh, \
        "list"#LIST"week"#WEEK"_end"#INDEX"_min",&g_rec_cfg[ViCh].record_time_policy_list[LIST].record_time_policy[WEEK].end##INDEX.min), \
    \
    INI_VALUE_BYTE("record_time_info"#ViCh, \
        "list"#LIST"week"#WEEK"_start"#INDEX"_sec",&g_rec_cfg[ViCh].record_time_policy_list[LIST].record_time_policy[WEEK].start##INDEX.sec), \
    INI_VALUE_BYTE("record_time_info"#ViCh, \
        "list"#LIST"week"#WEEK"_end"#INDEX"_sec",&g_rec_cfg[ViCh].record_time_policy_list[LIST].record_time_policy[WEEK].end##INDEX.sec)

#define RECORD_CFG_DEFEND_TIME(ViCh, LIST, WEEK) \
    INI_VALUE_INT("record_time_info"#ViCh, \
        "list"#LIST"_week"#WEEK"_en", &g_rec_cfg[ViCh].record_time_policy_list[LIST].record_time_policy[WEEK].en), \
    RECORD_CFG_DEFEND_INDEX_TIME(ViCh, LIST, WEEK, 1), \
    RECORD_CFG_DEFEND_INDEX_TIME(ViCh, LIST, WEEK, 2)

#define RECORD_CFG_DEFEND_TIME_LIST(ViCh, LIST) \
    INI_VALUE_INT("record_time_info"#ViCh, \
        "list"#LIST"_en", &g_rec_cfg[ViCh].record_time_policy_list[LIST].en), \
    RECORD_CFG_DEFEND_TIME(ViCh, LIST, 0), \
    RECORD_CFG_DEFEND_TIME(ViCh, LIST, 1), \
    RECORD_CFG_DEFEND_TIME(ViCh, LIST, 2), \
    RECORD_CFG_DEFEND_TIME(ViCh, LIST, 3), \
    RECORD_CFG_DEFEND_TIME(ViCh, LIST, 4), \
    RECORD_CFG_DEFEND_TIME(ViCh, LIST, 5), \
    RECORD_CFG_DEFEND_TIME(ViCh, LIST, 6), \
    RECORD_CFG_DEFEND_TIME(ViCh, LIST, 7)

#define INI_RECORD_CFG(ViCh) \
    INI_VALUE_INT("record_time_info"#ViCh, "rec_enable", &g_rec_cfg[ViCh].enable), \
    INI_VALUE_SHORT("record_time_info"#ViCh, "ch_no", &g_rec_cfg[ViCh].vich), \
    INI_VALUE_SHORT("record_time_info"#ViCh, "vench_no", &g_rec_cfg[ViCh].vech), \
    INI_VALUE_BYTE("record_time_info"#ViCh, "auto_del", &g_rec_cfg[ViCh].auto_del), \
    INI_VALUE_INT("record_time_info"#ViCh, "rec_time", &g_rec_cfg[ViCh].rec_time), \
    INI_VALUE_INT("record_time_info"#ViCh, "type", &g_rec_cfg[ViCh].storage_type), \
    INI_VALUE_INT("record_time_info"#ViCh, "time_type", &g_rec_cfg[ViCh].time_type), \
    INI_VALUE_NUM_ARRAY("record_time_info"#ViCh, "pkt_tm_opt", g_rec_cfg[ViCh].pkt_tm_options), \
    INI_VALUE_INT("record_time_info"#ViCh, "rec_mode", &g_rec_cfg[ViCh].recMode), \
    RECORD_CFG_DEFEND_TIME_LIST(ViCh, 0), \
    RECORD_CFG_DEFEND_TIME_LIST(ViCh, 1), \
    RECORD_CFG_DEFEND_TIME_LIST(ViCh, 2)     
    
static INI_HEAD_INFO_t g_record_cfg_info[] =
{
    INI_RECORD_CFG(0)
    // INI_RECORD_CFG(1),
    // INI_RECORD_CFG(2),
    // INI_RECORD_CFG(3)
};

static INLINE_t int send_record_task_msg(int src_mid,T_U32 type,void *msg, T_S32 msg_len)
{
    MSG_NODE_t local_msg;
    local_msg.dst_mid = RECORD_TASK_ID;
    local_msg.src_mid = src_mid;
    local_msg.msg = (void *)msg;
    local_msg.msg_len = msg_len;
    local_msg.type = type;
    return send_msg(&local_msg);
}

#if 0
int get_cfg_file_version_ini(const char *filename)
{
    int version = 0;
    if(access(filename,F_OK)<0)
    {
        LOGI("%s not exit!",filename);
        return version;
    }

    if(TRUE == OpenIniFile(filename))							
    {																									
        version = ReadInt("ver_info","version",0);			
																						
        CloseIniFile();
    }
    return version;	
}
#else
/**获取配置文件的版本号，filename-文件�?*/
static int get_ini_cfg_version(const char *filename)
{
    int version = 0;
    
    void *hd = OpenIniFileHandle(filename);
    if(hd)
    {
        IniHandle_ReadInt(hd, "ver_info", CFG_VER_STRING_TAG, &version);
        CloseIniHandle(hd);
    }
    return version;	
}
#endif


/**
 * \brief ���ݰ汾��ȥ�����Ƿ�Ҫ�滻�����ļ�
 * \param src-Դ�ļ���dest-Ŀ���ļ�
 * \return 0-�ɹ���1-ʧ��
 */
int replace_default_cfg_file(const char *src,const char *dest)
{
    char cmd[256] = {0};
    int src_ver = 0;
    int dest_ver = 0;

	if((NULL == src) || (NULL == dest))
	{
		LOGI("error!NULL==src or NULL == dest");
		return FAIL;
	}

	if(access(src,F_OK) < 0)
	{
		LOGI("src:%s not exist!",src);	
		return FAIL;
	}

	if(access(dest,F_OK) < 0)
	{
		sprintf(cmd,"cp -rf %s %s",src,dest);
        system(cmd);
		return SUCCESS;
	}

    if(NULL != strstr(src,".ini"))
    {
        src_ver = get_ini_cfg_version(src);
        dest_ver = get_ini_cfg_version(dest);
    }

#if 0 /**��ʱû�õ�json**/
    else if(NULL != strstr(src,".json"))
    {
        src_ver = get_cfg_file_version_json(src);
        dest_ver = get_cfg_file_version_json(dest);
    }
#endif
    else
    {
        LOGI("UNKONW CFG FILE TYPE");
    }

    if(src_ver != dest_ver)
    {
        LOGI("need replace default cfg src_ver:%d dest_ver:%d file:%s",
            src_ver, dest_ver, dest);
        sprintf(cmd,"cp -rf %s %s",src,dest);
        system(cmd);
    }
    
    return SUCCESS;
}

#if 0
int reserve_json_cfg_version(cJSON *pjson,const char *pfile)
{
    int version = 0;

    if((NULL == pjson) || (NULL == pfile))
    {
        LOGI("input param is NULL");
        return FAIL;
    }
    
    version = get_cfg_file_version_json(pfile);
    LOGI("version=%d,file=%s",version,pfile);
    if(0 != version)
	{
        cJSON_AddNumberToObject(pjson,"Version",version);
	}
    return SUCCESS;
}
#endif

#if 0
int reserve_ini_cfg_version(int version)
{
    LOGI("version=%d",version);
    if(version > 0)
    {
        WriteInt("ver_info","version",version);
    }
    return SUCCESS;
}
#else
// int reserve_ini_cfg_version(void *ini_handle, int version)
// {
//     if(version > 0)
//     {
//         IniHandle_WriteInt(ini_handle, "ver_info", CFG_VER_STRING_TAG, version);
//     }
//     return SUCCESS;
// }
#endif

//start------------------------record info------------------------------------

int dev_get_record_cfg(int vich, DEV_RECORD_CFG_t *info_p)
{
	CHECK_VIDEO_IN_CHANNEL(vich);
	
	memcpy(info_p, &g_rec_cfg[vich], sizeof(DEV_RECORD_CFG_t));
	return SUCCESS;
}

int get_recorder_auto_del(int vich)
{
	int auto_del_flg = 1;
	if((vich) < 0 || (vich) >= MAX_VI_CHANNEL_NUM)
	{
		LOGW("vich=%d!out of range",vich);
		return auto_del_flg;
	}		
	auto_del_flg = g_rec_cfg[vich].auto_del;
	return auto_del_flg;
}

int dev_check_datetime(DEV_TIME_t *date_time)
{
    if(date_time->hour >= 24
    || date_time->min >= 60
    || date_time->sec >= 60)
    {
        LOGE( "date_time error:%d:%d:%d",
            date_time->hour, date_time->min, date_time->sec);
        return DEV_PARAM_INVALID;
    }
    return SUCCESS;
}

int dev_check_defend_datetime(DEV_TIME_t *start, DEV_TIME_t *end)
{
    if(SUCCESS != dev_check_datetime(start)
    || SUCCESS != dev_check_datetime(end))
    {
        return DEV_PARAM_INVALID;
    }
    
    #if 0   //disable by lwj
    if(start->hour > end->hour
    || (start->hour == end->hour && start->min > end->min)
    || (start->hour == end->hour && start->min == end->min && start->sec > end->sec))
    {
        LOGE( "defend time error:start %d:%d:%d, end %d:%d:%d",
            start->hour, start->min, start->sec,
            end->hour, end->min, end->sec);
        return DEV_PARAM_INVALID;
    }
    #endif 
    return SUCCESS;
}

int dev_check_defend_time(DEV_DEFEND_TIME_INFO_t *record_time_policy)
{
    DEV_TIME_t *date_time;
    
    if(!record_time_policy->en)
    {
       return SUCCESS; 
    }

    if(SUCCESS != dev_check_defend_datetime(&record_time_policy->start1, &record_time_policy->end1))
    {
        LOGE( "start1 end1 error");
        return DEV_PARAM_INVALID;
    }
    if(SUCCESS != dev_check_defend_datetime(&record_time_policy->start2, &record_time_policy->end2))
    {
        LOGE( "start2 end2 error");
        return DEV_PARAM_INVALID;
    }

    return SUCCESS;
}

#if 0
int dev_set_record_cfg(DEV_RECORD_CFG_t *info_p)
{
    DEV_RECORD_CFG_t *rec;
    int vich = info_p->vich;
    int i;

    CHECK_VIDEO_IN_CHANNEL(vich);

    rec = &g_rec_cfg[vich];

    for(i = 0; i < (int)(sizeof(info_p->record_time_policy)/sizeof(DEV_DEFEND_TIME_INFO_t)); i++)
    {
        if(SUCCESS != dev_check_defend_time(&info_p->record_time_policy[i]))
        {
            LOGE( "the week %d record_time_policy error", i);
            return DEV_PARAM_INVALID;
        }
    }
    
    memcpy(rec, info_p, sizeof(DEV_RECORD_CFG_t));
    send_record_task_msg(0, LOCAL_MSG_REC_TIME_INFO,
                    (char*)info_p, sizeof(DEV_RECORD_CFG_t));
    save_record_info();
    return SUCCESS;
}
#else
int dev_set_record_cfg(DEV_RECORD_CFG_t *info_p)
{
    DEV_RECORD_CFG_t *rec;
    int vich = info_p->vich;
    int i, j;

    CHECK_VIDEO_IN_CHANNEL(vich);

    rec = &g_rec_cfg[vich];

    for(j = 0; j < (int)(sizeof(info_p->record_time_policy_list)/sizeof(DEV_DEFEND_TIME_LIST_INFO_t)); j++)
    {
        // info_p->record_time_policy_list[j].en = 
        for(i = 0; i < (int)(sizeof(info_p->record_time_policy_list[j].record_time_policy)/sizeof(DEV_DEFEND_TIME_INFO_t)); i++)
        {
            if(SUCCESS != dev_check_defend_time(&info_p->record_time_policy_list[j].record_time_policy[i]))
            {
                LOGE( "the week %d record_time_policy error", i);
                return DEV_PARAM_INVALID;
            }
        }
    }
    
    memcpy(rec, info_p, sizeof(DEV_RECORD_CFG_t));
    send_record_task_msg(0, LOCAL_MSG_REC_TIME_INFO,
                    (char*)info_p, sizeof(DEV_RECORD_CFG_t));
    save_record_info();
    return SUCCESS;
}
#endif

static void set_record_default_info()
{
    int i;
    int j;
    int k;
    for(i = 0; i < MAX_VI_CHANNEL_NUM; i++)
    {
        memset(&g_rec_cfg[i],0,sizeof(g_rec_cfg[i]));
        g_rec_cfg[i].vich = i;
        g_rec_cfg[i].auto_del = 1;
        g_rec_cfg[i].rec_time = 5;
        g_rec_cfg[i].vech = 0;
        g_rec_cfg[i].storage_type= 0;//DISK_SD
        g_rec_cfg[i].time_type = 1;//default,local time

        for(j = 0; j < (int)(sizeof(g_rec_cfg->record_time_policy_list)/sizeof(DEV_DEFEND_TIME_LIST_INFO_t)); j++)
        {
            for(k = 0; k < (int)(sizeof(g_rec_cfg->record_time_policy_list[j].record_time_policy)/sizeof(DEV_DEFEND_TIME_INFO_t)); k++)
            {
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].en = 0;

                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].start1.hour = 0;
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].start1.min = 0;
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].start1.sec = 0;
                
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].end1.hour = 23;
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].end1.min = 59;
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].end1.sec = 59;

                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].start2.hour = 0;
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].start2.min = 0;
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].start2.sec = 0;
                
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].end2.hour = 23;
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].end2.min = 59;
                g_rec_cfg[i].record_time_policy_list[j].record_time_policy[k].end2.sec = 59;
            }
        }
    }

    // 默认连续录像，每天，000000-235959
    g_rec_cfg[0].enable = 1; //录像计划总开关
    g_rec_cfg[0].recMode = 0; //模式
    g_rec_cfg[0].record_time_policy_list[0].en = 1;
    for(k = 1; k < (int)(sizeof(g_rec_cfg->record_time_policy_list[j].record_time_policy)/sizeof(DEV_DEFEND_TIME_INFO_t)); k++)
    {
        g_rec_cfg[0].record_time_policy_list[0].record_time_policy[k].en = 1;
    }
}

#if 0
static RESULT_t read_record_info()
{
    unsigned int i;
    if(TRUE == OpenIniFile(g_record_cfg_file))
    {
        for(i=0; i<sizeof(g_record_cfg_info)/sizeof(INI_HEAD_INFO_t); i++)
        {
            if(FAIL == read_param(&g_record_cfg_info[i]))
            {
                LOGD( "read param end");
            }
        }
        CloseIniFile();
        return SUCCESS;
    }
    return FAIL;
}


static RESULT_t save_record_info()
{
    unsigned int i;
    RESULT_t ret = SUCCESS;


    int version = get_ini_cfg_version(g_record_cfg_file);

    for(i = 0; i < sizeof(g_record_cfg_info)/sizeof(INI_HEAD_INFO_t);i++)
    {
        if(FAIL ==write_param(&g_record_cfg_info[i]))
        {
            LOGI("save param end");
        }
    }
    
    reserve_ini_cfg_version(version); 
    if(FALSE == WriteIniFile(g_record_cfg_file))
    {
        LOGI("write ini file:%s fail",g_record_cfg_file);
        ret = FAIL;
    }
    CloseIniFile ();
    return ret;
}

#else
static RESULT_t save_record_info()
{
    unsigned int i;
    RESULT_t ret = SUCCESS;
    void* handle;

    int version = get_ini_cfg_version(g_record_cfg_file);
    
    handle = OpenIniHandle();
    if(!handle)
    {
        LOGE("OpenIniHandle: FAIL");
    }
    
    for (i = 0; i < sizeof(g_record_cfg_info) / sizeof(INI_HEAD_INFO_t); i++)
    {
        if (FAIL == inihandle_write_param(handle, &g_record_cfg_info[i]))
        {
            LOGI("key:%s", g_record_cfg_info[i].keyword);
            LOGI("save param end");
        }
    }
    if (FALSE == IniHandle_WriteIniFile(handle, g_record_cfg_file))
    {
        ret = FAIL;
    }
    CloseIniHandle(handle);

    return ret;
}

static RESULT_t read_record_info()
{
    unsigned int i;
    void* handle;
   
    if ((handle = OpenIniFileHandle(g_record_cfg_file)))
    {
        for (i = 0; i < sizeof(g_record_cfg_info) / sizeof(INI_HEAD_INFO_t); i++)
        {
            if (FAIL == inihandle_read_param(handle, &g_record_cfg_info[i]))
            {
                LOGI("read param end");
            }
        }
        CloseIniHandle(handle);
        return SUCCESS;
    }
    else
    {
        LOGW("open inifile:%s fail", g_record_cfg_file);
    }
    return FAIL;
}
#endif

RESULT_t init_record_info(char *config_root)
{
    char log_path[64];
    char *log_name = "recorder.log";
    int log_flag = 1;
    int i, j;
    int ini_update = 0;
    int ret = 0;

    memset(&g_rec_cfg, 0, sizeof(g_rec_cfg));
    for(i = 0; i < MAX_VI_CHANNEL_NUM; i++)
    {
        g_rec_cfg[i].time_type = 1;
    }
    
    LOGI("init_video_record_info config_root:%s",
        config_root ? (config_root[0] ? config_root : "null") : "null");
    if(!config_root || !config_root[0])
    {
        #if 1
        ini_config_def("./");
        sprintf(g_record_cfg_file, "%s/%s", get_var_config_path(), DEFAULT_RECORD_CFG_FILE);
        sprintf(log_path, "%s/log/", get_var_root_path());
        LOGI("record cfg path:%s", g_record_cfg_file);
        LOGI("record log path:%s", log_path);
        #else
        sprintf(g_record_cfg_file, "%s/%s", CONFIG_PATH, DEFAULT_RECORD_CFG_FILE);
        sprintf(log_path, "%s/log/", CONFIG_PATH);
        #endif
    }
    else
    {
        if(config_root[strlen(config_root)-1] != '/')
        {
            sprintf(g_record_cfg_file, "%s%s%s", config_root, "/", DEFAULT_RECORD_CFG_FILE);
            sprintf(log_path, "%s/log/", config_root);
        }
        else
        {
            sprintf(g_record_cfg_file, "%s/%s", config_root, DEFAULT_RECORD_CFG_FILE);
            sprintf(log_path, "%slog/", config_root);
        }
    }

    if(0 != access(log_path, F_OK))
    {
        if(mkdir(log_path, 0755) < 0)
        {
            log_flag = 0;
        }
    }
    
#if RECORDER_LOG
    if(0 == init_logs_file(log_name))
    {
        g_recorder_log_flag = 1;
    }
    else 
    {
        g_recorder_log_flag = 0;
    }

#else
    if(log_flag)
    {
        sprintf(log_path + strlen(log_path), "%s", log_name);
        LOGI("init_logs_mgr log_path:%s", log_path);
        init_logs_mgr(log_path, 2*1024);
    }
#endif
    
	ret = replace_default_cfg_file(g_record_cfg_file_vendor, g_record_cfg_file);
    if(ret < 0)
    {
    	ret = replace_default_cfg_file(g_record_cfg_file_default, g_record_cfg_file);
        LOGI("FILE NOT EXIST:%s", g_record_cfg_file_vendor);
    }
    
        LOGI("[lwj test]1 ----------------------------------");
    if(FAIL == read_record_info())
    {
        LOGI("[lwj test]2 ----------------------------------");
        set_record_default_info();
        if(FAIL == save_record_info())
        {
            LOGE("WriteIniFile : %s FAILD",g_record_cfg_file);
            return FAIL;
        }
    }

    for(i = 0; i < MAX_VI_CHANNEL_NUM; i++)
    {
        if(0 != g_rec_cfg[i].pkt_tm_options[0])
        {
            for(j = 0; j < (int)sizeof(g_rec_cfg[0].pkt_tm_options); j++)
            {
                if(0 == g_rec_cfg[i].pkt_tm_options[j])
                {
                    break;
                }
                
                // LOGI("vich:%d j:%d val:%d", i, j, g_rec_cfg[i].pkt_tm_options[j]);
            }
            g_rec_cfg[i].pkt_tm_opt_num = j;
        }
        else
        {
            for(j = 0; j < (int)(sizeof(DEFAULT_PKT_TM_OPT)/sizeof(DEFAULT_PKT_TM_OPT[0])); j++)
            {
                g_rec_cfg[i].pkt_tm_options[j] = DEFAULT_PKT_TM_OPT[j];
                // LOGI("vich:%d j:%d val:%d", i, j, g_rec_cfg[i].pkt_tm_options[j]);
            }
            g_rec_cfg[i].pkt_tm_opt_num = sizeof(DEFAULT_PKT_TM_OPT)/sizeof(DEFAULT_PKT_TM_OPT[0]);

            ini_update = 1;
        }
        
        if(g_rec_cfg[i].rec_time >= g_rec_cfg[i].pkt_tm_opt_num)
        {
            g_rec_cfg[i].rec_time = g_rec_cfg[i].pkt_tm_opt_num - 1;
            ini_update = 1;
        }
        
    	LOGI("vich:%d,time_type=%d,auto_del=%d,rec_time=%d,vich=%d",
    		i,g_rec_cfg[i].time_type,g_rec_cfg[i].auto_del,g_rec_cfg[i].rec_time,g_rec_cfg[i].vich);
    }

    if(ini_update)
    {
        LOGI("save_record_info");
        save_record_info();
    }

    // LOGI("type:%d, %02d:%02d:%02d - %02d:%02d:%02d", g_rec_cfg[0].recMode,
    //                                                 g_rec_cfg[0].record_time_policy[0].start1.hour,
    //                                                 g_rec_cfg[0].record_time_policy[0].start1.min,
    //                                                 g_rec_cfg[0].record_time_policy[0].start1.sec,
    //                                                 g_rec_cfg[0].record_time_policy[0].end1.hour,
    //                                                 g_rec_cfg[0].record_time_policy[0].end1.min,
    //                                                 g_rec_cfg[0].record_time_policy[0].end1.sec);

    
    return SUCCESS;
}

/****文件时间类型，0-GMT,1-LOCAL***/
int get_recorder_time_type(int Vich)
{
	if((Vich < 0) || (Vich > MAX_VI_CHANNEL_NUM))
		return 1;
		
	return g_rec_cfg[Vich].time_type;
}

#if 0
/**用于测试录像回收功能**/
int get_reserved_size_from_file_for_test()
{
    int size = 0;
    if(access(TEST_DISK_RESERVED_FILE,F_OK)<0)
    {
        LOGI("%s not exit!",TEST_DISK_RESERVED_FILE);
        return size;
    }

    if(TRUE == OpenIniFile(TEST_DISK_RESERVED_FILE))							
    {																									
        size = ReadInt("disk","size",0);			
																						
        CloseIniFile();
    }
    return size;	
}
#else
/**用于测试录像回收功能**/
int get_reserved_size_from_file_for_test()
{
    int size = 0;
    if(access(TEST_DISK_RESERVED_FILE,F_OK)<0)
    {
        LOGI("%s not exit!",TEST_DISK_RESERVED_FILE);
        return size;
    }

    // if(TRUE == OpenIniFile(TEST_DISK_RESERVED_FILE))							
    // {																									
    //     size = ReadInt("disk","size",0);			
																						
    //     CloseIniFile();
    // }

    void *handle = NULL;
    if((handle = OpenIniFileHandle(TEST_DISK_RESERVED_FILE)))
    {
        if(0 != IniHandle_ReadInt(handle, "disk", "size", &size)) {
            CloseIniHandle(handle);
            return 0;
        }
        CloseIniHandle(handle);
    }
    else
    {
        LOGE("ini_file_path:%s OpenIniFileHandle fail", TEST_DISK_RESERVED_FILE);
        return 0;
    }

    return size;	
}
#endif


//end------------------------record info------------------------------------

