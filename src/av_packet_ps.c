/**
* created by yys(Vincent.Yeh)
*/
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
#include "av_packet_ps.h"
#include "recorder_es2pes.h"
// #include "com_hw_type.h"
// #include "sepcam_misc.h"
#include "recorder_params.h"

// added by lishun
#include "recorder_defs.h"
// end by lishun

// add by lwj
#include "stream_frame_info.h"
// add end by lwj

#define frame_is_key(frame_type) (frame_type >4)
#define CANNOT_FIND_PES_HEAD (-2)
#define SEEK_STEP_SECS (6)

#define DO_NRWITE
//#define WRITE_PES_REC_TEST
#ifdef WRITE_PES_REC_TEST //test
static int test_fd = -1;
#endif

static int audio_fd = -1, video_fd = -1;


//add by lwj test
static unsigned int getTimeStamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec*1000 + tv.tv_usec/1000);
}
//add end by lwj test

static void _test_write_audio(int *data, int len)
{
    if(audio_fd == -1)
    {
        audio_fd = open("/tmp/pes_audio.pm",O_RDWR | O_CREAT, 0666);
    }
    write(audio_fd, data, len);
    return;
}

static void _test_write_video(int *data, int len)
{
    if(video_fd == -1)
    {
        video_fd = open("/tmp/pes_audio.h264",O_RDWR | O_CREAT, 0666);
    }
    write(video_fd, data, len);
    return;
}

static int recorder_build_pes_hdr(STREAM_FRAME_INFO_t *frameinfo, unsigned char *buf)
{
	PES_PRIVATE_DATA_t pri_data;
	T_U8 stream_id = 0;
    uint32_t pts,dts;
	int eslen;
	int vich  = 0;
	int vench = frameinfo->vench;
	
    // if(frameinfo->payload == 96)
    if(PLAYLOAD_IS_VIDEO(frameinfo->payload))    //change by lwj
    {
    	stream_id = 0xe0;
        //add by lwj
        if(vench == 1)
            stream_id = PES_CODE_VIDEO1;
        //add end by lwj

    	
        pri_data.pic_width = 0;//video_info.resolution_size.width;
        pri_data.pic_height = 0;//video_info.resolution_size.height;
		pri_data.frame_rate = 0;//video_info.frame_rate;	

        if(frame_is_key(frameinfo->frame_type))
		{
		    // LOGD("is KEY!!!");
			pri_data.mode = 7;
		}
		else
		{
		    LOGV("is pppppppppppp");
			pri_data.mode = 1;
		}
		pri_data.YUV_Format = 1;
    }
    else
    {
    	stream_id = 0xc0;
    	
		pri_data.pic_width = 0;
		pri_data.pic_height = 0;
		pri_data.frame_rate = 0;
		pri_data.mode = 22;
		pri_data.YUV_Format = 1;
	    LOGV("is audio");
    }
    
	eslen = frameinfo->frame_size;

	memset(buf, 0, PES_HEAD_LEN);

    dts = (uint32_t)(frameinfo->timestamp*9/100);// timestamp
    pts = dts + 3600;
    
    recorder_es2pes(buf, eslen, stream_id, pts, dts, &pri_data);

    return SUCCESS;
}

int record_async_data_buff(int fd, RDWR_BUFF_t *wr_buf)
{
    int ret = 0;

    if(wr_buf->len)
    {
#ifdef DO_NRWITE
        ret = writen(fd, (const void *)wr_buf->buff, wr_buf->len);
#else //test
        ret = wr_buf->len;
#endif
        if(-1 == ret)
        {
            LOGE("writen pes data FAIL need:%d ret:%d", wr_buf->buff_len, ret);
            return FAIL;
        }
    }
    init_rdwr_buff(wr_buf, 0);
    return ret;
}

int record_write_data_buff(int fd, char *data, int len, RDWR_BUFF_t *wr_buf)
{
    char *pos = data;
    int remain = len;
    int ret = 0;
int i=0;
unsigned int time1,time2,time3,time4;
    do
    {
#if 0 /*注释原因：就按照扇区对齐写最好*/
        if(remain > (int)wr_buf->buff_len && 0 == wr_buf->len)
        //如果要写的数据长度大于buff，面且wr_buf没有数据需要更新写入，就直接写了
        {
#ifdef DO_NRWITE
            ret = writen(fd, (const void *)pos, remain);
#else //test
            ret = remain;
#endif
            if(-1 == ret)
            {
                LOGE("writen pes data FAIL need:%d ret:%d", wr_buf->buff_len, ret);
                return FAIL;
            }
            return ret;
        }
#endif
        // ret = write_rdwr_buff_data(wr_buf, pos, remain);
    i++;
     //add buy lwj test
    // time1 = getTimeStamp();
    //add buy lwj test
        ret = push_write_buff_data(wr_buf, pos, remain);
    //add buy lwj test
    // time2 = getTimeStamp();
    // printf("%d4[time:%u] ",i, time2-time1);
    //add buy lwj test
        pos += ret;
        remain -= ret;
        
        if (remain < 0)
        /*算法有问题*/
        {
            LOGF("(remain < 0)");
            return FAIL;
        }
        
        if (CHECK_RDWR_BUFF_FULL(wr_buf))
        {
            LOGV("writen buff_len:%d", wr_buf->buff_len);
#ifdef DO_NRWITE
            ret = writen(fd, (const void *)wr_buf->buff, wr_buf->buff_len);
             //add buy lwj test
            // time3 = getTimeStamp();
            // printf("%d5[time:%u][len=%d] ",i, time3-time2, wr_buf->buff_len);
            //add buy lwj test
#else //test
            ret = wr_buf->buff_len;
#endif
            if(-1 == ret)
            {
                LOGE("writen pes data FAIL need:%d ret:%d", wr_buf->buff_len, ret);
                return FAIL;
            }

            init_rdwr_buff(wr_buf, 0);
            //add buy lwj test
            // time4 = getTimeStamp();
            // printf("%d6[time:%u] ",i, time4-time3);
            //add buy lwj test
        }
    }while(remain);
    // printf("[lwj test]i=%d", i);
    return ret;
}

int wirte_es2pes_packet(int fd, STREAM_FRAME_INFO_t *frameinfo, char *es, int es_len, RDWR_BUFF_t *wr_buf)
{
    unsigned char pes_head[PES_HEAD_LEN];
    int ret;
    
    LOGV("buff_len:%d es_len:%d", wr_buf->buff_len, es_len);
    ret = recorder_build_pes_hdr(frameinfo, pes_head);
    if(ret != SUCCESS)
    {
        LOGE("recorder_build_pes_hdr FAIL");
        return FAIL;
    }
    ret = record_write_data_buff(fd, (char*)pes_head, PES_HEAD_LEN, wr_buf);
    if(-1 == ret)
    {
        LOGE("writen pes head FAIL");
        return FAIL;
    }
    ret = record_write_data_buff(fd, es, es_len, wr_buf);
    if(-1 == ret)
    {
        LOGE("writen pes data FAIL es_len:%d ret:%d", es_len, ret);
        return FAIL;
    }
   
#ifdef WRITE_PES_REC_TEST //test
    if(test_fd > 0)
    {
        ret = writen(test_fd, (const void *)pes_head, PES_HEAD_LEN);
        ret = writen(test_fd, (const void *)es, es_len);
    }
#endif
    return 0;
}

// added by lishun
int open_pes_record(char *filename)
{
    int fd;
    
	fd = open(filename, O_RDWR|O_APPEND|O_SYNC, 0666);
	if(fd < 0)
	{
	    LOGE("fopen:%s FAIL", filename);
	    fd = INVAILD_HANDLE_VALUE;
	}

	return fd;
}
// end by lishun

int create_pes_record(char *filename)
{
    int fd;
    
	fd = open(filename, O_CREAT|O_RDWR|O_TRUNC|O_SYNC, 0666);
	if(fd < 0)
	{
	    LOGE("fopen:%s FAIL", filename);
	    fd = INVAILD_HANDLE_VALUE;
	}

#ifdef WRITE_PES_REC_TEST //test
    if(-1 == test_fd)
    {
        char file[128];
        int len = strlen(filename);
        int i;
        
        for(i = len; i > 0, filename[i - 1] != '/'; i--);

        sprintf(file, "%s_test", &filename[i]);
        test_fd = open(file, O_RDWR | O_CREAT, 0666);
        LOGI("TEST open %s %s", file, test_fd > 0 ? "SUCCESS" : "FAIL");
    }
#endif
	return fd;
}

int stop_pes_record(int handle)
{
    close(handle);
#ifdef WRITE_PES_REC_TEST //test
    close(test_fd);
    test_fd = -1;
#endif
    return 0;
}

frame_buff_t* read_pes2es_packet_frame(int fd)
{
#define READ_PES_HEAD_LEN (PES_HEAD_LEN + 5)
#define READ_BUF_LEN (1024)

    unsigned char pes_head[READ_PES_HEAD_LEN];
    PES_header PES;
    char frame_type;
    frame_buff_t *frame_p;
    int eslen;
    int ret;

    char buf[READ_BUF_LEN];
    int wantlen;
    int tmp_len;

#if 0
    ret = freadn(fp, pes_head, READ_PES_HEAD_LEN);
    if(ret <= 0)
    {
        LOGW("fread END...");
        return NULL;
    }


#if 0
    int ii;
    for(ii = 0; ii < 6; ii++)
    {
        printf("%02x ", pes_head[ii]);
    }
    printf("\n");
#endif
    
    ret = recorder_parser_pes_info(pes_head, &PES, &eslen, &frame_type);
    if(ret != 0)
    {
        LOGE("recorder_parser_pes_info fail");
        return NULL;
    }
#else
    ret = seek_pes_head(fd, &PES, &eslen, pes_head);
    if(ret < 0)
    {
        LOGE("seek_pes_head FAIL");
        return NULL;
    }
    
    ret = readn(fd, &pes_head[PES_HEAD_LEN], 5);
    if(ret <= 0)
    {
        LOGW("fread END...");
        return NULL;
    }
    
    // frame_type = pes_head[PES_HEAD_LEN +4]&0x0f;
    // add by lwj
    frame_type = pes_head[PES_HEAD_LEN -7]&0x0f;
    // LOGI("frame_type:%d", pes_head[PES_HEAD_LEN -7]&0xff);
    // printf("[");
    // for (int i = 0; i < PES_HEAD_LEN +4+1; i++)
    // {
    //    printf("%X ", pes_head[i]);
    // }
    // printf("]\n");
    // add end by lwj
    
    // LOGI("frame_type:%d", pes_head[PES_HEAD_LEN + 4]&0xff);
#endif

    //LOGI("eslen:%d", eslen);
    frame_p = alloc_frame(eslen + 32);
    if(!frame_p)
    {
        LOGE("alloc_frame fail eslen:%d", eslen);
        return NULL;
    }
    
    if(PES.stream_id == 0xc0)
    {
    	// LOGI("---------PES.stream_id:%x", PES.stream_id);
        frame_p->frame_type = ENC_G726<<8;//2<<8;// ENC_G726 == 2
    }
    else
    {
        //add by lwj
        if(PES.stream_id == PES_CODE_VIDEO)
        {
            frame_p->frame_ch = 0;
        }
        else if(PES.stream_id == PES_CODE_VIDEO1)
        {
            frame_p->frame_ch = 1;
        }
        //add end by lwj
        frame_p->frame_type = frame_type;
        //LOGI("recive video");
        if(frame_type > 4)
        {
            LOGV("recive video KEY");
        }
    }
    LOGV("frame_type in[0x%x] frame_type:0x%x",frame_type, frame_p->frame_type);
    frame_p->timestamp = PES.DTS;
    frame_p->timestamp = frame_p->timestamp*100/9; 

    frame_copy_data(frame_p, &pes_head[PES_HEAD_LEN], 5);
    //LOGI("frame_copy_data >>> used_len:%d len:%d", frame_p->used_len, frame_p->len);

    wantlen = eslen - 5;
    while(wantlen)
    {
        tmp_len = (wantlen > READ_BUF_LEN) ? READ_BUF_LEN : wantlen;
        memset(buf, 0, sizeof(buf));
        ret = readn(fd, buf, tmp_len);
        if(ret <= 0)
        {
            LOGE( "val %d wantto read len:%d", ret, tmp_len);
            free_frame(frame_p);
            return NULL;
        }

#if 0
        int ii;
        for(ii = 0; ii < tmp_len + 10; ii++)
        {
            printf("%02x ", buf[ii]);
            if(ii%8 == 7)
            {
                printf("\n");
            }
        }
        printf("\n");
#endif
        //LOGI("wantlen:%d tmp_len:%d", wantlen, tmp_len);
    
        frame_copy_data(frame_p, (unsigned char *)buf, tmp_len);
        //LOGI("frame_copy_data >>> used_len:%d len:%d", frame_p->used_len, frame_p->len);
    
    	wantlen -= tmp_len;
    }
    
    return frame_p;
}

static int seek_frame_by_secs(int fd, int secs)
{
#define SEEK_PES_HEAD_LEN (PES_HEAD_LEN + 5)

    unsigned char pes_head[SEEK_PES_HEAD_LEN];
    PES_header PES;
    char frame_type;
    int eslen;
    UINT64_t timestamp;
    long pre_tell = 0;
    int ret;

    while(1)
    {
        ret = readn(fd, pes_head, SEEK_PES_HEAD_LEN);
        if(ret <= 0)
        {
            LOGW("fread END...");
            return FAIL;
        }
        //printf("freadn parser_pes:%02x %02x %02x %02x %02x %02x\n",
        //    pes_head[0], pes_head[1], pes_head[2],
        //    pes_head[3], pes_head[4], pes_head[5]);
        
        ret = recorder_parser_pes_info(pes_head, &PES, &eslen, &frame_type);
        if(ret != 0)
        {
            LOGE("recorder_parser_pes_info fail");
            return FAIL;
        }
        
        if(PES.stream_id != 0xc0 && FRAME_TYPE_IS_V_KEY(frame_type & 0xff))
        {
            pre_tell = lseek(fd, 0, SEEK_CUR) - (SEEK_PES_HEAD_LEN);
            
            timestamp = PES.DTS;
            timestamp = timestamp*100/9; 
            LOGV("timestamp:%lld", timestamp);
            LOGI("[lwj test]timestamp:%lld(ms)", timestamp);

            if((int)(timestamp ) > (secs-2) * 1000)     //change by lwj
            {
                LOGI("timestamp sec:%d pre_tell:%d", (int)(timestamp), pre_tell);
                lseek(fd, pre_tell, SEEK_SET);
                return 0;
            }
            // if((int)(timestamp / 1000) > secs * 1000)
            // {
            //     LOGI("timestamp sec:%d pre_tell:%d", (int)(timestamp / 1000), pre_tell);
            //     lseek(fd, pre_tell, SEEK_SET);
            //     return 0;
            // }
        }
        
        ret = lseek(fd, eslen - 5, SEEK_CUR);
        if(ret < 0)
        {
            LOGE("lseek fail eslen:%d current:%d", eslen, lseek(fd, 0, SEEK_CUR));
            return FAIL;
        }
    }
    return FAIL;
}

static int __seek_frame_by_size(int fd, int start_size, int end_size, PES_header *PES)
{
#define SEEK_FRAME_PES_HEAD_LEN (PES_HEAD_LEN)

    unsigned char pes_head[SEEK_FRAME_PES_HEAD_LEN];
    char frame_type;
    int eslen;
    UINT64_t timestamp;
    long pre_tell = 0;
    int ret;

    ret = readn(fd, pes_head, SEEK_FRAME_PES_HEAD_LEN);
    if(ret <= 0)
    {
        LOGW("fread END...");
        return FAIL;
    }
    start_size += ret;
    
    do
    {
        ret = find_pes_head((char *)&pes_head[0], SEEK_FRAME_PES_HEAD_LEN);
        if(ret >= 0)
        {
            if(ret > 0)
            {
#if 1
                int ii;
                memset(&pes_head[0], 0x11, ret);
                LOGI("memmove BEFOR<<<<<<<<<<<<<<");
                for(ii = 0; ii < 6; ii++)
                {
                    printf("%02x ", pes_head[ii]);
                    if(ii%8 == 7)
                    {
                        printf("\n");
                    }
                }
                printf("\n");
#endif
                memmove(&pes_head[0], &pes_head[ret], SEEK_FRAME_PES_HEAD_LEN - ret);
#if 1
                LOGI("memmove AFTER<<<<<<<<<<<<<<");
                for(ii = 0; ii < 6; ii++)
                {
                    printf("%02x ", pes_head[ii]);
                    if(ii%8 == 7)
                    {
                        printf("\n");
                    }
                }
                printf("\n");
#endif
                ret = readn(fd, &pes_head[SEEK_FRAME_PES_HEAD_LEN - ret], ret);
                if(ret <= 0)
                {
                    LOGW("fread END...");
                    return FAIL;
                }
                start_size += ret;
            }
            
            ret = recorder_parser_pes_info(pes_head, PES, &eslen, NULL);
            if(ret != 0)
            {
                LOGE("recorder_parser_pes_info fail");
                return FAIL;
            }
            
            ret = lseek(fd, -(SEEK_FRAME_PES_HEAD_LEN), SEEK_CUR);
            if(ret != 0)
            {
                LOGE("fseek fail eslen:%d current:%d", eslen, lseek(fd, 0, SEEK_CUR));
                return FAIL;
            }

            LOGI("OK");
            return SUCCESS;
        }

        pes_head[0] = pes_head[SEEK_FRAME_PES_HEAD_LEN - 3];
        pes_head[1] = pes_head[SEEK_FRAME_PES_HEAD_LEN - 2];
        pes_head[2] = pes_head[SEEK_FRAME_PES_HEAD_LEN - 1];
        
        ret = readn(fd, &pes_head[3], SEEK_FRAME_PES_HEAD_LEN - 3);
        if(ret <= 0)
        {
            LOGW("fread END...");
            return FAIL;
        }
        start_size += ret;
    }while(start_size <= end_size);
    
    return CANNOT_FIND_PES_HEAD;
}

static int seek_frame_whence(int fd, int start_off, int end_off, PES_header *PES)
{
#define SEEK_READ_BUF_SIZE (512)
    char data[SEEK_READ_BUF_SIZE];
    char *pos = data;
    int data_len = 0;
    int rd_len;
    int ret;

    if(start_off != lseek(fd, 0, SEEK_CUR))
    {
        LOGF("start_off:%d cur:%d", start_off, lseek(fd, 0, SEEK_CUR));
        assert(0);
    }
    
    rd_len = ((end_off - start_off) > SEEK_READ_BUF_SIZE) ? SEEK_READ_BUF_SIZE : (end_off - start_off);
    data_len = rd_len;
    while((end_off - start_off) >= SEEK_FRAME_PES_HEAD_LEN)
    {
        //LOGI("start_off:%d end_off:%d data_len:%d rd_len:%d", start_off, end_off, data_len, rd_len);
        ret = readn(fd, pos, rd_len);
        if(ret <= 0)
        {
            LOGE("readn END...");
            return -1;
        }
        
        start_off += rd_len;
        if(start_off != lseek(fd, 0, SEEK_CUR))
        {
            LOGF("start_off:%d cur:%d", start_off, lseek(fd, 0, SEEK_CUR));
            assert(0);
        }

        //LOGI("find_pes_head_from_buf <<");
        ret = find_pes_head_from_buf(PES, data, data_len, 0);
        if(ret >= 0)
        {
            /*LOGI("find_pes_head_from_buf DTS:%lld ret:%d", PES->DTS, ret);
            LOGI("..%02x %02x %02x %02x",
                data[ret + 0], data[ret + 1], data[ret + 2], data[ret + 3]);
            LOGF("start_off:%d data_len:%d ret:%d (data_len - ret):%d  %d",
                start_off, data_len, ret, (data_len - ret), start_off - (data_len - ret));*/
            
            lseek(fd, start_off - (data_len - ret), SEEK_SET);
        #if 0
            LOGI("TTTTTTTTTTTTTTT");
            char test[8];
            readn(fd, test, 8);
            int ii;
            for(ii = 0; ii < 8; ii++)
            {
                printf("%02x ", test[ii]);
            }
            printf("\n");
            lseek(fd, -8, SEEK_CUR);
        #endif
            return 0;
        }

        memmove(&data[0], &data[data_len - SEEK_FRAME_PES_HEAD_LEN], SEEK_FRAME_PES_HEAD_LEN);
        pos = &data[SEEK_FRAME_PES_HEAD_LEN];
        rd_len = ((end_off - start_off) > (SEEK_READ_BUF_SIZE - SEEK_FRAME_PES_HEAD_LEN)) ?
                        (SEEK_READ_BUF_SIZE - SEEK_FRAME_PES_HEAD_LEN) : (end_off - start_off);
        data_len = SEEK_FRAME_PES_HEAD_LEN + rd_len;
    }

    LOGD("not found pes");
    return -2;
}

#if 0
// v1.0
int _record_seek(int fd, int dur_secs, int secs)
{
    int file_size, start_off, end_off, size_step;
    PES_header PES;
    UINT64_t timestamp;
    int ret = 0;

    file_size = lseek(fd, 0, SEEK_END);

#if 1   //add by lwj
    size_step = 200*1024*10;
    // if (dur_secs<=0)
    // {
    //     dur_secs = 1;
    // }
    
    int fileSeekSec = (dur_secs>RECORD_FILE_TIME-1)?(RECORD_FILE_TIME-1):dur_secs;  
    start_off = (file_size/RECORD_FILE_TIME)*(fileSeekSec);
    // start_off = file_size;
#endif

    //大约是这么多秒(SEEK_STEP_SECS)做一次搜索的step
#if 0
    size_step = file_size / ((dur_secs + SEEK_STEP_SECS - 1) / SEEK_STEP_SECS);
    start_off = size_step * ((secs + SEEK_STEP_SECS - 1) / SEEK_STEP_SECS);
    LOGI("start_off:%d size_step:%d", start_off, size_step);
    while(start_off > file_size)
    {
        start_off -= size_step;
    }
#else
    // size_step = 200*1024*10; //change by lwj
    // // size_step = 200*1024;
    // start_off = (file_size > size_step * 2) ? (file_size - size_step) : 0;
#endif
    end_off = file_size;
    LOGI("start_off:%d file_size:%d, dur_secs=%d", start_off, file_size, dur_secs);
    while(start_off >= 0)
    {
        ret = lseek(fd, start_off, SEEK_SET);
        if(ret < 0)
        {
            LOGE("seek FAIL errno:%d", errno);
            return FAIL;
        }

        ret = seek_frame_whence(fd, start_off, end_off, &PES);
        if(FAIL == ret)
        {
            LOGE("seek_frame_by_size fail");
            return FAIL;
        }
        else if(SUCCESS == ret)
        {
            timestamp = PES.DTS;
            timestamp = timestamp*100/9;
            // if((int)(timestamp / 1000) < secs * 1000)
            LOGI("secs=%d timestamp=%d", secs * 1000, (int)timestamp);
            // if((int)(timestamp) < secs * 1000)      //change by lwj
            if((int)(abs(timestamp) - secs*1000) < 2*1000)      //change by lwj
            {
                LOGI("seek_frame_by_secs OK TMP:%d req secs:%d cur:%d",
                    (int)(timestamp), secs*1000, lseek(fd, 0, SEEK_CUR));
                    // (int)(timestamp / 1000), secs, lseek(fd, 0, SEEK_CUR));
                ret = seek_frame_by_secs(fd, secs);
                LOGI("seek_frame_by_secs ret:%d", ret);
                if (ret == 0)
                {
                #if 0
                    LOGI("TTTTTTTTTTTTTTT");
                    char test[8];
                    readn(fd, test, 8);
                    int ii;
                    for(ii = 0; ii < 8; ii++)
                    {
                        printf("%02x ", test[ii]);
                    }
                    printf("\n");
                    lseek(fd, -8, SEEK_CUR);
                #endif
                }
                return ret;
            }
        }

        LOGI("start_off:%d end_off:%d", start_off, end_off);
        if(start_off == 0)
        {
            break;
        }
        else if(start_off > size_step)
        {
            end_off = start_off + 36;//多检索几个字节防止00 00 01 跨搜索段
            start_off -= size_step;
        }
        else
        {
            start_off = 0;
            end_off = size_step + 36;//多检索几个字节防止00 00 01 跨搜索段
        }
    }

    LOGI("_record_seek FAIL");
    return FAIL;
}
#else
// v2.0
int _record_seek(int fd, int dur_secs, int secs)
{
    int file_size, start_off, end_off, size_step;
    PES_header PES;
    UINT64_t timestamp;
    int ret = 0;

    file_size = lseek(fd, 0, SEEK_END);

    int offset = dur_secs;
    start_off = offset;
    //大约是这么多秒(SEEK_STEP_SECS)做一次搜索的step
#if 0
    size_step = file_size / ((dur_secs + SEEK_STEP_SECS - 1) / SEEK_STEP_SECS);
    start_off = size_step * ((secs + SEEK_STEP_SECS - 1) / SEEK_STEP_SECS);
    LOGI("start_off:%d size_step:%d", start_off, size_step);
    while(start_off > file_size)
    {
        start_off -= size_step;
    }
#else
    // size_step = 200*1024*10; //change by lwj
    // // size_step = 200*1024;
    // start_off = (file_size > size_step * 2) ? (file_size - size_step) : 0;
#endif
    end_off = file_size;
    LOGI("start_off:%d file_size:%d, dur_secs=%d", start_off, file_size, dur_secs);
    while(start_off < end_off)
    {
        ret = lseek(fd, start_off, SEEK_SET);
        if(ret < 0)
        {
            LOGE("seek FAIL errno:%d", errno);
            return FAIL;
        }

        ret = seek_frame_whence(fd, start_off, end_off, &PES);
        if(FAIL == ret)
        {
            LOGE("seek_frame_by_size fail");
            return FAIL;
        }
        else if(SUCCESS == ret)
        {
            timestamp = PES.DTS;
            timestamp = timestamp*100/9;
            LOGI("secs=%d timestamp=%d", secs * 1000, (int)timestamp);
            if((abs(timestamp - secs*1000) < 2*1000) || (0 == start_off))     //change by lwj
            {
                LOGI("seek_frame_by_secs OK TMP:%d req secs:%d cur:%d",
                    (int)(timestamp), secs*1000, lseek(fd, 0, SEEK_CUR));
                ret = seek_frame_by_secs(fd, secs);
                LOGI("seek_frame_by_secs ret:%d", ret);
                if (ret == 0)
                {
                #if 0
                    LOGI("TTTTTTTTTTTTTTT");
                    char test[8];
                    readn(fd, test, 8);
                    int ii;
                    for(ii = 0; ii < 8; ii++)
                    {
                        printf("%02x ", test[ii]);
                    }
                    printf("\n");
                    lseek(fd, -8, SEEK_CUR);
                #endif
                }
                return ret;
            }
        }

        LOGI("start_off:%d end_off:%d", start_off, end_off);
        start_off += 200*1024;
        // if(start_off == 0)
        // {
        //     break;
        // }
        // else if(start_off > size_step)
        // {
        //     end_off = start_off + 36;//多检索几个字节防止00 00 01 跨搜索段
        //     start_off -= size_step;
        // }
        // else
        // {
        //     start_off = 0;
        //     end_off = size_step + 36;//多检索几个字节防止00 00 01 跨搜索段
        // }
    }

    LOGI("_record_seek FAIL");
    return FAIL;
}
#endif

int _record_seek_by_offset(int fd, int offset_s, int secs)
{
    int file_size, start_off, end_off, size_step;
    PES_header PES;
    UINT64_t timestamp;
    int ret = 0;

    file_size = lseek(fd, 0, SEEK_END);

    // size_step = 200*1024*10;    
    size_step = 200*1024;    
    start_off = offset_s;

    //大约是这么多秒(SEEK_STEP_SECS)做一次搜索的step
// #if 0
//     size_step = file_size / ((dur_secs + SEEK_STEP_SECS - 1) / SEEK_STEP_SECS);
//     start_off = size_step * ((secs + SEEK_STEP_SECS - 1) / SEEK_STEP_SECS);
//     LOGI("start_off:%d size_step:%d", start_off, size_step);
//     while(start_off > file_size)
//     {
//         start_off -= size_step;
//     }
// #else
//     // size_step = 200*1024*10; //change by lwj
//     // // size_step = 200*1024;
//     // start_off = (file_size > size_step * 2) ? (file_size - size_step) : 0;
// #endif
    end_off = file_size;
    LOGI("start_off:%d file_size:%d", start_off, file_size);
    while(start_off >= 0)
    {
        ret = lseek(fd, start_off, SEEK_SET);
        if(ret < 0)
        {
            LOGE("seek FAIL errno:%d", errno);
            return FAIL;
        }

        ret = seek_frame_whence(fd, start_off, end_off, &PES);
        if(FAIL == ret)
        {
            LOGE("seek_frame_by_size fail");
            return FAIL;
        }
        else if(SUCCESS == ret)
        {
            timestamp = PES.DTS;
            timestamp = timestamp*100/9;
            // if((int)(timestamp / 1000) < secs * 1000)
            LOGI("secs=%d timestamp=%d", secs * 1000, (int)timestamp);
            // if((int)(timestamp) < secs * 1000)      //change by lwj
            // if((int)(abs(timestamp) - secs*1000) < 2*1000)      //change by lwj
            if((int)(timestamp) < (secs * 1000))
            {
                LOGI("seek_frame_by_secs OK TMP:%d req secs:%d cur:%d",
                    (int)(timestamp), secs*1000, lseek(fd, 0, SEEK_CUR));
                    // (int)(timestamp / 1000), secs, lseek(fd, 0, SEEK_CUR));
                ret = seek_frame_by_secs(fd, secs);
                LOGI("seek_frame_by_secs ret:%d", ret);
                if (ret == 0)
                {
                #if 0
                    LOGI("TTTTTTTTTTTTTTT");
                    char test[8];
                    readn(fd, test, 8);
                    int ii;
                    for(ii = 0; ii < 8; ii++)
                    {
                        printf("%02x ", test[ii]);
                    }
                    printf("\n");
                    lseek(fd, -8, SEEK_CUR);
                #endif
                }
                return ret;
            }
        }

        LOGI("start_off:%d end_off:%d", start_off, end_off);
        if(start_off == 0)
        {
            break;
        }
        else if(start_off > size_step)
        {
            end_off = start_off + 36;//多检索几个字节防止00 00 01 跨搜索段
            start_off -= size_step;
        }
        else
        {
            start_off = 0;
            end_off = size_step + 36;//多检索几个字节防止00 00 01 跨搜索段
        }
    }

    LOGI("_record_seek FAIL");
    return FAIL;
}

#if 0
int record_seek(int fd, int dur_secs, int secs)
{
    int file_size = lseek(fd, 0, SEEK_END);
    UINT64_t timestamp;
    PES_header PES;
    int ret;

    if(file_size <= 0)
    {
        return FAIL;
    }

    file_size -= SEEK_LAST_HEAD_BUFFSIZE;
    while(file_size > 0)
    {
        ret = seek_pes_head_by_offset(&PES, fd, file_size, 0);
        if(ret >= 0)
        {
            LOGI("timestamp:%lld", PES.DTS*100/9);
            timestamp = PES.DTS*100/9;
            return (int)((timestamp) / (1000*1000)); 
        }
        else if(ret == -1)
        {
            LOGI("seek_pes_head_by_offset FAIL");
            return FAIL;
        }

        file_size -= (SEEK_LAST_HEAD_BUFFSIZE - PES_HEAD_LEN + 1);
    }

    LOGI("_get_pes_file_durtime FAIL");
    return -1;
}
#endif

// int record_seek(int fd, int dur_secs, int secs)
int record_seek(int fd, int offset_s, int secs)
{
    uint64_t timestamp;
    int eslen;
    int current;
    int ret;

    current = lseek(fd, 0, SEEK_CUR);
    LOGI("current:%d\n", current);
    
    ret = lseek(fd, 0, SEEK_SET);
    if(ret < 0)
    {
        LOGE("seek FAIL errno:%d", errno);
        if(ret != SUCCESS)
        {
            lseek(fd, current, SEEK_SET);
        }
        return FAIL;
    }
    //LOGI("after seek - current:%d", ftell(fp));
    
    if(0 == secs)
    {
        return 0;
    }
    
#if 0
    ret = seek_frame_by_secs(fp, secs);
#else
    LOGI("_record_seek <<<<< fd:%d", fd);
    // ret = _record_seek(fd, dur_secs, secs);
    ret = _record_seek_by_offset(fd, offset_s, secs);
    LOGI("_record_seek >>>>>");

#endif
    if(ret != SUCCESS)
    {
        LOGE("_record_seek fail");
        lseek(fd, current, SEEK_SET);
    }
    else
    {
        LOGI("_record_seek success");
    }
    return ret;
}

#define SEEK_LAST_HEAD_BUFFSIZE (512)
static int seek_pes_head_by_offset(PES_header *pes, int fd, int offset, int last)
{
    char buf[SEEK_LAST_HEAD_BUFFSIZE];
    int ret;

    if(offset < 0)
    {
        LOGE("offset:%d < 0", offset);
        return -1;
    }
    
    ret = lseek(fd, offset, SEEK_SET);
    if(ret < 0)
    {
        LOGE("lseek SEEK_SET offset:%d FAIL", offset);
        return -1;
    }

    ret = readn(fd, buf, SEEK_LAST_HEAD_BUFFSIZE);
    if(ret < 0)
    {
        LOGE("readn FAIL");
        return -1;
    }

    ret = find_pes_head_from_buf(pes, buf, ret, last);
    if( ret < 0)
    {
        return -2;
    }
    
    return offset + ret;
}

static int _get_pes_file_durtime(int fd)
{
    int file_size = lseek(fd, 0, SEEK_END);
    UINT64_t timestamp;
    PES_header PES;
    int ret;

    if(file_size <= 0)
    {
        return FAIL;
    }

    file_size -= SEEK_LAST_HEAD_BUFFSIZE;
    while(file_size > 0)
    {
        ret = seek_pes_head_by_offset(&PES, fd, file_size, 1);
        if(ret >= 0)
        {
            LOGI("timestamp:%lld", PES.DTS*100/9);
            timestamp = PES.DTS*100/9;
            return (int)((timestamp) / (1000));     //change by lwj
            // return (int)((timestamp) / (1000*1000)); 
        }
        else if(ret == -1)
        {
            LOGI("seek_pes_head_by_offset FAIL");
            return FAIL;
        }

        file_size -= (SEEK_LAST_HEAD_BUFFSIZE - PES_HEAD_LEN + 1);
    }

    LOGI("_get_pes_file_durtime FAIL");
    return -1;
}

int get_pes_file_durtime(char *filename)
{
    int fd = open(filename, O_RDONLY);
    int ret = -1;

    if(fd < 0)
    {
        LOGE("open %s FAIL", filename);
        return -1;
    }

    ret = _get_pes_file_durtime(fd);
    close(fd);
    return ret;
}

