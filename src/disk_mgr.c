#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/vfs.h>
#include "disk_mgr.h"
#include "logServer.h"
#include "recorder_params.h"

#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

static char gDiskRoot[64] = {0};
static char gCommRecRoot[64] = {0};
static char gAlarmRecRoot[64] = {0};

const char *get_alarm_record_path()
{
    return gAlarmRecRoot;
}

const char *get_common_record_path()
{
    return gCommRecRoot;
}

const char *get_disk_root_path()
{
    return gDiskRoot;
}

int init_record_path(char *diskRoot, char *commRecRoot, char *alarmRecRoot)
{
    LOGI("diskRoot:%s commRecRoot:%s alarmRecRoot:%s",
        diskRoot, commRecRoot, alarmRecRoot);
    if(diskRoot && strlen(diskRoot))
    {
        strncpy(gDiskRoot, diskRoot, sizeof(gDiskRoot)-1);
        if(gDiskRoot[strlen(gDiskRoot)-1] != '/')
        {
            strcpy(&gDiskRoot[strlen(gDiskRoot)], "/");
        }
    }
    else
    {
        strncpy(gDiskRoot, DEFAULT_MMC_ROOT_PATH, sizeof(gDiskRoot)-1);
    }
    
    if(commRecRoot && strlen(commRecRoot))
    {
        strncpy(gCommRecRoot, commRecRoot, sizeof(gCommRecRoot)-1);
        if(gCommRecRoot[strlen(gCommRecRoot)-1] != '/')
        {
            strcpy(&gCommRecRoot[strlen(gCommRecRoot)], "/");
        }
    }
    else
    {
        strncpy(gCommRecRoot, DEFAULT_COMMOM_REC_PATH, sizeof(gCommRecRoot)-1);
    }
    
    if(alarmRecRoot && strlen(alarmRecRoot))
    {
        strncpy(gAlarmRecRoot, alarmRecRoot, sizeof(gAlarmRecRoot)-1);
        if(gAlarmRecRoot[strlen(gAlarmRecRoot)-1] != '/')
        {
            strcpy(&gAlarmRecRoot[strlen(gAlarmRecRoot)], "/");
        }
    }
    else
    {
        strncpy(gAlarmRecRoot, DEFAULT_ALARM_REC_PATH, sizeof(gAlarmRecRoot)-1);
    }

    LOGI("gDiskRoot:%s gCommRecRoot:%s gAlarmRecRoot:%s",
        gDiskRoot, gCommRecRoot, gAlarmRecRoot);
    return 0;
}

//文件管理
/**return 0-not recording file,1-recording file**/
int is_recording_file_or_not(const char *path,char *d_name)
{ 
    int ret = 0;
	time_t timenow;
	struct stat statbuf;
	char filename[NAME_LEN] = {0};
	int LenPath = strlen(path);
	
    if(NULL != strstr(d_name,"-.avi"))    //*-.avi  表示正在录像的文�?这个不搜索出�?
    {
        if(path[LenPath-1] == '/')
        {
            sprintf(filename,"%s%s",path,d_name);
        }
        else
        {
            sprintf(filename,"%s/%s",path,d_name);
        }
    
        ret = stat(filename, &statbuf);         
        if(ret != 0)
        {
            LOGI("file %s stat error",filename);
            perror("stat error:");
            ret = 1;
        }
        timenow = time(NULL);
    
        LOGI("timenow=%ld,statbuf.st_ctime=%ld",timenow,statbuf.st_ctime);
        if((timenow-statbuf.st_ctime) < 3600)   //创建时间跟现在时间比�?
        {
            LOGI("file:%s is recording file",d_name);
            ret = 1;
        } 
    }
    
    return ret;
}

bool check_string_is_num(char *pstr,int len)
{
	bool result = TRUE;
	int i = 0;
	unsigned char val = 0;
	while(i < len)
	{
		val = *((unsigned char *)(pstr+i));
		i++;
		if((val < 0x30) || (val > 0x39))	//数字0-9
		{
			result = FALSE;	
			break;
		}
	}
	return result;
}

/****获取目录中文件列表信息，
path-路径�?
type:DT_DIR-查询path中的目录信息，DT_REG-查询path中的文件信息
pInFileList:pInFileList为空时，将去申请，不为空时，会将内容附到pInFileList后面�?
****/
DirFileListInfo_st *GetDirFileInfo(const char *path, DirFileListInfo_st *pInFileList, FileType_st type)
{
	int ret = -1;
	int i = 0;
	int counter = 0;
	DIR *dir;
	struct dirent *ptr;
	DirFileListInfo_st *pFileList = NULL;
	DirectoryFileInfo_st *pFileInfo = NULL;
	struct stat statbuf;
	char filename[NAME_LEN] = {0};
	int LenPath = 0;
	int FileType = DT_REG;
	DirectoryFileInfo_st *pNewAddr = NULL;
	int src_cnt = 0;
	char tmp_time[32] = {0};

	DirectoryFileInfo_st *ptmp = NULL;

	if(FILE_TYPE_DIR == type)
	{
		FileType = DT_DIR;
	}
	else
	{
		FileType = DT_REG;
	}
	
	LenPath = strlen(path);
	
	if((NULL == path) || (LenPath <= 0))
	{
		LOGI("path is NULL,or LenPath=%d",LenPath);
		return pInFileList;
	}
	
	LOGI("path[%s],File type=[0x%x]",path,FileType);
	/***查询文件个数***/
	dir=opendir(path);
	if(NULL == dir)
	{
        perror("opendir error:");
		LOGI("GetDirFileInfo Open dir:[%s] error",path); 
		return pInFileList;     
	}
	
	while(NULL != (ptr=readdir(dir)))
	{
		if(NULL == ptr)
		{
			LOGI("read ptr is NULL");
			break;
		}
		
		LOGV("ptr->d_type=0x%x,type=0x%x,name=%s",ptr->d_type,type,ptr->d_name);
		if(ptr->d_type&FileType)
		{
			//printf("d_type and type\n");
			if((strcmp(ptr->d_name,"..") == 0) || (strcmp(ptr->d_name,".") == 0))
			{
				//printf("d_name is .. or .\n");
				continue;
			}
			
			if(ptr->d_reclen > (NAME_LEN-LenPath))
			{
				LOGI("name len larger than buf len");
				continue;
			}
            
			if(FileType == DT_REG)
			{
			    if(is_recording_file_or_not(path,ptr->d_name))
			    {
                    LOGI("recording file,ignore it");
                    continue;
			    }                
			}
			
			LOGV("get a file name:%s",ptr->d_name);
			counter++;
		}
	}
	closedir(dir);

	LOGI("file counter=%d",counter);
	if(counter <= 0)
	{
		LOGI("counter=%d",counter);
		return pInFileList;
	}
	else
	{
		if(NULL == pInFileList)
		{
			pFileList = (DirFileListInfo_st *)malloc(sizeof(DirFileListInfo_st));
			LOGI("first time,malloc,pFileList=%p",pFileList);
			if(NULL == pFileList)
			{
				LOGI("malloc for pFileList fail");
				return NULL;
			}
			else
			{
				pFileList->counter = 0;
				pFileList->pFileStartAddr = NULL;
			}
		}
		else
		{
			pFileList = pInFileList;
		}

		//刚进来第一次申请的时候，才去申请pFileStartAddr
		if(NULL == pFileList->pFileStartAddr)
		{
			//没有目录或者文件，直接在内部释放，返回NULL
			LOGI("pFileList->counter=%d,pFileStartAddr=%p,file counter=%d",
				pFileList->counter,pFileList->pFileStartAddr,counter);
			pFileList->counter = counter;
			pFileList->pFileStartAddr = (DirectoryFileInfo_st *)malloc(sizeof(DirectoryFileInfo_st)*counter);
			memset((void *)pFileList->pFileStartAddr,0,sizeof(DirectoryFileInfo_st)*counter);
		}
		else
		{
			src_cnt = pFileList->counter;
			LOGI("src_cnt=%d,file counter=%d",src_cnt,counter);
#if 0			
			for(i=0; i<pFileList->counter; i++)
			{
				ptmp = pFileList->pFileStartAddr + i;
				LOGI("one i=%d,name=%s,time=%u",i,ptmp->name,ptmp->time);
			}
#endif			
			pNewAddr = (DirectoryFileInfo_st *)realloc((void *)pFileList->pFileStartAddr,sizeof(DirectoryFileInfo_st)*(src_cnt+counter));
			if(NULL == pNewAddr)	//没申请成功，不修改，直接返回
			{
				LOGI("realloc FAIL!src counter=%d,add counter=%d",src_cnt,counter);
				return pFileList;
			}

			//申请到的地址不一样，更新一下地址,原地址realloc已经释放，不用理�?
			LOGI("pNewAddr=%p,src addr=%p",pNewAddr,pFileList->pFileStartAddr);
			if(pNewAddr != pFileList->pFileStartAddr)
			{
				pFileList->pFileStartAddr = pNewAddr;
			}
#if 0
			for(i=0; i<pFileList->counter; i++)
			{
				ptmp = pFileList->pFileStartAddr + i;
				LOGI("two i=%d,name=%s,time=%u",i,ptmp->name,ptmp->time);
			}
#endif
			pFileList->counter = pFileList->counter + counter;
			memset((void *)(pFileList->pFileStartAddr + src_cnt),0,sizeof(DirectoryFileInfo_st)*counter);
			
			LOGI("pFileStartAddr=%p,src_cnt=%d,size=%d,offset addr=%p",
				pFileList->pFileStartAddr,src_cnt,sizeof(DirectoryFileInfo_st),(pFileList->pFileStartAddr + src_cnt));
#if 0			
			for(i=0; i<pFileList->counter; i++)
			{
				ptmp = pFileList->pFileStartAddr + i;
				LOGI("three i=%d,name=%s,time=%u",i,ptmp->name,ptmp->time);
			}
#endif
		}		
	}
	
	/****遍历所有文�?***/
	dir=opendir(path);
	if(dir == NULL)
	{
        perror("opendir error:");
		LOGI("line %d GetDirFileInfo Open dir [%s] error...",path); 
        //DirFileListFree(pFileList);
        //pFileList = NULL;
		return pFileList;        
	}

	i = 0;
	while(NULL != (ptr=readdir(dir)))
	{
		//printf("ptr->d_type=%x,type=%x,name=%s\n",ptr->d_type,FileType,ptr->d_name);
		if(ptr->d_type&FileType)
		{
			if((strcmp(ptr->d_name,"..") == 0) || (strcmp(ptr->d_name,".") == 0))
				continue;
			
			if(ptr->d_reclen > (NAME_LEN-LenPath))	//file name len out of len 
			{
				LOGI("name len larger than buf len");
				continue;
			}

			if(FileType == DT_REG)
			{
			    if(is_recording_file_or_not(path,ptr->d_name))
			    {
                    LOGI("recording file,ignore it");
                    continue;
			    }                
			}
			
			if(i > counter)
			{
				LOGI("i = %d,counter=%d,break",i,counter);
				break;
			}
			
			if(path[LenPath-1] == '/')
			{
				sprintf(filename,"%s%s",path,ptr->d_name);
			}
			else
			{
				sprintf(filename,"%s/%s",path,ptr->d_name);
			}
			
			pFileInfo = pFileList->pFileStartAddr + src_cnt + (i++);
			
			strncpy(pFileInfo->name,filename,strlen(filename));
			if(DT_REG == FileType)	//文件使用stat获取文件信息
			{
				ret = stat(pFileInfo->name, &statbuf);			
				if(ret != 0)
				{
					LOGI("file %s stat FAIL",pFileInfo->name);
					perror("stat error:");
					continue;
				}
				LOGI("file:%s,st_size=%ld,st_mtime=%ld,st_ctime=%ld,st_atime=%ld",
					pFileInfo->name,statbuf.st_size,statbuf.st_mtime,statbuf.st_ctime,statbuf.st_atime);
				
				pFileInfo->byte = statbuf.st_size;	
				pFileInfo->time = statbuf.st_ctime;	
			}
			else	//目录从文件名获取时间，不需要查询大�?
			{
				memset(tmp_time,0,sizeof(tmp_time));
				strcpy(tmp_time,ptr->d_name);
				if(check_string_is_num(tmp_time,strlen(tmp_time)))//是纯数字的文件夹名称
				{
					pFileInfo->time = atoi(tmp_time);
				}
				else	//非纯数字的，不是程序创建的，time�?，让程序去回�?
				{
					pFileInfo->time = 0;
				}
				pFileInfo->byte = 0;

				LOGV("directry:%s,time=%d,byte=%d",
					pFileInfo->name,pFileInfo->time,pFileInfo->byte);
			}
		}
	}
	
	closedir(dir);
	return pFileList;
}

/***释放GetDirFileInfo 函数中申请的资源***/
int DirFileListFree(DirFileListInfo_st *pFileList)
{
	if(NULL != pFileList)
	{
		if(NULL != pFileList->pFileStartAddr)
			free(pFileList->pFileStartAddr);
		
		free(pFileList);
	}
	return 0;
}

/**对pFileList中的文件使用快速排序法按照修改时间从小到大排序**/
void DirFileQuickSort(DirectoryFileInfo_st *pFileList,int left,int right) 
{ 
    int i,j; 
    if(left>right) 
       return; 
	
	DirectoryFileInfo_st FileInfoBase;	//基数
	DirectoryFileInfo_st *pFileI;
	DirectoryFileInfo_st *pFileJ;
	DirectoryFileInfo_st FileInfoTemp;
	
	int len = sizeof(DirectoryFileInfo_st);
	
	memcpy((void *)&FileInfoBase,(void *)&pFileList[left],len);
	i=left; 
    j=right;
	
	while(i!=j) 
    { 
		//顺序很重要，要先从右边开始找
		while((&pFileList[j])->time >= FileInfoBase.time && i<j) 
			j--; 
		
			//再找右边�?
			while((&pFileList[i])->time <= FileInfoBase.time && i<j) 
				i++; 
			//交换两个数在数组中的位置 
			if(i<j) 
			{ 
				memcpy((void *)&FileInfoTemp,(void *)(&pFileList[i]),len);
				memcpy((void *)(&pFileList[i]),(void *)(&pFileList[j]),len);
				memcpy((void *)(&pFileList[j]),(void *)&FileInfoTemp,len); 
			} 
    } 
    //最终将基准数归�?
	memcpy((void *)&pFileList[left],(void *)&pFileList[i],len);
	memcpy((void *)&pFileList[i],(void *)&FileInfoBase,len);
                             
    DirFileQuickSort(pFileList,left,i-1);//继续处理左边的，这里是一个递归的过�?
    DirFileQuickSort(pFileList,i+1,right);//继续处理右边�?，这里是一个递归的过�?
}

/**
return:0-����Ҫ���գ�1-��Ҫ����
**/
int need_recycle_disk(int reserved_size)
{
	DISK_CAP_INTO_t disk_cap;
	int ret = 1;
	int auto_del = 1;
	int available_MB = 0;
	auto_del = get_recorder_auto_del(0);

	// LOGI("reserved_size=%d MB,auto_del=%d",reserved_size,auto_del);
	ret = get_disk_path_capacity_info(get_disk_root_path(),&disk_cap);
	if(0 != ret)
	{
		LOGE("get_disk_path_capacity_info FAIL!ret[%d]",ret);
		ret = 0;
	}

	available_MB = disk_cap.available>>20;
	
	// LOGI("available:%d MB,reserved_byte:%d MB,auto_del=%d",
	// 	available_MB, reserved_size, auto_del);
	
	if(available_MB <= reserved_size || available_MB <= 100/*�������*/)	//need recycle
	{
		if(0 == auto_del)
		{
			LOGI("disk is full!not auto del the file!");
			ret = 0;
		}

		ret = 1;
	}
	else
	{
	    static uint8_t print_tm = 0;
	    if(print_tm++ % 20 == 0)
	    {
		    LOGI("recycle disk end,available=%d MB,reserved=%d MB,auto_del=%d",
			    available_MB,reserved_size,auto_del);
		}
		ret = 0;
	}

	return ret;
}

//一次删除一整天的录�?
int recycle_disk_del_day_dir(int reserved_size)
{
	DirFileListInfo_st *pMonDirList = NULL;
	DirFileListInfo_st *pDayDirList = NULL;

	DirectoryFileInfo_st *pMonFileInfo = NULL;
	DirectoryFileInfo_st *pDayFileInfo = NULL;
	DirectoryFileInfo_st *pFile = NULL;
	DirectoryFileInfo_st *pNextMonFileInfo = NULL;
	DirectoryFileInfo_st *pNextDayFileInfo = NULL;

	int ret = -1;
	int i = 0;
	int nMon = 0;
	int nDay = 0;
	long DelSize = 0;
	int j = 0;

	char *pMonPath = NULL;
	char *pDayPath  =NULL;
	char *pAlarmPath = NULL;
	int ret_del = 0;
	DirectoryFileInfo_st *ptmp = NULL;
	char cmd[96] = {0};
	
	LOGI("reserved_size = %d MB",reserved_size);

	//先列定时录像的目�?
	LOGI("recycle COMMOM file,dir:%s", get_common_record_path());
	pMonDirList = GetDirFileInfo(get_common_record_path(), NULL, FILE_TYPE_DIR);
	
	//再列报警录像的目录到同一个链表表中，再一起进行排�?
	LOGI("recycle ALARM file,dir:%s", get_alarm_record_path());
	pMonDirList = GetDirFileInfo(get_alarm_record_path(), pMonDirList, FILE_TYPE_DIR);
	
	if(NULL == pMonDirList)
	{
		LOGI("get year and month directory fail");
		ret = -1;
		goto recycle_end;
	}
	
	/******年月的目录进行排�?*****/
	LOGI("Mou DirFileQuickSort BEFORE:counter=%d",pMonDirList->counter);
	for(i=0; i<pMonDirList->counter; i++)
	{
		pFile = pMonDirList->pFileStartAddr+i;
		LOGI("i=%d,name=%s,time=%ld,byte=%ld",i,pFile->name,pFile->time,pFile->byte);
	}
	
	DirFileQuickSort(pMonDirList->pFileStartAddr,0,pMonDirList->counter-1);
	
	LOGI("Mou DirFileQuickSort AFTER:counter=%d",pMonDirList->counter);
	for(i=0; i<pMonDirList->counter; i++)
	{
		pFile = pMonDirList->pFileStartAddr+i;
		LOGI("i=%d,name=%s,time=%ld,byte=%ld",i,pFile->name,pFile->time,pFile->byte);
	}
get_month_dir:

	if(nMon >=  pMonDirList->counter)	//所有的年月目录已经遍历完，不管有没删除够容量，回收结束
	{
		LOGI("nMon=%d,pMonDirList->counter=%d,recycle end",nMon,pMonDirList->counter);
		ret = 0;
		goto recycle_end;
	}
	
	pMonFileInfo = (pMonDirList->pFileStartAddr + nMon);
	nMon += 1;
	if(nMon <  pMonDirList->counter)	//取下一个目录，看是否有同一个月份的
		pNextMonFileInfo = (pMonDirList->pFileStartAddr + nMon);

	if((NULL != pNextMonFileInfo) && (pMonFileInfo->time == pNextMonFileInfo->time))
	{
		LOGI("check dir[%s] and [%s]",pMonFileInfo->name,pNextMonFileInfo->name);
		pMonPath = pMonFileInfo->name;
		pDayDirList = GetDirFileInfo(pMonPath,NULL,FILE_TYPE_DIR);	//查询年月目录下的目录列表
		if(NULL == pDayDirList)
		{
			LOGI("##remove empty pMonPath:%s##",pMonPath);
			remove(pMonPath);	//空目录删除掉
		}
		
		pMonPath = pNextMonFileInfo->name;
		//将pNextMonFileInfo中的内容也附到pDayDirList上去，再排序
		pDayDirList = GetDirFileInfo(pMonPath,pDayDirList,FILE_TYPE_DIR);
		if(NULL == pDayDirList)	
		{
			LOGI("##remove empty pMonPath:%s##",pMonPath);
			remove(pMonPath);	//空目录删除掉
			goto get_month_dir;
		}
	
		nMon += 1;	//多查询了一个目录，�?
	}
	else
	{
		pMonPath = pMonFileInfo->name;
		LOGI("check dir:%s",pMonFileInfo->name);
		pDayDirList = GetDirFileInfo(pMonPath,NULL,FILE_TYPE_DIR);	//查询年月目录下的目录列表
		if(NULL == pDayDirList)	
		{
			LOGI("##remove empty pMonPath:%s##",pMonPath);
			remove(pMonPath);	//空目录删除掉
			goto get_month_dir;
		}
	}

	LOGI("Day DirFileQuickSort BEFORE:");
	for(i=0; i<pDayDirList->counter; i++)
	{
		pFile = pDayDirList->pFileStartAddr+i;
		LOGI("i=%d,name=%s,time=%ld,byte=%ld",i,pFile->name,pFile->time,pFile->byte);
	}
	
	DirFileQuickSort(pDayDirList->pFileStartAddr,0,pDayDirList->counter-1);	//对day目录按时间旧到新排序

	LOGI("Day DirFileQuickSort AFTER:");
	for(i=0; i<pDayDirList->counter; i++)
	{
		pFile = pDayDirList->pFileStartAddr+i;
		LOGI("i=%d,name=%s,time=%ld,byte=%ld",i,pFile->name,pFile->time,pFile->byte);
	}

get_day_dir:
	if(nDay >=  pDayDirList->counter)
	{
		/**����µ�Ŀ¼�����Ŀ¼ɾ�����ˣ�ɾ������Ŀ¼��Ȼ���ͷŵ�����ȡ��һ����Ŀ¼**/
		if(pMonPath)
		{
			LOGI("##remove empty path:%s##",pMonPath);
			remove(pMonPath);
		}
		
		DirFileListFree(pDayDirList);
		pDayDirList = NULL;
		goto get_month_dir;
	}

	//删除文件�?
	pDayFileInfo = (pDayDirList->pFileStartAddr + nDay);
	nDay += 1;
	if(nDay < pDayDirList->counter)
		pNextDayFileInfo = (pDayDirList->pFileStartAddr + nDay);
		
	//delete directory
	//ALARM and COMMOM 存在同一天的文件，两边都删除�?
	if((NULL != pNextDayFileInfo) && (pDayFileInfo->time == pNextDayFileInfo->time))
	{	
		pDayPath = pDayFileInfo->name;
		LOGI("##remove %s##",pDayPath);
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"rm -rf %s",pDayPath);
		Logfi("rm %s[%s]", __FUNCTION__, cmd);
		system(cmd);

		usleep(500);
		pDayPath = pNextDayFileInfo->name;
		LOGI("##remove %s##",pDayPath);
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"rm -rf %s",pDayPath);
		Logfi("rm %s[%s]", __FUNCTION__, cmd);
		system(cmd);
		nDay += 1;	//多删了一�?计算�?
	}
	else
	{
		pDayPath = pDayFileInfo->name;
		LOGI("##remove %s##",pDayPath);
		memset(cmd,0,sizeof(cmd));
		sprintf(cmd,"rm -rf %s",pDayPath);
		Logfi("rm %s[%s]", __FUNCTION__, cmd);
		system(cmd);
	}
	
	sync();
	/**���ɾ����������������������ȥɾ����һ��Ŀ¼**/
	if(need_recycle_disk(reserved_size))
	{
		LOGI("continue to recyle again");
		goto get_day_dir;
	}
	
recycle_end:
	if(NULL != pMonDirList)
	{
		if((nMon >=  pMonDirList->counter) && need_recycle_disk(reserved_size))
		{
			LOGI("recycle FAIL!all the day path had delete,but it still not enough capility!,reserved_size=%d MB",reserved_size);
			ret = -1;
		}
	}
	else
	{
		LOGI("recycle FAIL!no recorder path in the disk,reserved_size=%d MB",reserved_size);
		ret = -1;
	}
	
	DirFileListFree(pMonDirList);
	DirFileListFree(pDayDirList);
	return ret;
}

int get_disk_path_capacity_info(const char *path,DISK_CAP_INTO_t *pinfo)
{
    struct statfs diskInfo;
	if(pinfo == NULL)
	{
		LOGE("ERROR!pinfo == NULL!");
		return -1;
	}
	
	if(access(path,F_OK)<0)
	{
		LOGI("path[%s] not exist!",path);
		pinfo->size = 0;
    		pinfo->free = 0;
		pinfo->available = 0;
		return -1;
	}
	
    statfs(path,&diskInfo);
    unsigned long long blocksize = diskInfo.f_bsize;
	pinfo->size = diskInfo.f_blocks*blocksize;
    pinfo->free = diskInfo.f_bfree*blocksize;
	pinfo->available = diskInfo.f_bavail*blocksize;
	
	// LOGI("total:%llu[%lluM] free:%llu[%lluM] available:%llu[%lluM]",
	//     pinfo->size, pinfo->size>>20,
	//     pinfo->free, pinfo->free>>20,
	//     pinfo->available, pinfo->available>>20);
	return 0;
}

