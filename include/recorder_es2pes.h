#ifndef _RECORDER_ES2PES_H
#define _RECORDER_ES2PES_H

#define PES_HEAD_LEN    36

#define PES_SYNC_SIZE   3
#define PES_STR_TYPE_UNKNOW 0x00
#define PES_STR_TYPE_VIDEO  0x01
#define PES_STR_TYPE_AUDIO  0x02
#define PES_PREFIX      0x000001
#define PES_CODE_AUDIO  0xC0
#define PES_NUMB_AUDIO  0x20
#define PES_CODE_VIDEO  0xE0
#define PES_NUMB_VIDEO  0x20

//add by lwj
#if 1
#define PES_CODE_VIDEO1  0xE1
#define CHECK_PES_STREAM_ID(x) ((x) == 0xc0 || ((x) & 0xf0) == 0xe0 || ((x) & 0xff) == 0xe1)
#else
#define CHECK_PES_STREAM_ID(x) ((x) == 0xc0 || ((x) & 0xf0) == 0xe0)
#endif
//add end by lwj


typedef struct PES_PRIVATE_DATA
{
    unsigned short      pic_width;
    unsigned short      pic_height;
    unsigned char       frame_rate;
    unsigned char       mode; //h264:(1-9 I帧为7，其他为1，音频大于20，目前用22); MJPEG:(10 - 19)
    unsigned char       YUV_Format;
    unsigned char       StreamType;   //海斯的为0  ，中途的为4 ，智原为5
    // unsigned char       vench;   //vench 0:第一路流的帧 1:第二路流的帧
} PES_PRIVATE_DATA_t;

struct PES_HEADER
{
    int stream_id;  //流ID  标示音频、视频流
    uint32_t pts;   //pts 时间戳
    uint32_t dts;   //dts 时间戳
    int pes_head_len; //pes header长度
    int es_len;      //es流长度
    int key_frame_flg; //是否为关键帧
};
typedef struct PES_HEADER PES_HEADER_t;


struct BITS_BUFFER
{
    int				i_size;
    int				i_data;
    unsigned char	i_mask;
    unsigned char*	p_data;
};
typedef struct BITS_BUFFER BITS_BUFFER_t;

typedef struct {  // data in the PES-header
  unsigned int stream_id,
               PES_packet_length,
               PES_scrambling_control;
  int          PES_priority,
               data_alignment_indicator,
               copyright,
               original_or_copy,
               PTS_flag,
               DTS_flag,
               ESCR_flag,
               ES_rate_flag,
               DSM_trick_mode_flag,
               additional_copy_info_flag,
               PES_CRC_flag,
               PES_extension_flag;
  unsigned int PES_header_data_length;
  unsigned long long int PTS,
                         DTS,
                         ESCR_base;
  unsigned int ESCR_extension,
               ES_rate,
               trick_mode_control,
               additional_copy_info,
               previous_PES_packet_CRC;
  int          PES_private_data_flag,
               pack_header_field_flag,
               program_packet_sequence_counter_flag,
               P_STD_buffer_flag,
               PES_extension_flag_2;
  char         PES_private_data[128];
  unsigned int pack_field_length,
               program_packet_sequence_counter;
  int          MPEG1_MPEG2_identifier;
  unsigned int original_stuff_length;
  int          P_STD_buffer_scale;
  unsigned int P_STD_buffer_size,
               PES_extension_field_length;
} PES_header;

#define program_stream_map_id 0xBC
#define private_stream_1_id 0xBD
#define padding_stream_id 0xBE
#define private_stream_2_id 0xBF
#define ECM_stream_id 0xF0
#define EMM_stream_id 0xF1
#define DSMCC_stream_id 0xF2
#define ISO_13522_stream_id 0xF3
#define ITU_A_id 0xF4
#define ITU_B_id 0xF5
#define ITU_C_id 0xF6
#define ITU_D_id 0xF7
#define ITU_E_id 0xF8
#define ancillary_stream_id 0xF9
#define program_stream_directory_id 0xFF
#define packet_start_code   0xBA
#define packet_system_head_code 0xBB

//int fnwrite(FILE *fp, const void *ptr, size_t n);
//int freadn(FILE *fp, void *ptr, size_t n);

int find_pes_head(char *data, size_t datalen);
int find_pes_head_end(char *data, size_t datalen);
int find_pes_head_from_buf(PES_header *pes, char *data, int datalen, int flag);

// int seek_pes_head(int fp, PES_header *PES, int *eslen);
// add by lwj
int seek_pes_head(int fd, PES_header *PES, int *eslen, unsigned char *out);
// add end by lwj
int recorder_es2pes(unsigned char *pes_head_p,int eslen,unsigned char streamID,uint32_t pts,uint32_t dts,PES_PRIVATE_DATA_t *pData);
int recorder_parser_pes_info(unsigned char* head, PES_header *PES, int* len, char *frame_type);

#endif

