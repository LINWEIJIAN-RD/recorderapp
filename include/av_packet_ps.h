#ifndef _RECORDER_PACKET_PS_H_
#define _RECORDER_PACKET_PS_H_

#include "stream_frame_info.h"
#include "attach_rdwr_buff.h"
#include "frame_buff.h"

// added by lishun
#define ENC_G726 2
// end by lishun

int record_write_data_buff(int fd, char *data, int len, RDWR_BUFF_t *wr_buf);
int record_async_data_buff(int fd, RDWR_BUFF_t *wr_buf);

int wirte_es2pes_packet(int fd, STREAM_FRAME_INFO_t *frameinfo, char *es, int es_len, RDWR_BUFF_t *wr_buf);
frame_buff_t* read_pes2es_packet_frame(int fd);
int get_pes_file_durtime(char *filename);
int create_pes_record(char *filename);
int stop_pes_record();

// added by lishun
int open_pes_record(char *filename);
// end by lishun

// int record_seek(int fd, int dur_secs, int secs);
int record_seek(int fd, int offset_s, int secs);

#endif

