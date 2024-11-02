#include <stdlib.h>
#include "file_list.h"
#include "logServer.h"
// #include "cqueue.h"

#ifdef MAX_FILE_PATH
#undef MAX_FILE_PATH
#endif
#define MAX_FILE_PATH	260

#define NEED_IGNORE_FILES(filename)    \
    (NULL != strstr((filename),"-.avi")     \
    || NULL != strstr((filename),"-.pes")   \
    || NULL != strstr((filename),"-.mp4"))
    
// VideoFileInfo_t gVideoFileInfo;	//0 cur utc day  1:last utc day
// mutex_t gVideoFileInfoLock;
// VideoFileInfo_t gLastVideoFileInfo;	//0 cur utc day  1:last utc day

// int list_file_to_jsonstr(char*buf,int len,int mode,int start,int count,const char*dir_path,const char*dir_name,int recurse){
// //	cJSON *json;
// 	DIR *dir;
// 	int ret = 0;
//     struct dirent *ptr; 
// 	struct stat statbuf;
//     int num = 0, i = 0;
// 	char filepath[MAX_FILE_PATH];
// 	char*tmpbuf = (char*)malloc(len);
// 	if(tmpbuf == NULL){
// 		return -1;
// 	}
// 	dir=opendir(dir_path);
//     if (dir == NULL){
//         perror("opendir error:");
//         LOGI("LINE %d list_file_to_jsonstr Open dir %s error...",__LINE__,dir_path);
// 		free(tmpbuf);
//         return -1;
//     }
// 	int strl = strlen(buf);
// 	if(strl + 64 >= len){
// 		ret = -1;
// 		goto error;
// 	}
// 	if(strl<2 || buf[strl-2] != ']' || buf[strl-1] != '}'){
// 		buf[0] = '\0';
// 		sprintf(buf,"{\"t\":%d,\"n\":\"%s\",\"l\":[]}",DT_DIR,dir_name);
// 	}else if(buf[strl-3] == '['){
// 		buf[strl-2] = '\0';
// 		sprintf(tmpbuf,"%s{\"t\":%d,\"n\":\"%s\",\"l\":[]}]}",buf,DT_DIR,dir_name);
// 		strncpy(buf,tmpbuf,len);
// 	}else{
// 		buf[strl-2] = '\0';
// 		sprintf(tmpbuf,"%s,{\"t\":%d,\"n\":\"%s\",\"l\":[]}]}",buf,DT_DIR,dir_name);
// 		strncpy(buf,tmpbuf,len);
// 	}

//     while ((ptr=readdir(dir))){//ptr!=NULL
// 		if(ptr->d_type&DT_DIR){
// 			// printf("dir_path = %s\n",dir_path);
// 			if(mode<1 || ptr->d_name == NULL || ptr->d_name[0] == '.' || !strcmp(ptr->d_name,".."))
// 				continue;
// 			if(start>=0 && count >=0 ){
// 				if(i++<start){
// 					continue;
// 				}
// 				if(num++>=count){
// 					break;
// 				}
// 			}
// 			sprintf(filepath,"%s/%s",dir_path,ptr->d_name);
// 			printf("open[%s]\n",filepath);
// 			int rec = recurse-1;
// 			if(rec>0){
// 				if(list_file_to_jsonstr(buf,len,mode,start,count,filepath,ptr->d_name,rec)<0){
// 					ret = -1;
// 					goto error;
// 				}
// 			}else{
// 				int strl = strlen(buf);
// 				if(buf[strl-3] == '['){
// 					buf[strl-2] = '\0';
// 					sprintf(tmpbuf,"%s{\"t\":%d,\"n\":\"%s\",\"l\":[]}]}",buf,ptr->d_type,ptr->d_name);
// 					strncpy(buf,tmpbuf,len);
// 				}else{
// 					buf[strl-2] = '\0';
// 					sprintf(tmpbuf,"%s,{\"t\":%d,\"n\":\"%s\",\"l\":[]}]}",buf,ptr->d_type,ptr->d_name);
// 					strncpy(buf,tmpbuf,len);
// 				}
// 			}
// 		}
// 		else if(ptr->d_type&DT_REG){
// 			// printf("file_path = %s\n",dir_path);
// 			if(NEED_IGNORE_FILES(ptr->d_name))    //正在录像的文件，不允许被搜索出来播放
//             {
//                 LOGI("search recording file:%s ignore it", ptr->d_name); 
//                 continue;
//             }
            
// 			sprintf(filepath,"%s/%s",dir_path,ptr->d_name);
// 			if (stat(filepath, &statbuf)) {
// 				LOGI("stat error!");
// 				continue;
// 			}
// 			printf("---filepath = %s\n",filepath);
// 			if(start>=0 && count >=0 ){
// 				if(i++<start){
// 					continue;
// 				}
// 				if(num++>=count){
// 					break;
// 				}
// 			}
// //			LOGI("num:%d", num);
// 			int strl = strlen(buf);
// 			//printf("stat len:%d\n",strl);
// 			int var_len = sizeof(statbuf.st_size);
// 			unsigned int l_size = 0;    //大小的高位
// 			unsigned int h_size = 0;    //大小的低位

// 			if(8 == var_len)
// 			{
// 			    uint64_t st_size = (uint64_t)statbuf.st_size;
//                 l_size = (unsigned int)(st_size&0xffffffff);
//                 h_size = (unsigned int)(st_size >> 32);
// 			}
// 			else
// 			{
//                 l_size = (unsigned int)(statbuf.st_size&0xffffffff);
//                 h_size = 0;
// 			}
// //            LOGI("statbuf.st_size=0x%x,len=%d,l_size=0x%x,h_size=0x%x",statbuf.st_size,sizeof(statbuf.st_size),l_size,h_size);
            
// 			if(buf[strl-3] == '['){
// 				buf[strl-2] = '\0';
// 				sprintf(tmpbuf,"%s{\"t\":%d,\"n\":\"%s\",\"sl\":%u,\"sh\":%u}]}",
// 					buf,ptr->d_type,ptr->d_name,l_size,h_size);
// 				strncpy(buf,tmpbuf,len);
// 			}else{
// 				buf[strl-2] = '\0';
// 				sprintf(tmpbuf,"%s,{\"t\":%d,\"n\":\"%s\",\"sl\":%u,\"sh\":%u}]}",
// 					buf,ptr->d_type,ptr->d_name,l_size,h_size);
// 				strncpy(buf,tmpbuf,len);
// 			}
// 		}
//     }

// error:
//     closedir(dir);
// 	free(tmpbuf);
// 	return 0;
// }

cJSON * list_file(const char*dir_path,const char*dir_name,int recurse){
	cJSON *json;
	DIR *dir;
    struct dirent *ptr; 
	struct stat statbuf;
   // int num = 0, i = 0;
	char filepath[MAX_FILE_PATH];
	
	dir=opendir(dir_path);
    if (dir == NULL){
        perror("opendir error:");
        LOGI("line %d :list_file Open dir %s error...",__LINE__,dir_path); 
        return NULL;        
    }
	json = cJSON_CreateObject();
	if(!json){
		return NULL;
	}
	
	cJSON_AddNumberToObject(json, "t", DT_DIR);
	cJSON_AddStringToObject(json, "n", dir_name);
	cJSON *sa = cJSON_CreateArray();
	if(!sa){
		return json;
	}
    while ((ptr=readdir(dir))){//ptr!=NULL
		cJSON *subitem;
		if(ptr->d_type&DT_DIR){
			LOGI("dir_path = %s",dir_path);
			if(ptr->d_name == NULL || ptr->d_name[0] == '.' || !strcmp(ptr->d_name,".."))
				continue;
			sprintf(filepath,"%s/%s",dir_path,ptr->d_name);
			//printf("open[%s]\n",filepath);
			int rec = recurse-1;
			if(rec>0){
				subitem = list_file(filepath,ptr->d_name,rec);
				if(subitem){
					cJSON_AddItemToArray(sa, subitem);
				}
			}else{
				subitem = cJSON_CreateObject();
				if(subitem){
					cJSON_AddNumberToObject(subitem, "t", ptr->d_type);
					cJSON_AddStringToObject(subitem, "n", ptr->d_name);

					cJSON *ssa = cJSON_CreateArray();
					cJSON_AddItemToObject(subitem, "l", ssa);

					cJSON_AddItemToArray(sa, subitem);
				}
			}
		}else if(ptr->d_type&DT_REG){
			sprintf(filepath,"%s/%s",dir_path,ptr->d_name);
			if (stat(filepath, &statbuf)) {
				LOGI("stat error!");
				break;
			}
			//printf("\n\nino:%u\t offset:%u\t file length:%u\t file type:%u\t file name:%s/%s\n",
			//	ptr->d_ino, ptr->d_off,ptr->d_reclen,ptr->d_type,dir_path,ptr->d_name);
			//printf("size:%u\t st_atime:%u\t st_mtime:%u\t st_ctime:%u\n",
			//	statbuf.st_size,statbuf.st_atime,statbuf.st_mtime,statbuf.st_ctime);
			subitem = cJSON_CreateObject();
			if(subitem){
				cJSON_AddNumberToObject(subitem, "t",ptr->d_type);
				cJSON_AddStringToObject(subitem,"n",ptr->d_name);
				/*sprintf(filepath,"%lu",statbuf.st_size);
				cJSON_AddStringToObject(subitem,"s",filepath);
				sprintf(filepath,"%lu",statbuf.st_atime);
				cJSON_AddStringToObject(subitem,"at",filepath);
				sprintf(filepath,"%lu",statbuf.st_mtime);
				cJSON_AddStringToObject(subitem,"mt",filepath);
				sprintf(filepath,"%lu",statbuf.st_ctime);
				cJSON_AddStringToObject(subitem,"ct",filepath);

				
				cJSON_AddNumberToObject(subitem, "s",(unsigned)statbuf.st_size);
				cJSON_AddNumberToObject(subitem,"at",(unsigned)statbuf.st_atime);
				cJSON_AddNumberToObject(subitem,"mt",(unsigned)statbuf.st_mtime);
				cJSON_AddNumberToObject(subitem,"ct",(unsigned)statbuf.st_ctime);*/

				cJSON_AddItemToArray(sa, subitem);
			}
		}
    }
	cJSON_AddItemToObject(json, "l",sa);
    closedir(dir);
	return json;
}

//mode:0:file only,1:include directory.
int get_dir_file_num(const char*path,int mode,int start,int end)
{
	DIR *dir;
    struct dirent *ptr;
    int num = 0;

	LOGI("recorder:0 path[%s] mode:%d start:%d end:%d",path, mode, start, end);
	dir=opendir(path);
    if (dir == NULL){
        LOGI("line %d get_dir_file_num Open dir error..."); 
        perror("opendir error:");
        return -1;        
    }
    LOGI("start to read dir!");
    
	while ((ptr=readdir(dir))){//ptr!=NULL
		if(ptr->d_type&DT_DIR){
			if(mode < 1 || ptr->d_name == NULL || ptr->d_name[0] == '.' || !strcmp(ptr->d_name, ".."))
				continue;
			num++;
		}else if(ptr->d_type&DT_REG){
            if(NEED_IGNORE_FILES(ptr->d_name))    //正在录像的文件，不允许被搜索出来播放
            {
                LOGI("search recording file:%s ignore it", ptr->d_name); 
                continue;
            }
		    
			if(ptr->d_name!=NULL&&ptr->d_name[6]=='-'){
				char startStr[8];
				char endStr[8] = "0";
				int nl = strlen(ptr->d_name);
				strncpy(startStr,ptr->d_name,6);
				if(nl>14)//6+6+1+1
					strncpy(endStr,&ptr->d_name[7],6);
				int cs = atoi(startStr);
				int ce = atoi(endStr);
				if(ce == 0){//assume the recorded file has been completed.
					int sh = cs/10000;
					int sm = cs%10000;
					int ss = sm%100;
					sm = sm/100;

					int rh = 0;//recorder->length/10000;
					int rm = 0;//recorder->length%10000;
					int rs = rm%100;
					rm = rm/100;

					int eh = sh + rh;
					int em = sm + rm;
					int es = ss + rs;

					if(eh >= 24){
						eh = 24;
						em = es = 0;
					}
					
					ce = eh*10000 + em*100 + es;
				}
				if((start<=cs && end>=cs) || (start<=ce && end>=ce) || (cs<start && ce>end))
					num++;
			}
		}
	}
	closedir(dir);//added in@201601241121

	return num;
}

int get_folder_free_space(const char *path,disk_info_t *info)
{
    struct statfs diskInfo;
	if(info == NULL)
		return -1;
	
	if(access(path,F_OK)<0)
	{
		info->size = 0;
    		info->free = 0;
		info->available = 0;
		return -1;
	}
	
    statfs(path,&diskInfo);
    unsigned long long blocksize = diskInfo.f_bsize;
	info->size = diskInfo.f_blocks*blocksize;
    info->free = diskInfo.f_bfree*blocksize;
	info->available = diskInfo.f_bavail*blocksize;
	
	LOGI("total:%llu[%lluM] free:%llu[%lluM] available:%llu[%lluM]",info->size,info->size>>20
		,info->free,info->free>>20,info->available,info->available>>20);
	return 0;
}



int list_file_to_jsonstr(FileInfo_t *FileInfo,int len,int mode,int start,int count,const char*dir_path,const char*dir_name,int recurse)
{
	DIR *dir;
	int ret = 0;
    struct dirent *ptr; 
	struct stat statbuf;
    int num = 0, i = 0;
	char filepath[MAX_FILE_PATH];
	dir=opendir(dir_path);
    if (dir == NULL){
        perror("opendir error:");
        LOGI("LINE %d list_file_to_jsonstr Open dir %s error...",__LINE__,dir_path);
        return -1;
    }

    while ((ptr=readdir(dir))){//ptr!=NULL
		if(ptr->d_type&DT_DIR){
			//printf("dir_path = %s\n",dir_path);
			if(mode<1 || ptr->d_name == NULL || ptr->d_name[0] == '.' || !strcmp(ptr->d_name,".."))
				continue;
			if(start>=0 && count >=0 ){
				if(i++<start){
					continue;
				}
				if(num++>=count){
					break;
				}
			}
			sprintf(filepath,"%s/%s",dir_path,ptr->d_name);
			printf("open[%s]\n",filepath);
			int rec = recurse-1;
			if(rec>0){
				// if(list_file_to_jsonstr(FileInfo,len,mode,start,count,filepath,ptr->d_name,rec)<0){
				// 	ret = -1;
				// 	goto error;
				// }
			}else{
				// FileInfo->n
				// int strl = strlen(buf);
				// if(buf[strl-3] == '['){
				// 	buf[strl-2] = '\0';
				// 	sprintf(tmpbuf,"%s{\"t\":%d,\"n\":\"%s\",\"l\":[]}]}",buf,ptr->d_type,ptr->d_name);
				// 	strncpy(buf,tmpbuf,len);
				// }else{
				// 	buf[strl-2] = '\0';
				// 	sprintf(tmpbuf,"%s,{\"t\":%d,\"n\":\"%s\",\"l\":[]}]}",buf,ptr->d_type,ptr->d_name);
				// 	strncpy(buf,tmpbuf,len);
				// }

				// FileInfo[num]->t = ptr->d_type;
				// strcpy(FileInfo[num]->n, ptr->d_name);
				// // strcpy(FileInfo->n, "12-32.pes");
				// FileInfo[num]->sl = 10;
				// FileInfo[num]->sh = 0;

				FileInfo[num].t = ptr->d_type;
				strcpy(FileInfo[num].n, ptr->d_name);
				// strcpy(FileInfo->n, "12-32.pes");
				FileInfo[num].sl = 10;
				FileInfo[num].sh = 0;
			}
		}
		else if(ptr->d_type&DT_REG){
			if(NEED_IGNORE_FILES(ptr->d_name))    //正在录像的文件，不允许被搜索出来播放
            {
                LOGI("search recording file:%s ignore it", ptr->d_name); 
                continue;
            }
            
			sprintf(filepath,"%s/%s",dir_path,ptr->d_name);
			if (stat(filepath, &statbuf)) {
				LOGI("stat error!");
				continue;
			}
			printf("---filepath = %s\n",filepath);
			if(start>=0 && count >=0 ){
				if(i++<start){
					continue;
				}
				if(num++>=count){
					break;
				}
			}
			LOGI("num:%d", num);
			// int strl = strlen(buf);
			//printf("stat len:%d\n",strl);
			int var_len = sizeof(statbuf.st_size);
			unsigned int l_size = 0;    //大小的高位
			unsigned int h_size = 0;    //大小的低位

			if(8 == var_len)
			{
			    uint64_t st_size = (uint64_t)statbuf.st_size;
                l_size = (unsigned int)(st_size&0xffffffff);
                h_size = (unsigned int)(st_size >> 32);
			}
			else
			{
                l_size = (unsigned int)(statbuf.st_size&0xffffffff);
                h_size = 0;
			}
//            LOGI("statbuf.st_size=0x%x,len=%d,l_size=0x%x,h_size=0x%x",statbuf.st_size,sizeof(statbuf.st_size),l_size,h_size);
            
			// if(buf[strl-3] == '['){
			// 	buf[strl-2] = '\0';
			// 	sprintf(tmpbuf,"%s{\"t\":%d,\"n\":\"%s\",\"sl\":%u,\"sh\":%u}]}",
			// 		buf,ptr->d_type,ptr->d_name,l_size,h_size);
			// 	strncpy(buf,tmpbuf,len);
			// }else{
			// 	buf[strl-2] = '\0';
			// 	sprintf(tmpbuf,"%s,{\"t\":%d,\"n\":\"%s\",\"sl\":%u,\"sh\":%u}]}",
			// 		buf,ptr->d_type,ptr->d_name,l_size,);
			// 	strncpy(buf,tmpbuf,len);
			// }
			FileInfo[num].t = ptr->d_type;
			strcpy(FileInfo[num].n, ptr->d_name);
			// strcpy(FileInfo->n, "12-32.pes");
			FileInfo[num].sl = l_size;
			FileInfo[num].sh = h_size;
		}
    }

error:
    closedir(dir);
	return 0;
}



//mode:0:file only,1:include directory.
int get_dir_file_list(FileInfo_t *FileInfo, int count, const char*path,int mode,int start,int end)
{
	DIR *dir;
    struct dirent *ptr;
    int num = 0;

	LOGI("recorder:0 path[%s] mode:%d start:%d end:%d",path, mode, start, end);
	dir=opendir(path);
    if (dir == NULL){
        LOGI("line %d get_dir_file_num Open dir error..."); 
        perror("opendir error:");
        return -1;        
    }
    LOGI("start to read dir!");
    
	while ((ptr=readdir(dir))){//ptr!=NULL
		if(ptr->d_type&DT_DIR){
			if(mode < 1 || ptr->d_name == NULL || ptr->d_name[0] == '.' || !strcmp(ptr->d_name, ".."))
				continue;
			// num++;
		}else if(ptr->d_type&DT_REG){
            if(NEED_IGNORE_FILES(ptr->d_name))    //正在录像的文件，不允许被搜索出来播放
            {
                LOGI("search recording file:%s ignore it", ptr->d_name); 
                continue;
            }
		    
			if(ptr->d_name!=NULL&&ptr->d_name[6]=='-'){
				// printf("1---filepath = %s\n",ptr->d_name);
				char startStr[8];
				char endStr[8] = "0";
				int nl = strlen(ptr->d_name);
				strncpy(startStr,ptr->d_name,6);
				if(nl>14)//6+6+1+1
					strncpy(endStr,&ptr->d_name[7],6);
				int cs = atoi(startStr);
				int ce = atoi(endStr);
				// printf("start :%d,end :%d\n",cs, ce);
				if(ce == 0){//assume the recorded file has been completed.
					int sh = cs/10000;
					int sm = cs%10000;
					int ss = sm%100;
					sm = sm/100;

					int rh = 0;//recorder->length/10000;
					int rm = 0;//recorder->length%10000;
					int rs = rm%100;
					rm = rm/100;

					int eh = sh + rh;
					int em = sm + rm;
					int es = ss + rs;

					if(eh >= 24){
						eh = 24;
						em = es = 0;
					}
					
					ce = eh*10000 + em*100 + es;
				}
				if((start<=cs && end>=cs) || (start<=ce && end>=ce) || (cs<start && ce>end))
				{
					
					char filepath[MAX_FILE_PATH];
					struct stat statbuf;
					sprintf(filepath,"%s/%s",path,ptr->d_name);
					if (stat(filepath, &statbuf)) {
						LOGI("stat error!");
						continue;
					}
					// printf("---filepath = %s\n",filepath);
					if(num>=count){
						break;
					}
					// LOGI("num:%d", num);
					// int strl = strlen(buf);
					//printf("stat len:%d\n",strl);
					int var_len = sizeof(statbuf.st_size);
					unsigned int l_size = 0;    //大小的高位
					unsigned int h_size = 0;    //大小的低位

					if(8 == var_len)
					{
						uint64_t st_size = (uint64_t)statbuf.st_size;
						l_size = (unsigned int)(st_size&0xffffffff);
						h_size = (unsigned int)(st_size >> 32);
					}
					else
					{
						l_size = (unsigned int)(statbuf.st_size&0xffffffff);
						h_size = 0;
					}
					FileInfo[num].t = ptr->d_type;
					strcpy(FileInfo[num].n, ptr->d_name);
					FileInfo[num].sl = l_size;
					FileInfo[num].sh = h_size;
					num++;
				}
			}
		}
	}
	closedir(dir);//added in@201601241121

	return num;
}

//add by lwj
static void sortStruct(VideoFileDayInfo_t *a, int len)//a为数组地址，l为数组长度。
{
    int i, j;
    VideoFileDayInfo_t tmp;
    for(i = 0; i < len - 1; i ++)
	{
        for(j = i+1; j < len; j ++)
        {
            // printf("-------[lwj test]sortStruct cs1:%d cs2:%d\n",cs1, cs2);
            if(a[i].stTime > a[j].stTime)//如前面的比后面的大，则交换。
            {
                // printf("-------[lwj test]change\n");
                memcpy(&tmp, &a[i], sizeof(VideoFileDayInfo_t));
                memcpy(&a[i], &a[j], sizeof(VideoFileDayInfo_t));
                memcpy(&a[j], &tmp, sizeof(VideoFileDayInfo_t));
            }
        }
	}
}

// int getFileListToQueue(FileInfo_t *FileInfo, const char*path, int mode,int start,int end)
// {
// 	DIR *dir;
//     struct dirent *ptr;
//     int num = 0;
	
// 	memset(&gVideoFileInfo, 0, sizeof(gVideoFileInfo));
// 	LOGI("recorder:0 path[%s] mode:%d start:%d end:%d",path, mode, start, end);
// 	dir=opendir(path);
//     if (dir == NULL){
//         LOGI("line %d get_dir_file_num Open dir error..."); 
//         perror("opendir error:");
//         return -1;        
//     }
//     LOGI("start to read dir!");
    
// 	while ((ptr=readdir(dir))){//ptr!=NULL
// 		if(ptr->d_type&DT_DIR){
// 			if(mode < 1 || ptr->d_name == NULL || ptr->d_name[0] == '.' || !strcmp(ptr->d_name, ".."))
// 				continue;
// 			// num++;
// 		}else if(ptr->d_type&DT_REG){
//             if(NEED_IGNORE_FILES(ptr->d_name))    //正在录像的文件，不允许被搜索出来播放
//             {
//                 LOGI("search recording file:%s ignore it", ptr->d_name); 
//                 continue;
//             }
		    
// 			if(ptr->d_name!=NULL&&ptr->d_name[6]=='-'){
// 				// printf("1---filepath = %s\n",ptr->d_name);
// 				char startStr[8];
// 				char endStr[8] = "0";
// 				int nl = strlen(ptr->d_name);
// 				strncpy(startStr,ptr->d_name,6);
// 				if(nl>14)//6+6+1+1
// 					strncpy(endStr,&ptr->d_name[7],6);
// 				int cs = atoi(startStr);
// 				int ce = atoi(endStr);
// 				// printf("start :%d,end :%d\n",cs, ce);
// 				if(ce == 0){//assume the recorded file has been completed.
// 					int sh = cs/10000;
// 					int sm = cs%10000;
// 					int ss = sm%100;
// 					sm = sm/100;

// 					int rh = 0;//recorder->length/10000;
// 					int rm = 0;//recorder->length%10000;
// 					int rs = rm%100;
// 					rm = rm/100;

// 					int eh = sh + rh;
// 					int em = sm + rm;
// 					int es = ss + rs;

// 					if(eh >= 24){
// 						eh = 24;
// 						em = es = 0;
// 					}
					
// 					ce = eh*10000 + em*100 + es;
// 				}
// 				if((start<=cs && end>=cs) || (start<=ce && end>=ce) || (cs<start && ce>end))
// 				{
					
// 					char filepath[MAX_FILE_PATH];
// 					struct stat statbuf;
// 					sprintf(filepath,"%s/%s",path,ptr->d_name);
// 					if (stat(filepath, &statbuf)) {
// 						LOGI("stat error!");
// 						continue;
// 					}
// 					// printf("---filepath = %s\n",filepath);
// 					if(num>=1024){
// 						break;
// 					}
// 					gVideoFileInfo.fileList[num].stTime = cs;
// 					gVideoFileInfo.fileList[num].eTime = ce;
// 					num++;
// 				}

// 			}
// 		}
// 	}
// 	// for (int i = 0; i < num; i++)
// 	// {
// 	// 	printf("%d=%d ",i, gVideoFileInfo[i].stTime);
// 	// }
// 	// printf("\n\n");

// 	closedir(dir);//added in@201601241121
// 	sortStruct(&gVideoFileInfo, num);

// 	for (int i = 0; i < num; i++)
// 	{
// 		printf("%d=%d ",i, gVideoFileInfo.fileList[i].stTime);
// 	}
// 	printf("\n\n");
// 	int j = 0;
// 	sprintf(&(FileInfo[0].n[0]),"%06d",gVideoFileInfo.fileList[j].stTime);
// 	for (int i = 0; i < num-1; i++)
// 	{
// 		if(abs(gVideoFileInfo.fileList[i].eTime-gVideoFileInfo.fileList[i+1].stTime)<=1)
// 		{
// 			continue;
// 		}
// 		else 
// 		{
// 			sprintf(&(FileInfo[j].n[6]),"-%06d.pes",gVideoFileInfo.fileList[i].eTime);
// 			j++;
// 			if (j>=20)
// 				break;			
// 			sprintf(&(FileInfo[j].n[0]),"%06d",gVideoFileInfo.fileList[i+1].stTime);
// 		}
// 	}
// 	printf("test3  j=%d\n",j);
// 	for (size_t i = 0; i < j; i++)
// 	{
// 		printf("[%s]",FileInfo[i].n);
// 	}
// 		printf("\n");
	
// 	sprintf(&(FileInfo[j].n[6]),"-%06d.pes",gVideoFileInfo.fileList[num-1].eTime);
// 	return num;
// }



int standard_to_stamp(char *str_time)  
{  
        struct tm stm;  
        int iY, iM, iD, iH, iMin, iS;  

        memset(&stm,0,sizeof(stm));  
        iY = atoi(str_time);  
        iM = atoi(str_time+5);  
        iD = atoi(str_time+8);  
        iH = atoi(str_time+11);  
        iMin = atoi(str_time+14);  
        iS = atoi(str_time+17);  

        stm.tm_year=iY-1900;  
        stm.tm_mon=iM-1;  
        stm.tm_mday=iD;  
        stm.tm_hour=iH;  
        stm.tm_min=iMin;  
        stm.tm_sec=iS;  

        /*printf("%d-%0d-%0d %0d:%0d:%0d\n", iY, iM, iD, iH, iMin, iS);*/   //标准时间格式例如：2016:08:02 12:12:30
        return (int)mktime(&stm);  
}  


int syncFileListToArray(VideoFileInfo_t *FileInfo, int mode, int reqDate)	//reqDate 0:当天	20221022:2022年10月22日
{
	DIR *dir;
    struct dirent *ptr;
    int num = 0;
	char *rootDir="/mnt/s0/media/sensor0/";
	char path[128];
	int start,end;
	int startSize = 0;
	memset(FileInfo, 0, sizeof(VideoFileInfo_t));
	start = 000000;
	end = 235959;
	// LOGI("FileInfo->date=%d  reqDate=%d\n", FileInfo->date, reqDate);
	if (reqDate == 0)
	{
		struct tm *time_p;
		struct tm result;
		time_p = &result;
		time_t tm;
		time(&tm);
		tm += 8*3600;
		time_p = localtime_r(&tm, &result);
		sprintf(path, "%s%04hd%02d/%02d",
			rootDir, time_p->tm_year + 1900, time_p->tm_mon + 1, time_p->tm_mday);
		FileInfo->date = (time_p->tm_year+1900)*100*100 + (time_p->tm_mon + 1)*100 + time_p->tm_mday;
	}
	else
	{
		sprintf(path, "%s%04hd%02d/%02d",
			rootDir, reqDate/10000, (reqDate/100)%100, reqDate%100);
		FileInfo->date = reqDate;
	}
	
	LOGI("recorder:0 path[%s] mode:%d start:%d end:%d",path, mode, start, end);
	dir=opendir(path);
	if (dir == NULL){
		LOGI("line %d get_dir_file_num Open dir error..."); 
		perror("opendir error:");
		return -1;        
	}
	// LOGI("start to read dir!");
	while ((ptr=readdir(dir))){//ptr!=NULL
		if(ptr->d_type&DT_DIR){
			if(mode < 1 || ptr->d_name == NULL || ptr->d_name[0] == '.' || !strcmp(ptr->d_name, ".."))
				continue;
			// num++;
		}else if(ptr->d_type&DT_REG){
			if(NEED_IGNORE_FILES(ptr->d_name))    //正在录像的文件，不允许被搜索出来播放
			{
				LOGV("search recording file:%s ignore it", ptr->d_name); 
				continue;
			}
			
			if(ptr->d_name!=NULL&&ptr->d_name[6]=='-'){
				// printf("1---filepath = %s\n",ptr->d_name);
				char startStr[8];
				char endStr[8] = "0";
				int nl = strlen(ptr->d_name);
				strncpy(startStr,ptr->d_name,6);
				if(nl>14)//6+6+1+1
					strncpy(endStr,&ptr->d_name[7],6);
				int cs = atoi(startStr);
				int ce = atoi(endStr);
				// printf("start :%d,end :%d\n",cs, ce);
				if(ce == 0){//assume the recorded file has been completed.
					int sh = cs/10000;
					int sm = cs%10000;
					int ss = sm%100;
					sm = sm/100;

					int rh = 0;//recorder->length/10000;
					int rm = 0;//recorder->length%10000;
					int rs = rm%100;
					rm = rm/100;

					int eh = sh + rh;
					int em = sm + rm;
					int es = ss + rs;

					if(eh >= 24){
						eh = 24;
						em = es = 0;
					}
					
					ce = eh*10000 + em*100 + es;
				}
				// if(start<=cs && end>=cs)
				if((start<=cs && end>=cs) || (start<=ce && end>=ce) || (cs<start && ce>end))
				{
					
					char filepath[MAX_FILE_PATH];
					struct stat statbuf;
					sprintf(filepath,"%s/%s",path,ptr->d_name);
					if (stat(filepath, &statbuf)) {
						LOGI("stat error!");
						continue;
					}
					if (cs > ce)	//开始时间大于结束时间 跨天
					{
						continue;
					}
					
					// printf("---filepath = %s\n",filepath);
					if(num>=MAX_VIDEOS_IN_A_DAY){
						break;
					}
					FileInfo->fileList[num].stTime = cs;
					FileInfo->fileList[num].eTime = ce;
					num++;
				}
			}
		}
	}
	closedir(dir);//added in@201601241121
	sortStruct(&(FileInfo->fileList[startSize]), num-startSize);
	startSize = num;
	FileInfo->fileTotal = num;
	// for (int i = 0; i < num; i++)
	// {
	// 	printf("%d=[%d-%d] ",i, FileInfo->fileList[i].stTime, FileInfo->fileList[i].eTime);
	// }
	// printf("\n\n");

	return num;
}

int videoFileSeek(VideoFileInfo_t *pFileInfo, int time, char *outFilename, int *dayTimesStamp, int *playingFN)	//用于seek请求
{
	int seekTime = time;
	if (NULL == pFileInfo)
	{
		LOGI("pFileInfo NULL");
		return -1;
	}
	LOGI("[lwj test]req date=%d", pFileInfo->date);
	
	if (pFileInfo->fileTotal<0)
	{
		return -1;
	}
	// int len = sprintf(outFilename, "%s%06d/%02d/", get_common_record_path(), 
	// 			pFileInfo->date/100, pFileInfo->date%100);
	// if (date == 0)
	// {
	// 	seekTime = pFileInfo[pFileInfo->fileTotal].fileList->stTime;
	// }
	//  int len = sprintf(outFilename, "%s%d%02d/%02d/", get_common_record_path(), 
    //             playrecord_req_p->Date.Year, playrecord_req_p->Date.Mon, playrecord_req_p->Date.Day);
		
	LOGI("[lwj test]date[%d] fileTotal[%d] seekTime=%d", pFileInfo->date, pFileInfo->fileTotal, seekTime);
	for(int i=0; i<pFileInfo->fileTotal; i++)
	{
		if ((pFileInfo->fileList[i].stTime<=seekTime) && (seekTime<pFileInfo->fileList[i].eTime))
		{
			sprintf(outFilename, "%06d-%06d.pes",pFileInfo->fileList[i].stTime, pFileInfo->fileList[i].eTime);
			*playingFN = i;
			LOGI("\n[lwj test]outFilename:%s\n", outFilename);
			*dayTimesStamp = (seekTime/10000)*3600+(seekTime/100)%100*60+seekTime%100;
			return seekTime-pFileInfo->fileList[i].stTime;
		}
		else if (((seekTime<pFileInfo->fileList[i].eTime) ||(pFileInfo->fileList[i].stTime>seekTime)) && (i < pFileInfo->fileTotal))
		{
			sprintf(outFilename, "%06d-%06d.pes",pFileInfo->fileList[i].stTime, pFileInfo->fileList[i].eTime);
			*playingFN = i;
			LOGI("\n[lwj test]seek fail get next outFilename:%s\n", outFilename);
			seekTime = pFileInfo->fileList[i].stTime;
			*dayTimesStamp = (seekTime/10000)*3600+(seekTime/100)%100*60+seekTime%100;
			return 0;
		}
	}
	LOGI("not find file");
	return -1;
}

int videoFileSeek2(VideoFileInfo_t *pFileInfo, int time, char *outFilename ,int *dayTimesStamp, int *playingFN)	//用于start recorder请求
{
	int seekTime = time;
	if (NULL == pFileInfo)
	{
		LOGI("getCurVideolist fail");
		return -1;
	}
	LOGI("[lwj test]req date=%d", pFileInfo->date);

	char *filename = NULL;
	#if 0
	filename = outFilename
	#else
	int len = sprintf(outFilename, "%s%06d/%02d/", get_common_record_path(), 
				pFileInfo->date/100, pFileInfo->date%100);
	filename = outFilename+len;
	#endif
	
	LOGI("[lwj test]date=%d seekTime=%d",pFileInfo->date, seekTime);

	for(int i=0; i<pFileInfo->fileTotal; i++)
	{
		if ((pFileInfo->fileList[i].stTime<=seekTime) && (seekTime<pFileInfo->fileList[i].eTime))
		{
			sprintf(filename, "%06d-%06d.pes",pFileInfo->fileList[i].stTime, pFileInfo->fileList[i].eTime);
			*playingFN = i;
			LOGI("\n[lwj test] seek success outFilename:%s\n", filename);
			*dayTimesStamp = (seekTime/10000)*3600+(seekTime/100)%100*60+seekTime%100;
			return seekTime-pFileInfo->fileList[i].stTime;
		}
		else if ((seekTime<pFileInfo->fileList[i].eTime) && (i < pFileInfo->fileTotal))
		{
			sprintf(filename, "%06d-%06d.pes",pFileInfo->fileList[i].stTime, pFileInfo->fileList[i].eTime);
			*playingFN = i;
			LOGI("\n[lwj test]seek fail get next outFilename:%s\n", filename);
			seekTime = pFileInfo->fileList[i].stTime;
			*dayTimesStamp = (seekTime/10000)*3600+(seekTime/100)%100*60+seekTime%100;
			return 0;
		}
	}
	sprintf(filename, "%06d-%06d.pes",pFileInfo->fileList[pFileInfo->fileTotal-1].stTime, pFileInfo->fileList[pFileInfo->fileTotal-1].eTime);
	*playingFN = pFileInfo->fileTotal-1;
	LOGI("\n-------------------------------------------[lwj test] seek fail get end outFilename:%s\n", filename);
	seekTime = pFileInfo->fileList[pFileInfo->fileTotal-1].stTime;
	*dayTimesStamp = (seekTime/10000)*3600+((seekTime/100)%100)*60+seekTime%100;
	return 0;
}
//add end by lwj

// added by lishun
int listVideoFileInDir(char *dirPath, VideoFileInfo_t *FileInfo, int *pVideoCap)
{
	T_U64 videoCap = 0;		// in megabyte

	int start = 0;
	int end = 235959;
	int num = 0;

	struct dirent *ptr;
	DIR *dir=opendir(dirPath);
	if (NULL == dir){
		LOGE("open dir file, dirPath is %s", dirPath); 
		return -1;        
	}
	LOGI("start to read dir!");
	while ((ptr=readdir(dir))){//ptr!=NULL
		if(DT_DIR == ptr->d_type ){
			continue;
		}
		else if(DT_REG == ptr->d_type){
			if(NEED_IGNORE_FILES(ptr->d_name))    //正在录像的文件，不允许被搜索出来播放
			{
				LOGV("search recording file:%s ignore it", ptr->d_name); 
				continue;
			}
		#if 0
			if(NULL != ptr->d_name && '-' == ptr->d_name[6]){	
				char startStr[8];
				char endStr[8] = "0";
				int nl = strlen(ptr->d_name);
				strncpy(startStr,ptr->d_name,6);
				if(nl>14)//6+6+1+1
					strncpy(endStr,&ptr->d_name[7],6);
				int cs = atoi(startStr);
				int ce = atoi(endStr);
				// printf("start :%d,end :%d\n",cs, ce);
				if(0 == ce){//assume the recorded file has been completed.
					int sh = cs/10000;
					int sm = cs%10000;
					int ss = sm%100;
					sm = sm/100;

					int rh = 0;//recorder->length/10000;
					int rm = 0;//recorder->length%10000;
					int rs = rm%100;
					rm = rm/100;

					int eh = sh + rh;
					int em = sm + rm;
					int es = ss + rs;

					if(eh >= 24){
						eh = 24;
						em = es = 0;
					}
					
					ce = eh*10000 + em*100 + es;
				}
				// if(start<=cs && end>=cs)
				if((start<=cs && end>=cs) || (start<=ce && end>=ce) || (cs<start && ce>end))
				{
					char filepath[MAX_FILE_PATH];
					struct stat statbuf;
					sprintf(filepath,"%s/%s",dirPath,ptr->d_name);
					if (stat(filepath, &statbuf)) {
						LOGI("stat error!");
						continue;
					}
					if (cs > ce)	//开始时间大于结束时间 跨天
					{
						continue;
					}
					
					// printf("---filepath = %s\n",filepath);
					if(num>=MAX_VIDEOS_IN_A_DAY){
						break;
					}
					FileInfo->fileList[num].stTime = cs;
					FileInfo->fileList[num].eTime = ce;
					videoCap += (T_U64)statbuf.st_size;
					// LOGI("[%s]videoCap is %d", ptr->d_name, videoCap);
					num++;
				}
			}
		#else
			// LOGI("[lwj test]------[%s]", ptr->d_name);
			if(NULL != ptr->d_name && '-' == ptr->d_name[2] && '-' == ptr->d_name[5]){	
				char hhStr[3] = {0};
				char mmStr[3] = {0};
				char ssStr[3] = {0};
				int nl = strlen(ptr->d_name);
				// LOGI("[lwj test]------len[%d]",nl);

				strncpy(hhStr,ptr->d_name,2);
				strncpy(mmStr,&ptr->d_name[3],2);
				strncpy(ssStr,&ptr->d_name[6],2);
				// LOGI("hh:%s mm:%s ss:%s", hhStr, mmStr, ssStr);
				if (nl != 12)	 //01-00-00.pes 01-00-00.idx
				{
					continue;
				}
				int	hh = atoi(hhStr);
				int	mm = atoi(mmStr);
				int	ss = atoi(ssStr);
				if ((00<=hh && hh<24) && (00<=mm && mm<60) && (00<=ss && ss<60))
				{
					char filepath[MAX_FILE_PATH];
					struct stat statbuf;
					sprintf(filepath,"%s/%s",dirPath,ptr->d_name);
					// LOGI("file:%s",filepath);
					if (stat(filepath, &statbuf)) {
						LOGI("stat error!");
						continue;
					}
					videoCap += (T_U64)statbuf.st_size;
					num++;
				}
			}
		#endif
		}
	}
	*pVideoCap = videoCap >> 20;	//Byte --> Megabyte
	closedir(dir);//added in@201601241121
	// sortStruct(&(FileInfo->fileList[0]), num);
	// FileInfo->fileTotal = num;
	// for (int i = 0; i < num; i++)
	// {
	// 	printf("%d=[%d-%d] ",i, FileInfo->fileList[i].stTime, FileInfo->fileList[i].eTime);
	// }
	// printf("\n\n");
	LOGI("pVideoCap[%d]",*pVideoCap);
	return num;
}
// end by lishun