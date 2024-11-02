/**
* created by yys(Vincent.Yeh)
*/
// #include "com_lib_api.h"
#include "os_ini.h"
#include "mempool.h"
#include "msg_queue.h"
#include "event_handle.h"
#include "task_mgr.h"
#include "frame_buff.h"
#include "logServer.h"
#include "streamshmmgr.h"
#include "atomic.h"
#include "sepcam_recorder.h"
#include "sepcam_ipc_server.h"
#include "sepcam_ipc_internal.h"
#include "av_packet_ps.h"

#include "recorder_defs.h"
// add by lwj
#include "file_list.h"  
// extern VideoFileInfo_t gVideoFileInfo;
// add end by lwj

#include "recorder_utils.h"

#define PLAYBACK_DEBUG

typedef enum
{
    WALLCLOCK_INIT,
    WALLCLOCK_START,
    WALLCLOCK_PAUSE,
    WALLCLOCK_GET,
    WALLCLOCK_SPEED_1X,
    WALLCLOCK_SPEED_2X,
    WALLCLOCK_SPEED_4X,
    WALLCLOCK_SPEED_HALFX,
    WALLCLOCK_SPEED_QUALX
} WallClockStatus;

typedef struct _WallClock_t_
{
    unsigned int ms;
    int speed;
    int slow;
    struct timeval sysTime;
    WallClockStatus status;
} WallClock_t;

static int getTimevalDiff(struct timeval x , struct timeval y)
{
    int x_ms , y_ms , diff;

    x_ms = x.tv_sec*1000 + x.tv_usec/1000;
    y_ms = y.tv_sec*1000 + y.tv_usec/1000;

    diff = y_ms - x_ms;

    return diff;
}

//time:y-x
static int getClockTimevalDiff(struct timespec x , struct timespec y)
{
    int x_ms , y_ms , diff;

    x_ms = x.tv_sec*1000 + x.tv_nsec/(1000*1000);
    y_ms = y.tv_sec*1000 + y.tv_nsec/(1000*1000);

    diff = y_ms - x_ms;

    return diff;

}

static int my_gettimeofday(struct timeval *time, void* dump)
{
	struct timespec time_now;
	clock_gettime(CLOCK_MONOTONIC,&time_now);
	time->tv_sec = time_now.tv_sec;
	time->tv_usec = time_now.tv_nsec/1000;
	return 0;
}

static unsigned int WallClock(WallClock_t *clock, WallClockStatus status)
{
    struct timeval now;
    unsigned int pauseTime = 0;
    
    my_gettimeofday(&now, NULL);
    
    if(status == WALLCLOCK_INIT){
        memset(clock, 0, sizeof(WallClock_t));
        clock->status = WALLCLOCK_INIT;
        clock->speed = 1;
        clock->slow = 1;
    }
    else if(status == WALLCLOCK_START){
        if(clock->status == WALLCLOCK_INIT){
            memcpy(&clock->sysTime, &now, sizeof(struct timeval));
            clock->status = WALLCLOCK_START;
            clock->ms = 0;
        }
        else if(clock->status == WALLCLOCK_PAUSE){
            pauseTime = (unsigned int)getTimevalDiff(clock->sysTime, now);
            memcpy(&clock->sysTime, &now, sizeof(struct timeval));
            clock->status = WALLCLOCK_START;
            return pauseTime;
        }
    }
    else if(status == WALLCLOCK_PAUSE){
        if(clock->status == WALLCLOCK_START){
            clock->ms += (((unsigned int)getTimevalDiff(clock->sysTime, now)*clock->speed))/clock->slow;
            memcpy(&clock->sysTime, &now, sizeof(struct timeval));
            clock->status = WALLCLOCK_PAUSE;
        }
        return clock->ms;
    }
    else if(status == WALLCLOCK_GET){
        if(clock->status == WALLCLOCK_START){
            clock->ms += (((unsigned int)getTimevalDiff(clock->sysTime, now)*clock->speed))/clock->slow;
            memcpy(&clock->sysTime, &now, sizeof(struct timeval));
            //LOGI("clock->ms:%d", clock->ms);
        }
        return clock->ms;
    }
    else if(status == WALLCLOCK_SPEED_1X){
        clock->speed = 1;
        clock->slow = 1;
    }
    else if(status == WALLCLOCK_SPEED_2X){
        clock->speed = 2;
        clock->slow = 1;
    }
    else if(status == WALLCLOCK_SPEED_4X){
        clock->speed = 4;
        clock->slow = 1;
    }
    else if(status == WALLCLOCK_SPEED_HALFX){
        clock->speed = 1;
        clock->slow = 2;
    }
    else if(status == WALLCLOCK_SPEED_QUALX){
        clock->speed = 1;
        clock->slow = 4;
    }
    return 0;
}

static int WallClockSpeed(WallClock_t *clock, int speed, int slow)
{
    clock->speed = speed;
    clock->slow = slow;
    return 0;
}

typedef struct
{
    int audioTimestamp;
    int videoTimestamp;
    int playTime;
}REPLAY_RECORD_INFO_t;

#define VIDEO_BUF_SIZE	(1024 * 400)        // Video buffer size , Using at Streamout_VideoThread
#define AUDIO_BUF_SIZE	(1024)              // Audio buffer size , Using at Streamout_AudioThread

static int write_frame2shm(frame_buff_t *frame_p, char *stream_shm)
{
#define FRAME_PKT_MAX_NUM 80
    struct frame_buff *instance;
    STREAM_FRAME_INFO_t frameinfo;
    STREAM_FRAME_DATA_t framedata;
    STREAM_FRAME_PACK_t pkt[FRAME_PKT_MAX_NUM];
    int i = 0;
    int ret;

    memset(pkt, 0, sizeof(pkt));
    
    memset(&frameinfo, 0, sizeof(frameinfo));
    frameinfo.vench = 0;
    if(FRAME_TYPE_IS_VIDEO(frame_p->frame_type))
    {
        LOGV("FRAME_TYPE_IS_VIDEO frame_type=%d", frameinfo.frame_type);
        frameinfo.payload = 96;
        frameinfo.frame_type = frame_p->frame_type & 0xff;
        if(FRAME_TYPE_IS_V_KEY(frame_p->frame_type))
        {
            frameinfo.key = 1;
        }
    }
    else
    {
        LOGV("FRAME_TYPE_IS_AUDIO frame_type=%d", frameinfo.frame_type);
        frameinfo.payload = 19;
        frameinfo.frame_type = 2;
    }
    frameinfo.frame_no = frame_p->frame_no;
    frameinfo.frame_size = frame_p->used_len;
    frameinfo.timestamp = frame_p->timestamp;

    instance = frame_p->instance;

    //add by lwj
    frameinfo.vench = frame_p->frame_ch;
    // LOGI("[write_frame2shm]frameinfo.vench=%d\n", frameinfo.vench);
    //add end by lwj
    while(instance)
    {
        //LOGI("i:%d instance->data:%p instance->len:%d", i, instance->data, instance->len);
        pkt[i].addr = instance->data;
        pkt[i].pkt_size = instance->len;
        instance = __frame_buff_next(instance);
        i++;
        //LOGI("i:%d instance:%p", i, instance);
    }

    if(i > FRAME_PKT_MAX_NUM)
    {
        LOGE("i:%d > FRAME_PKT_MAX_NUM:%d", i, FRAME_PKT_MAX_NUM);
        return -1;
    }
	
    memset(&framedata, 0, sizeof(framedata));
    framedata.pkt_num = i;
    framedata.pkt = pkt;
    LOGV("ipcam_write_enc_streamshm<<<<<<<<<<<<<");
    ret = ipcam_write_enc_streamshm(stream_shm, &frameinfo, &framedata);
    LOGV("ipcam_write_enc_streamshm>>>>>>>>>>>>>");
    return ret;
}

struct PlayTime_st
{
    struct PlayTime_st *next;
    frame_buff_t *frame;
    int playTime;
};

static INLINE_t void debug_playtimeArr(struct PlayTime_st *idle_list, struct PlayTime_st *play_list)
{
    struct PlayTime_st *next = play_list->next;
    int i = 0;
    while(next)
    {
        LOGI("play list i:%d playtime:%d", i, next->playTime);
        i++;
        next = next->next;
    }
    
    next = idle_list->next;
    i = 0;
    while(next)
    {
        LOGI("idle list i:%d playtime:%d", i, next->playTime);
        i++;
        next = next->next;
    }
}

#define PLAYTIME_ARR_NUM 4

static void init_playTimeArr(struct PlayTime_st *idle_list, struct PlayTime_st *play_list, struct PlayTime_st playTimeArr[])
{
    int i;
    idle_list->next = playTimeArr;
    play_list->next = NULL;
    for(i = 0; i < PLAYTIME_ARR_NUM - 1; i++)
    {
        playTimeArr[i].next = &playTimeArr[i+1];
        playTimeArr[i].frame = NULL;
        playTimeArr[i].playTime = 0;
    }
    playTimeArr[PLAYTIME_ARR_NUM-1].next = NULL;
    playTimeArr[PLAYTIME_ARR_NUM-1].frame = NULL;
    playTimeArr[PLAYTIME_ARR_NUM-1].playTime = 0;
    
    //debug_playtimeArr(idle_list, play_list);
}

static INLINE_t void insert_idleTime(struct PlayTime_st *idle_list, struct PlayTime_st *new_Playgime)
{
    new_Playgime->next = idle_list->next;
    idle_list->next = new_Playgime;
}

static INLINE_t void insert_playTime(struct PlayTime_st *play_list, struct PlayTime_st *new_Playgime)
{
    struct PlayTime_st *priv = play_list;
    struct PlayTime_st *next;

    while((next = priv->next))
    {
        if(next->playTime > new_Playgime->playTime)
        {
            priv->next = new_Playgime;
            new_Playgime->next = next;
            return;
        }
        priv = next;
    }
    priv->next = new_Playgime;
    new_Playgime->next = NULL;
}

static INLINE_t struct PlayTime_st *pop_playTime(struct PlayTime_st *play_list)
{
    struct PlayTime_st *ret = play_list->next;
    if(ret)
    {
        play_list->next = ret->next;
        ret->next = NULL;
    }
    return ret;
}

static INLINE_t void clear_playTime(struct PlayTime_st *idle_list, struct PlayTime_st *play_list)
{
    struct PlayTime_st *playTime;
    while((playTime = pop_playTime(play_list)))
    {
        if(playTime->frame)
        {
            free_frame(playTime->frame);
            playTime->playTime = 0;
            playTime->frame = NULL;
        }
        insert_idleTime(idle_list, playTime);
    }
}

static INLINE_t void insert_and_pop_playTime(struct PlayTime_st *idle_list, struct PlayTime_st *play_list,
                frame_buff_t *frame, int playTime, struct PlayTime_st *getPlayTime)
{
    // LOGI("enter insert_and_pop_playTime");
    // if(NULL == idle_list) {
    //     LOGI("NULL == idle_list");
    // }
    // LOGI("NULL != idle_list");

    struct PlayTime_st *new_Playgime = idle_list->next;

    if(!frame)
    {
        LOGE("frame is NULL");
        return;
    }
    
    getPlayTime->frame = NULL;
    getPlayTime->next = NULL;
    getPlayTime->playTime = 0;
    
    if(new_Playgime)
    {
        idle_list->next = new_Playgime->next;
        new_Playgime->frame = frame;
        new_Playgime->playTime = playTime;
        new_Playgime->next = NULL;
        //LOGI("insert_playTime before");
        //debug_playtimeArr(idle_list, play_list);
        insert_playTime(play_list, new_Playgime);
        //LOGI("insert_playTime after");
    }
    else
    {
        new_Playgime = pop_playTime(play_list);
        getPlayTime->frame = new_Playgime->frame;
        getPlayTime->playTime = new_Playgime->playTime;
        getPlayTime->next = NULL;

        new_Playgime->frame = frame;
        new_Playgime->playTime = playTime;
        new_Playgime->next = NULL;
        insert_playTime(play_list, new_Playgime);
    }

    //debug_playtimeArr(idle_list, play_list);
    return;
}

// #define LWJ_TEST
void *thread_PlayBack_serv(void *arg)
{
    AV_RECORDER_SERVER_t *av_rec_serv_p;
	int recorder_id = *((int *)arg);
	IPCNET_RECORD_COMMAND_e playback_status = 0;
	int playback_flag, block_flag;
	int speed_flag = 0, play_speed = 1, play_slow = 1;
	// int seek_flag = 0, seek_secs = 0, dur_secs = 0;
    int seek_flag = 0, seek_secs = 0, offset_bytes = 0;
    
    char stream_shm[32];
    STREAM_FRAME_INFO_t frameinfo;
    STREAM_FRAME_DATA_t framedata;
    STREAM_FRAME_PACK_t pkt[5];
    uint32_t vframe1_no = 0,vframe2_no = 0, aframe_no = 0;  //change by lwj

    struct PlayTime_st stPlayTime;
    frame_buff_t *frame_p = NULL;
    int fd;

	struct timespec time_st = {0,0};
	struct timespec time_end = {0,0};
    int sleepTime = 0, nowTime = 0, playTime = 0;
    int audio_playTime = 0, video_playTime = 0;
    int audioTimestampOrg = -1, videoTimestampOrg = -1;
    WallClock_t clock;
	//WallClockStatus clockStatus = WALLCLOCK_SPEED_1X;
    int enc_type;

	free(arg);
    LOGV("recorder_id:%d", recorder_id);
	
    av_rec_serv_p = get_av_recorder_serv(recorder_id);
    if(!av_rec_serv_p)
    //按设计不应该出现在这�?
    {
        LOGF("!!!get_av_recorder_serv FAIL:%d", av_rec_serv_p);
    	pthread_exit(0);
    }

    mutex_lock(&av_rec_serv_p->lock);
    playback_flag = av_rec_serv_p->flag;
    fd = av_rec_serv_p->fd;
    strncpy(stream_shm, av_rec_serv_p->stream_shm, sizeof(stream_shm));
    stream_shm[sizeof(stream_shm)-1] = '\0';
    mutex_unlock(&av_rec_serv_p->lock);
    
    if(!playback_flag)
    {
        LOGE("get_av_recorder_serv id:%d flag:%d ERROR    ",
            recorder_id, av_rec_serv_p->flag, av_rec_serv_p->status);
        pthread_exit(0);
    }

    /*注意！！
    * 这里休眠一下是为了客户端那边也创建好共享内存，
    * 否则这边先写录像那边才创建好共享内存，会把前面的帧刷�?
    */
    // int sleep_sec = 2;
    //LOGI("sleep_sec:%d <<<<<<<<<<", sleep_sec);
    // sleep(sleep_sec);
    //LOGI("sleep_sec:%d >>>>>>>>>>", sleep_sec);

    struct PlayTime_st playTimeArr_idle_list = {0, 0, 0};
    struct PlayTime_st playTimeArr_play_list = {0, 0, 0};
    struct PlayTime_st playTimeArr[PLAYTIME_ARR_NUM] = {{0, 0, 0}};

    struct PlayTime_st *idle_list = &playTimeArr_idle_list;
    struct PlayTime_st *play_list = &playTimeArr_play_list;
    init_playTimeArr(idle_list, play_list, playTimeArr);
    
    while(1)
    {
        MyTime time_s;
        MyDate date_s;
        //clock_gettime(CLOCK_MONOTONIC, &time_st);
        
        // LOGI("mutex_lock<<<<<<<<<<<<recorder_id:%d playback_status:%d", recorder_id, playback_status);
        mutex_lock(&av_rec_serv_p->lock);
        // LOGI("mutex_lock>>>>>>>>>>>>>>>>>recorder_id:%d playback_status:%d", recorder_id, playback_status);

        // added by lishun
        date_s = av_rec_serv_p->date_s;
        time_s = av_rec_serv_p->time_s;
        // end by lishun

        playback_status = av_rec_serv_p->status;
        block_flag = av_rec_serv_p->block;
        
        //speed
        speed_flag = av_rec_serv_p->speed_flag;
        if(speed_flag)
        {
            play_speed = av_rec_serv_p->play_speed;
            play_slow = av_rec_serv_p->play_slow;
            av_rec_serv_p->speed_flag = 0;
        }
        
        //seek
        seek_flag = av_rec_serv_p->seek_flag;
        if(seek_flag)
        {
            #if 1   //add by lwj
            fd = av_rec_serv_p->fd;
            // videoTimestampOrg = audioTimestampOrg = -1;
            printf("\n\n[lwj test]av_rec_serv_p->fd=%d\n\n", fd);
            #endif

            seek_secs = av_rec_serv_p->seek_secs;
            // dur_secs = av_rec_serv_p->dur_secs;
            offset_bytes = av_rec_serv_p->offset;
            av_rec_serv_p->seek_flag = 0;        
        }
        //LOGI("mutex_unlock<<<<<<<<<<<<recorder_id:%d playback_status:%d", recorder_id, playback_status);
        mutex_unlock(&av_rec_serv_p->lock);
        //LOGI("mutex_unlock>>>>>>>>>>>>>>>>>recorder_id:%d playback_status:%d", recorder_id, playback_status);

        //LOGI("recorder_id:%d playback_status:%d", recorder_id, playback_status);
        if(playback_status == IPCNET_CMD_RECORD_STOP)
        {
            LOGI("IPCNET_CMD_RECORD_STOP");
            goto THREAD_PLAYBACK_END;
        }
        else if(playback_status == IPCNET_CMD_RECORD_PAUSE)
        {
            WallClock(&clock, WALLCLOCK_PAUSE);
            LOGI("IPCNET_CMD_RECORD_PAUSE");
            sleep(2);
            continue;
        }
        else if(playback_status == IPCNET_CMD_RECORD_PLAY)
        {
            WallClock(&clock, WALLCLOCK_START);
        }

        /*
        *   init wall clock
        */
        if(speed_flag)
        {
            WallClock(&clock, WALLCLOCK_INIT);
            WallClockSpeed(&clock, play_speed, play_slow);
            WallClock(&clock, WALLCLOCK_START);
            // videoTimestampOrg = audioTimestampOrg = -1;
            LOGI("play_speed:%d play_slow:%d", play_speed, play_slow);
        }
        else if(seek_flag)
        {

            if (0 == record_seek(fd, offset_bytes, seek_secs))
            {
                LOGI("record_seek SUCCESS clear_playTime <<");
                clear_playTime(idle_list, play_list);
                WallClock(&clock, WALLCLOCK_INIT);
                WallClock(&clock, WALLCLOCK_START);
                videoTimestampOrg = audioTimestampOrg = -1;
                LOGI("record_seek SUCCESS clear_playTime >>");
            }
        }
        else if(videoTimestampOrg == -1 && audioTimestampOrg == -1)
        {
            WallClock(&clock, WALLCLOCK_INIT);
            WallClockSpeed(&clock, play_speed, play_slow);
            WallClock(&clock, WALLCLOCK_START);
            LOGW("WallClock WALLCLOCK_INIT WALLCLOCK_START play_speed:%d play_slow:%d", play_speed, play_slow);
        }

        /*
        *   start read frame
        */
        frame_p = read_pes2es_packet_frame(fd);
        if(!frame_p)
        {
            LOGI("lwj test err");
            close(fd);

            MyTime time_c, time_n;
            int newfd = -1;
            char recPath[MAX_PATH];

            time_c = time_s;

            char *recDir = get_record_dir_by_date(get_rec_root(), &date_s, false);   // false: not create;
            if(NULL == recDir) {
                LOGE("get_record_dir_by_date fail");
                goto THREAD_PLAYBACK_END;
            }

#ifdef PLAYBACK_DEBUG
            LOGI("recDir: %s", recDir);
#endif

            while(1) {
                int ret = get_next_file_time_by_time(&time_c, &time_n);
                if( ret < 0) {
                    LOGE("get_next_file_time_by_time fail");
                    goto THREAD_PLAYBACK_END;
                }
                else if(0 == ret) { // not across day;
                    char* recName = get_record_name_by_time(&time_n);
#ifdef PLAYBACK_DEBUG
                    LOGI("recName: %s", recName);
#endif
                    sprintf(recPath, "%s/%s.pes", recDir, recName);
                    newfd = open(recPath, O_RDONLY);
                    if(newfd < 0)
                    {
                        LOGE("open:%s fail", recPath);
                        time_c = time_n;
                        continue;
                    }

                    break;
                }
                else {      // across day;
#ifdef PLAYBACK_DEBUG
                    LOGI("across day");
#endif
                    newfd = -1;
                    goto THREAD_PLAYBACK_END;
                }
            }

            if(newfd < 0) {
                goto THREAD_PLAYBACK_END;      // quit extra loop 
            }
            
#ifdef PLAYBACK_DEBUG
            LOGI("newfd >= 0");
#endif
            mutex_lock(&av_rec_serv_p->lock);
            av_rec_serv_p->time_s = time_n;
            av_rec_serv_p->fd = newfd;
            av_rec_serv_p->seek_flag = 0;
            mutex_unlock(&av_rec_serv_p->lock);
            continue;
        }

        // insert_and_pop_playTime(idle_list, play_list, frame_p, (int)(frame_p->timestamp/1000), &stPlayTime);
        insert_and_pop_playTime(idle_list, play_list, frame_p, (int)(frame_p->timestamp), &stPlayTime);     //add by lwj

        if(!stPlayTime.frame)
        {
            //LOGI("continue##########################################");
            //debug_playtimeArr(idle_list, play_list);
            continue;
        }
        frame_p = stPlayTime.frame;
        // LOGI("-----------[lwj test]frame_ch=%d time=%lld", frame_p->frame_ch, frame_p->timestamp);
        LOGV("video[%d] frame_type[%c]:%d frame No:%d frame size:%d, time:%d(ms)",frame_p->frame_ch,
                frame_p->frame_type > 4 ? 'I' : 'P', frame_p->frame_type, 
                frame_p->frame_no, frame_p->used_len, (int)(frame_p->timestamp));
        
        if(FRAME_TYPE_IS_VIDEO(frame_p->frame_type))
        {
            // add by lwj
            if(frame_p->frame_ch == 0)
            {
                frame_p->frame_no = vframe1_no++;
            }
            else
            {
                frame_p->frame_no = vframe2_no++;
            }
            // add end by lwj
            // frame_p->frame_no = vframe_no++;
            LOGV("video frame_type[%c]:%d frame No:%d frame size:%d, time:%d(ms)",
                    frame_p->frame_type > 4 ? 'I' : 'P', frame_p->frame_type, 
                    frame_p->frame_no, frame_p->used_len, (int)(frame_p->timestamp));
            if(videoTimestampOrg == -1)
            {
                LOGV("write_frame2shm<<<<<<<<<<");
                LOGI("first video frame No:%d frame_type:0x%x frame size:%d",
                    frame_p->frame_no, frame_p->frame_type, frame_p->used_len);
                videoTimestampOrg = frame_p->timestamp;//WallClock(&clock, WALLCLOCK_GET);
            }
            playTime = frame_p->timestamp - videoTimestampOrg;
            LOGV("videoTimestamp frame_p->timestamp:%lld(ms) playTime:%d(ms)", frame_p->timestamp, playTime);
        }
        else
        {
            frame_p->frame_no = aframe_no++;
            LOGV("FRAME_TYPE_IS_AUDIO audioTimestampOrg:%d", audioTimestampOrg);
            if(audioTimestampOrg == -1)
            {
                LOGV("write_frame2shm<<<<<<<<<<");
                LOGI("first audio frame No:%d frame_type:0x%x", frame_p->frame_no, frame_p->frame_type);
                audioTimestampOrg = frame_p->timestamp;//WallClock(&clock, WALLCLOCK_GET);
            }
            
            playTime = frame_p->timestamp - audioTimestampOrg;
            LOGV("audioTimestamp frame_p->timestamp:%lld(ms) playTime:%d(ms)", frame_p->timestamp, playTime);
        }
        
        //playTime = stPlayTime.playTime;
        
        nowTime = WallClock(&clock, WALLCLOCK_GET);
        sleepTime = playTime - nowTime;
        LOGV("nowTime:%d sleepTime:%d<<<<<<<<<<<playTimeArr playTime[%d]",
            nowTime, sleepTime, playTime);
        sleepTime = sleepTime * clock.slow / clock.speed;
        if(sleepTime < 0)
        {
            #ifdef LWJ_TEST
            LOGD("(sleepTime < 0)############nowTime:%d sleepTime:%d playTimeArr playTime[%d]",
                nowTime, sleepTime, playTime);
            #endif
        }
        while(sleepTime > 0)
        {
            if(sleepTime > 2000)
            {
                LOGW("@@@@@@@@@@@@sleepTime:%d too long!!!!!!!!!!!!", sleepTime);
                sleepTime = 2000;
            }
            
            usleep((sleepTime-1)*1000);
            LOGV("sleepTime:%d>>>>>>>>", sleepTime);
            
            nowTime = WallClock(&clock, WALLCLOCK_GET);
            sleepTime = playTime - nowTime;
            LOGV("nowTime:%d sleepTime:%d<<<<<<<<<<<", nowTime, sleepTime);

            //check status
            mutex_lock(&av_rec_serv_p->lock);
            playback_status = av_rec_serv_p->status;
            av_rec_serv_p->status = 0;  //add by lwj
            mutex_unlock(&av_rec_serv_p->lock);

            LOGV("playback_status:%d", playback_status);
            if(IPCNET_CMD_RECORD_STOP == playback_status)
            {
                LOGW("recorder_id:%d IPCNET_CMD_RECORD_STOP", recorder_id);
                free_frame(frame_p);
                frame_p = NULL;
                goto THREAD_PLAYBACK_END;
            }
            if(IPCNET_CMD_RECORD_SEEK == playback_status)
            {
                LOGW("recorder_id:%d IPCNET_CMD_RECORD_SEEK", recorder_id);
                break;
            }
        }

        if(IPCNET_CMD_RECORD_SEEK == playback_status)
        {
            free_frame(frame_p);
            frame_p = NULL;

            continue;
        }

// #ifdef RECORDER_DEBUG
//         LOGI("write_frame2shm recorder_id:%d stream_shm:%s end flag<<<<<<<<<<<<<<<<", recorder_id, stream_shm);
// #endif
        
        int wrshm_ret, wrshm_flag = 0;
        int wrshmErrCnt = 0;//add by lwj
        while(1)
        {
            wrshm_ret = write_frame2shm(frame_p, stream_shm);
            #ifdef LWJ_TEST
            LOGI("wrshm_ret ==%d", wrshm_ret);
            #endif
            if(0 == wrshm_ret)
            {
                wrshmErrCnt++;
                if (wrshmErrCnt>50)
                {
                    LOGI("write_frame2shm break");
                    break;
                }
                
                LOGI("wrshm_ret == 0");
                usleep(100*1000);
                        
                mutex_lock(&av_rec_serv_p->lock);
                playback_status = av_rec_serv_p->status;
                mutex_unlock(&av_rec_serv_p->lock);

                //LOGI("recorder_id:%d playback_status:%d", recorder_id, playback_status);
                if(playback_status != IPCNET_CMD_RECORD_PLAY)
                {
                    LOGI("playback_status != IPCNET_CMD_RECORD_PLAY");
                    break;
                }

                wrshm_flag = 1;
            }
            else
            {
                if(wrshm_ret < 0)
                {
                    LOGE("wrshm_ret:%d", wrshm_ret);
                }
                if(wrshm_flag)
                {
                    WallClock(&clock, WALLCLOCK_INIT);
                    WallClock(&clock, WALLCLOCK_START);
                    videoTimestampOrg = audioTimestampOrg = -1;
                }
                break;
            }
        }
        //LOGI("write_frame2shm recorder_id:%d stream_shm:%s end flag>>>>>>>>>>>>", recorder_id, stream_shm);
        free_frame(frame_p);
        frame_p = NULL;
        //LOGI("free_frame");
    }

THREAD_PLAYBACK_END:
    
    LOGI("REPLAY_RECORDER_INIT_END recorder_id:%d", recorder_id);

    if(frame_p)
    {
        free_frame(frame_p);
        frame_p = NULL;
    }
    
    //debug_playtimeArr(idle_list, play_list);
    
    clear_playTime(idle_list, play_list);
    
    //debug_playtimeArr(idle_list, play_list);

    if(playback_status != IPCNET_CMD_RECORD_STOP)
    {
        //写个结束帧通知客户端结�?
        memset(&framedata, 0, sizeof(STREAM_FRAME_DATA_t));
        framedata.pkt_num = 1;
        pkt[0].addr = NULL;
        pkt[0].pkt_size = 0;
        framedata.pkt = pkt;

        frameinfo.frame_size = 0;
        frameinfo.flag = 1;	
        frameinfo.key = 1;
        LOGI("ipcam_write_enc_streamshm:%s", stream_shm);
        ipcam_write_enc_streamshm(stream_shm, &frameinfo, &framedata);
        //LOGI("stream_shm:%s end flag>>>>>>>>>>>>>>>>", stream_shm);
    }
    //LOGI("release_ipcam_server_streamshm:%s <<<<<<<<<<<<", stream_shm);
    release_ipcam_server_streamshm(stream_shm);
    //LOGI("release_ipcam_server_streamshm:%s >>>>>>>>>>>>", stream_shm);

    mutex_lock(&av_rec_serv_p->lock);
    LOGI("IPCNET_CMD_RECORD_STOP flag:%d event_id:0x%x fd:%d",
         av_rec_serv_p->flag, av_rec_serv_p->event_id, av_rec_serv_p->fd);
    av_rec_serv_p->flag = 0;
    av_rec_serv_p->event_id = 0;
    close(av_rec_serv_p->fd);
    av_rec_serv_p->fd = INVAILD_HANDLE_VALUE;
    strcpy(stream_shm, av_rec_serv_p->stream_shm);
    av_rec_serv_p->stream_shm[0] = '\0';
    mutex_unlock(&av_rec_serv_p->lock);

    pthread_exit(0);
}

int create_replay_recorder_task(int recorder_id)
{
	int *sid = (int *)malloc(sizeof(int));
	*sid = recorder_id;
	pthread_t ThreadID;
	int ret;

	LOGI("recorder_id:%d", recorder_id);
	if((ret = pthread_create(&ThreadID, NULL, &thread_PlayBack_serv, (void *)sid)))
	{
		LOGE("pthread_create ret=%d\n", ret);
		exit(-1);
	}

    pthread_detach(ThreadID);
	return 0;
}