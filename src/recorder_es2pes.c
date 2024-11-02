/**
* created by yys(Vincent.Yeh)
*/
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "logServer.h"
// #include "sepcam_misc.h"
#include "recorder_es2pes.h"

static inline void bits_initwrite(BITS_BUFFER_t *bits_buffer_p,int i_size,void *data_p)
{
    //HY_ASSERT(data_p != NULL);
    bits_buffer_p->i_size = i_size;
    bits_buffer_p->i_data = 0;
    bits_buffer_p->p_data = (uint8_t *)data_p;
    bits_buffer_p->i_mask = 0x80;
}

void bits_write(BITS_BUFFER_t *bits_buffer_p,int i_count,uint64_t i_bits)
{
    while(i_count-- >0)
    {
        if((i_bits >>i_count)&0x01)
        {
            bits_buffer_p->p_data[bits_buffer_p->i_data] |= bits_buffer_p->i_mask;
        }
        else
        {
            bits_buffer_p->p_data[bits_buffer_p->i_data] &= ~bits_buffer_p->i_mask;
        }
        bits_buffer_p->i_mask >>=1;
        if(bits_buffer_p->i_mask == 0)
        {
            bits_buffer_p->i_mask = 0x80;
            bits_buffer_p->i_data++;
        }
    }
}


/*****************************************************************
* es流打包成pes流
* pes_head_p: pes流头空间
* eslen: es流的长度
* streamID:流id，音频流范围: 0xC0~ 0xDF 视频流范围 : 0xE0 ~0xFF
*pts dts 打包时间戳 毫秒数 以90Khz为单位 pts = dts +3600
*
*example: hisi time is hisi_pts (ms);
* base_pts = hisi_pts;
* delta_pts = (hisi_pts - base_pts);
* dts += (delta_pts*90)
* pts = dts + 3600;
*
*hisi_pts = base_pts + (delta_pts = dts/90) 
******************************************************************/
#if 0
int parser_pes(int fd,PES_header *PES,char **pdata,int* len,char *frame_type)
{
    //先需要找到pes头 000001 在分析pes头
    //unsigned int prefix;
    char peshead[PES_HEAD_LEN];
    int filechar;
    int ret = 0;

    ret = read(fd,peshead,PES_HEAD_LEN);
    if(ret != PES_HEAD_LEN)
    {
        printf("parser_pes................01");
       return -1; 
    }
    //printf("parser_pes...................1:%x\n",ret);
    if(peshead[0] != 0||peshead[1] != 0||peshead[2] != 1)
    {
        printf("parser_pes................02");
       return -1;  
    }

    PES->stream_id= peshead[3] & 0xFF;

    if (PES->stream_id >=0xbc) 
    {
          PES->PES_packet_length=(peshead[4]<<8 | (peshead[5] & 0xFF)) & 0xFFFF;
          
          if ((peshead[6] & 0xC0) != 0x80) 
          {
                printf("parser_pes................1");
                return -1;
          }
          
          filechar = peshead[7];
          PES->PTS_flag=filechar & 0x80;
          PES->DTS_flag=filechar & 0x40;
          PES->ESCR_flag=filechar & 0x20;
          PES->ES_rate_flag=filechar & 0x10;
          PES->DSM_trick_mode_flag=filechar & 0x08;
          PES->additional_copy_info_flag=filechar & 0x04;
          PES->PES_CRC_flag=filechar & 0x02;
          PES->PES_extension_flag=filechar & 0x01;
          filechar = peshead[8];
          PES->PES_header_data_length=filechar & 0xFF;
          
          //printf("PES->PES_header_data_length..........:%d\n",PES->PES_header_data_length);
          if(PES->PES_header_data_length != 27)
          {
                printf("parser_pes................2");
                return -1;
          }
          
          char *HD = &peshead[9];
          // parse subheaders
          
          int n=0;
          if (PES->PTS_flag) 
          {
                if ((HD[n] & 0xF0) != ((PES->DTS_flag)?0x30:0x20)) 
                {
                    printf("parser_pes................3");
                    return -1;
                }
                PES->PTS=(HD[n++] >> 1) & 0x07ULL;  // Bit 32-30
                PES->PTS=(PES->PTS << 8) | (HD[n++] & 0xFFULL);  // Bit 29-22
                PES->PTS=(PES->PTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 21-15
                PES->PTS=(PES->PTS << 8) | (HD[n++] & 0xFFULL);  // Bit 14-7
                PES->PTS=(PES->PTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 6-0
                if (PES->DTS_flag) 
                {
                      if ((HD[n] & 0xF0) != 0x10) 
                      {
                        printf("parser_pes................4");
                            return -1;
                      }
                      PES->DTS=(HD[n++] >> 1) & 0x07ULL;  // Bit 32-30
                      PES->DTS=(PES->DTS << 8) | (HD[n++] & 0xFFULL);  // Bit 29-22
                      PES->DTS=(PES->DTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 21-15
                      PES->DTS=(PES->DTS << 8) | (HD[n++] & 0xFFULL);  // Bit 14-7
                      PES->DTS=(PES->DTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 6-0
                }
          }
          if (PES->PES_extension_flag)
          {
            n+=1;
            PES->PES_packet_length = (HD[11]<< 24&0xFF000000)|(HD[12] << 16&0xFF0000)|(HD[13] << 8&0xFF00)|(HD[14]&0xFF);
            //printf("PES->PES_packet_length       is:%u\n",PES->PES_packet_length);
          }
          
          if (PES->PES_packet_length) 
          {
                // how long is the size of the remaining PES-data? (now ES-data, that is)
                int packet_data_length=PES->PES_packet_length;// - (3 + PES->PES_header_data_length);
                
                
                if(packet_data_length == 0)
                {
                        printf("parser_pes................5");
                    return -1;
                }
                
                char * pbuf = (char*)malloc(packet_data_length);
                if(pbuf == NULL)
                {
                    printf("my_malloc len:%d,%d\n ",\
                        PES->PES_packet_length,PES->PES_header_data_length);
                    printf("parse pes my_malloc fail!!!!len:%d\n",packet_data_length);
                    return -1;
                }
                ret = read(fd,pbuf,packet_data_length) ;
                if(ret != packet_data_length)
                {
                    printf("parse pes fread fail!!!!\n");
                    return -1;
                }
                //printf("parser_pes...................2:%x\n",ret);
                *pdata = pbuf;
                
                *len = packet_data_length;
                *frame_type = *(*pdata+4)&0xf;
                
                return 0;
          } 

    } 
    return -1;
}
#endif

static int check_pes_head(unsigned char *peshead)
{
    if(peshead[0] != 0||peshead[1] != 0||peshead[2] != 1)
    {
        //printf("parser_pes FAIL:%02x %02x %02x %02x %02x %02x\n",
        //    peshead[0], peshead[1], peshead[2],
        //    peshead[3], peshead[4], peshead[5]);
        return -1;  
    }
    
    //LOGI("%02x  == 0xc0 :%s  %02x== 0xe0 :%s",
    //    peshead[3], peshead[3] == 0xc0 ? "TRUE" : "FALSE",
    //    (peshead[3] & 0xf0), (peshead[3] & 0xf0) == 0xe0 ? "TRUE" : "FALSE");
    if (!CHECK_PES_STREAM_ID(peshead[3])) //((peshead[3] & 0xFF) < 0xbc) 
    {
        LOGE("CHECK_PES_STREAM_ID FAIL:%02x %02x %02x %02x %02x FAIL",
            peshead[0], peshead[1], peshead[2],
            peshead[3], peshead[4]);
        return -1;  
    }

    return 0;
}

int find_pes_head(char *data, size_t datalen)
{
    int eslen, offset = 0;
    int ret;
    
    while(datalen >= 4)
    {
        ret = check_pes_head((uint8_t *)data);
        if(ret == 0)
        {
            return offset;
        }

        data++;
        datalen--;
        offset++;
    }
    return FAIL;
}

int find_pes_head_from_buf(PES_header *pes, char *data, int datalen, int last)
{
    int eslen, offset = 0;
    int last_offset = -1;
    int ret;
    
    while(datalen >= PES_HEAD_LEN)
    {
        ret = check_pes_head((uint8_t *)data);
        if(ret == 0)
        {
            ret = recorder_parser_pes_info((uint8_t *)data, pes, &eslen, NULL);
            if(ret < 0)
            {
                LOGE("recorder_parser_pes_info FAIL");
                return last_offset;
            }

            if(pes->stream_id >= 0xe0)
            {
                last_offset = offset;
                if(!last)
                {
                    return offset;
                }
            }
                
            data += (eslen + PES_HEAD_LEN);
            datalen -= (eslen + PES_HEAD_LEN);
            if(datalen < 0)
            {
                return last_offset;
            }
            offset += (eslen + PES_HEAD_LEN);
            //LOGI("recorder_parser_pes_info ok");
        }
        else
        {
            data++;
            datalen--;
            offset++;
        }
    }

    return last_offset;
}

int find_pes_head_end(char *data, size_t datalen)
{
    char *pos;
    int offset;
    int ret;

    offset = datalen - 4;
    while(offset >= 0)
    {
        pos = &data[offset];
        ret = check_pes_head((uint8_t *)pos);
        if(ret == 0)
        {
            return offset;
        }
        offset--;
    }
    return FAIL;
}

int seek_pes_head(int fd, PES_header *PES, int *eslen, unsigned char *out)
{
#define SEEK_FRAME_PES_HEAD_LEN (PES_HEAD_LEN)

    unsigned char pes_head[SEEK_FRAME_PES_HEAD_LEN];
    char frame_type;
    UINT64_t timestamp;
    long pre_tell = 0;
    int try_times = 0;
    int ret;

    ret = readn(fd, pes_head, SEEK_FRAME_PES_HEAD_LEN);
    if(ret <= 0)
    {
        LOGW("fread END...");
        return FAIL;
    }
    
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
                LOGI("memmove BEFOR<<<<<<<<<<<<<<ret:%d ftell:%d", ret, lseek(fd, 0, SEEK_CUR));
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
            }
            
#if 0
            printf("recorder_parser_pes_info<<<<<<<<<<<<<<\n");
            int ii;
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
            ret = recorder_parser_pes_info(pes_head, PES, eslen, NULL);
            if(ret != 0)
            {
                LOGE("recorder_parser_pes_info fail");
                return FAIL;
            }

            // LOGI("OK");
            // add by lwj
            // printf("-------[");
            // for (int i = 0; i < PES_HEAD_LEN; i++)
            // {
            //     printf("%X ", pes_head[i]);
            // }
            // printf("]\n");
            memcpy(out, pes_head, PES_HEAD_LEN);
            // add end by lwj
            return SUCCESS;
        }

        LOGE("find_pes_head fail try_times:%d", try_times);
        if(try_times++ > SEEK_FRAME_PES_HEAD_LEN*100)
        {
            LOGE("find_pes_head try_times:%d FAIL", try_times);
            return FAIL;
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
    }while(1);
    
    return FAIL;
}

int recorder_parser_pes_info(unsigned char* head, PES_header *PES, int* len, char *frame_type)
{
    //先需要找到pes头 000001 在分析pes头
    //unsigned int prefix;
    //char *peshead[PES_HEAD_LEN];
    unsigned char *peshead = head;
    int filechar;
//    int ret = 0;
    unsigned char *pdata;
/*
    ret = read(fd,peshead,PES_HEAD_LEN);
    if(ret != PES_HEAD_LEN)
    {
        printf("parser_pes................01\n");
       return -1; 
    }*/
    
    //LOGI("parser_pes...................1:%x");
    if(peshead[0] != 0||peshead[1] != 0||peshead[2] != 1)
    {
        LOGE("parser_pes FAIL:%02x %02x %02x %02x %02x %02x",
            peshead[0], peshead[1], peshead[2],
            peshead[3], peshead[4], peshead[5]);
        return -1;  
    }

    PES->stream_id= peshead[3] & 0xFF;

    if (CHECK_PES_STREAM_ID(PES->stream_id)) //(PES->stream_id == 0xc0 || (PES->stream_id && 0xf0) == 0xe0) 
    {
          PES->PES_packet_length = (peshead[4]<<8 | (peshead[5] & 0xFF)) & 0xFFFF;
          
          if ((peshead[6] & 0xC0) != 0x80) 
          {
                LOGE("parser_pes................1");
                return -1;
          }
          
          filechar = peshead[7];
          PES->PTS_flag=filechar & 0x80;
          PES->DTS_flag=filechar & 0x40;
          PES->ESCR_flag=filechar & 0x20;
          PES->ES_rate_flag=filechar & 0x10;
          PES->DSM_trick_mode_flag=filechar & 0x08;
          PES->additional_copy_info_flag=filechar & 0x04;
          PES->PES_CRC_flag=filechar & 0x02;
          PES->PES_extension_flag=filechar & 0x01;
          filechar = peshead[8];
          PES->PES_header_data_length=filechar & 0xFF;
          
          //printf("PES->PES_header_data_length..........:%d\n",PES->PES_header_data_length);
          if(PES->PES_header_data_length != 27)
          {
                LOGE("parser_pes PES_header_data_length:%d != (27) err", PES->PES_header_data_length);
                return -1;
          }
          
          unsigned char *HD = &peshead[9];
          // parse subheaders
          
          int n=0;
          if (PES->PTS_flag) 
          {
                if ((HD[n] & 0xF0) != ((PES->DTS_flag)?0x30:0x20)) 
                {
                    LOGE("parser_pes................3");
                    return -1;
                }
                PES->PTS=(HD[n++] >> 1) & 0x07ULL;  // Bit 32-30
                PES->PTS=(PES->PTS << 8) | (HD[n++] & 0xFFULL);  // Bit 29-22
                PES->PTS=(PES->PTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 21-15
                PES->PTS=(PES->PTS << 8) | (HD[n++] & 0xFFULL);  // Bit 14-7
                PES->PTS=(PES->PTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 6-0
                if (PES->DTS_flag) 
                {
                    if ((HD[n] & 0xF0) != 0x10) 
                    {
                        LOGE("parser_pes................4");
                        return -1;
                    }
                    PES->DTS=(HD[n++] >> 1) & 0x07ULL;  // Bit 32-30
                    PES->DTS=(PES->DTS << 8) | (HD[n++] & 0xFFULL);  // Bit 29-22
                    PES->DTS=(PES->DTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 21-15
                    PES->DTS=(PES->DTS << 8) | (HD[n++] & 0xFFULL);  // Bit 14-7
                    PES->DTS=(PES->DTS << 7) | ((HD[n++] >> 1) & 0x7FULL);  // Bit 6-0
                }
          }
          
        if (PES->PES_extension_flag)
        {
            n+=1;
            PES->PES_packet_length = (HD[11]<< 24&0xFF000000)|(HD[12] << 16&0xFF0000)|(HD[13] << 8&0xFF00)|(HD[14]&0xFF);
            //LOGI("PES->PES_packet_length is:%u", PES->PES_packet_length);
        }

        if (PES->PES_packet_length) 
        {
            // how long is the size of the remaining PES-data? (now ES-data, that is)
            int packet_data_length = PES->PES_packet_length;// - (3 + PES->PES_header_data_length);
            if(packet_data_length == 0)
            {
                LOGE("parser_pes................5");
                return -1;
            }
            /*
            char * pbuf = (char*)malloc(packet_data_length);
            if(pbuf == NULL)
            {
                printf("my_malloc len:%d,%d\n ",\
                PES->PES_packet_length,PES->PES_header_data_length);
                printf("parse pes my_malloc fail!!!!len:%d\n",packet_data_length);
                return -1;
            }
            ret = read(fd,pbuf,packet_data_length) ;
            if(ret != packet_data_length)
            {
                printf("parse pes fread fail!!!!\n");
                return -1;
            }
            //printf("parser_pes...................2:%x\n",ret);
            *pdata = pbuf;
            */
            *len = packet_data_length;
            if(frame_type)
            {
                // pdata = head + PES_HEAD_LEN;
                // *frame_type = *(pdata+4)&0xf;
                // add by lwj
                pdata = head + PES_HEAD_LEN-7;
                *frame_type = *(pdata)&0xf;
                // printf("[");
                // for (int i = 0; i < PES_HEAD_LEN +4+1; i++)
                // {
                //     printf("%X ", head[i]);
                // }
                // printf("]\n");
                // add end by lwj
            }

            return 0;                
        } 
        else
        {
            LOGE("#### PES_packet_length is 0");
        }
    } 
    else
    {
        LOGE( "error:stream_id 0x%x,PES->PES_packet_length %d",PES->stream_id,PES->PES_packet_length);
    }

    return -1;
}

int recorder_es2pes(unsigned char *pes_head_p,int eslen,unsigned char streamID,uint32_t pts,uint32_t dts,PES_PRIVATE_DATA_t *pData)
{
    BITS_BUFFER_t bits_buffer;
    
    bits_initwrite(&bits_buffer,PES_HEAD_LEN,pes_head_p);
    
    bits_write( &bits_buffer, 24, 0x01 );		//start_code:0x000001
    bits_write( &bits_buffer, 8, streamID );//[3]		//stream_id
    //bits_write( &bits_buffer, 16, eslen+13 );		//packet_len
    bits_write( &bits_buffer, 16, 0 );		//packet_len
/*
    bits_write( &bits_buffer, 2, 2 );			//'10'
    bits_write( &bits_buffer, 2, 0 );			//scrambling_control
    bits_write( &bits_buffer, 1, 0 );			//priority
    bits_write( &bits_buffer, 1, 0 );			//data_alignment_indicator
    bits_write( &bits_buffer, 1, 0 );			//copyright	
    bits_write( &bits_buffer, 1, 0 );			//original_or_copy
*/
    bits_write(&bits_buffer,8,0x80);//[6]
/*
    bits_write( &bits_buffer, 1, 1 );			//PTS_flag
    bits_write( &bits_buffer, 1, 1 );			//DTS_flag
    bits_write( &bits_buffer, 1, 0 );			//ESCR_flag
    bits_write( &bits_buffer, 1, 0 );			//ES_rate_flag
    bits_write( &bits_buffer, 1, 0 );			//DSM_trick_mode_flag
    bits_write( &bits_buffer, 1, 0 );			//additional_copy_info_flag
    bits_write( &bits_buffer, 1, 0 );			//PES_CRC_flag
    bits_write( &bits_buffer, 1, 0 );			//PES_extension_flag
*/
    bits_write(&bits_buffer,8,0xC1);//[7]

    bits_write( &bits_buffer, 8, 27 );//[8]			//header_data_length
    //PTS,DTS
	bits_write( &bits_buffer, 4, 3 );	//[9]		//'0011'
	bits_write( &bits_buffer, 3, pts >>30 );		//PTS[32..30]
	bits_write( &bits_buffer, 1, 1 );
	bits_write( &bits_buffer, 15, (pts<<2)>>17);//[10]
	bits_write( &bits_buffer, 1, 1 );
	bits_write( &bits_buffer, 15, pts  & 0x7fff);//[12]
	bits_write( &bits_buffer, 1, 1 );

	bits_write( &bits_buffer, 4, 1 );	//[14]		//'0001'
	bits_write( &bits_buffer, 3, dts >>30 );		//PTS[32..30]
	bits_write( &bits_buffer, 1, 1 );
	bits_write( &bits_buffer, 15, (dts<<2)>>17);//[15]
	bits_write( &bits_buffer, 1, 1 );
	bits_write( &bits_buffer, 15, dts  & 0x7fff);//[17]
	bits_write( &bits_buffer, 1, 1 );
    
    bits_write( &bits_buffer, 8, 0x80 );//[19]
    bits_write( &bits_buffer, 32, eslen );//[20]
    if(pData)
    {
		bits_write(&bits_buffer, 16, pData->pic_width);
		bits_write(&bits_buffer, 16, pData->pic_height);
		bits_write(&bits_buffer, 8,  pData->frame_rate);
		bits_write(&bits_buffer, 8,  pData->mode); //
		bits_write(&bits_buffer, 8,  pData->YUV_Format);

		#if 1
		bits_write(&bits_buffer, 40, 0); //剩余清零
		#else	//for hxht
		bits_write(&bits_buffer, 8, 0x01);
		bits_write(&bits_buffer, 8, 0x01);
		bits_write(&bits_buffer, 24, 0); //剩余清零
		#endif
    }
    else
    {
	    bits_write( &bits_buffer, 96, 0);
    }
/*
    LOGI("streamID = %x eslen = %x %x %x %x %x %x %x",
        streamID,eslen,
        pes_head_p[0],pes_head_p[1],pes_head_p[2],pes_head_p[3],pes_head_p[4],
        pes_head_p[5],pes_head_p[6]);
*/
    return PES_HEAD_LEN;
}


