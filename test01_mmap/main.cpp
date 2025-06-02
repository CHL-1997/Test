#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<errno.h>
#include<time.h>

//#define BUFF_LEN (64*1024)
//#define BUFF_LEN (128*1024)
//#define BUFF_LEN (256*1024)
//#define BUFF_LEN (512*1024)
#define BUFF_LEN (1024*1024) //实测这个大小速度最快
//#define BUFF_LEN (3*1024*1024) //实测这个大小速度最快

#define MMAP_BUFF_LEN (2*1024*1024)

#define BILLION 1000000000

//计算耗时函数
static void timeStart(struct timespec *startTime){
    if(NULL == startTime){
		printf("input param NULL\n");
		return;
	}
    clock_gettime(CLOCK_MONOTONIC, startTime);
    return;
}
static void timeEnd(struct timespec *startTime, struct timespec *endTime){
    if(NULL == startTime || NULL == endTime){
		printf("input param NULL\n");
		return;
	}
	time_t interval_s=0;
	long interval_ns=0;
    clock_gettime(CLOCK_MONOTONIC, endTime);
	interval_s = endTime->tv_sec - startTime->tv_sec;
	if(interval_s > 0){
		interval_s -= 1;
		interval_ns = endTime->tv_nsec - startTime->tv_nsec + BILLION;
	}else{
		interval_ns = endTime->tv_nsec - startTime->tv_nsec;
	}
	printf("fread cost [%ld]s, [%ld.%ld]ms\n", interval_s, interval_ns/1000000, interval_ns%1000000);
    return;
}

static void timeEnd(){
    return;
}

static int read_function(const char *file_path, char *buff, int buffLen){
	if(NULL==file_path || NULL==buff || buffLen<=0){
		printf("input param err.\n");
		return 0;
	}
	
	int fd = -1;
	int num = 0;
	int readLen = 0;
	
	fd = open(file_path, O_RDONLY);
	if(fd < 0){
		printf("open file:[%s] failed, err:[%s]\n", file_path, strerror(errno));
		return readLen;
	}
	while((num = read(fd, buff, buffLen)) != 0){
		if(num == -1 && errno == EINTR){
			continue;
		}
		if(num == -1 && errno != EINTR){
			printf("read err:[%s]\n", strerror(errno));
			break;
		}
		readLen += num;
		memset(buff, 0, buffLen);
	}
	if(-1 != fd){
		close(fd);
	}
	printf("read end, readLen:[%d]\n", readLen);
	return readLen;
}

static int fread_function(const char *file_path, char *buff, int buffLen){
	if(NULL==file_path || NULL==buff || buffLen<=0){
		printf("input param err.\n");
		return 0;
	}
	
	FILE* fp = NULL;
	int num = 0;
	int readLen = 0;
	
	fp = fopen(file_path, "rb");
	if(fp == NULL){
		printf("fopen file:[%s] failed, err:[%s]\n", file_path, strerror(errno));
		return readLen;
	}
	while((num=fread(buff, sizeof(char), buffLen, fp)) != 0){
		if((num != buffLen) && feof(fp)){
			readLen += num;
			break;
		}
		if((num != buffLen) && ferror(fp)){
			printf("fread err:[%s]\n", strerror(errno));
			return readLen;
		}
		readLen += num;
		memset(buff, 0, buffLen);
	}
	if(NULL != fp){
	    fclose(fp);
	}
	printf("fread end, readLen:[%d]\n", readLen);
	return readLen;
}

static int mmap_function(const char *file_path, char *buff, int buffLen){
	if(NULL==file_path || NULL==buff || buffLen<=0){
		printf("input param err.\n");
		return 0;
	}
	
	int fd = -1;
	int num = 0;
	int readLen = 0;
	int chunkNum = 0;
	void *mmaped = NULL;
	struct stat statBuf;

	do{
        fd = open(file_path, O_RDONLY);
	    if(fd == -1){
		    printf("open file:[%s] failed, err:[%s]\n", file_path, strerror(errno));
		    break;
	    }
	    if(-1 == fstat(fd, &statBuf)){
		    printf("fstat failed, err:[%s]\n", strerror(errno));
		    break;
	    }
		chunkNum = (statBuf.st_size%MMAP_BUFF_LEN==0)?(statBuf.st_size/MMAP_BUFF_LEN):(statBuf.st_size/MMAP_BUFF_LEN+1);
		for(int i=0; i < chunkNum; ++i){
			off_t offset = i * MMAP_BUFF_LEN;
			size_t chunk_size = (i==chunkNum-1)?(statBuf.st_size%MMAP_BUFF_LEN):(MMAP_BUFF_LEN);
		
			mmaped = mmap(NULL, chunk_size, PROT_READ, MAP_PRIVATE, fd, offset);
            if(mmaped == MAP_FAILED){
				printf("mmap failed, err:[%s]\n", strerror(errno));
                break;
            }
			size_t temp = chunk_size;
			char *src = (char*)mmaped;
			while(temp > 0){
				num = (temp>buffLen)?buffLen:temp;
				temp -= num;
			    readLen += num;
				memcpy(buff, src, num);
				src += num;
				memset(buff, 0, buffLen);
			}
			if(munmap(mmaped, chunk_size) == -1){
				printf("munmmap failed, err:[%s]\n", strerror(errno));
                break;
            }
		}
	}while(0);
	if(-1 != fd){
		close(fd);
	}
	printf("mmap end, readLen:[%d]\n", readLen);
	return readLen;
}

int main(int argc, char** argv){
	if(argc<2){
			printf("usage app file_path\n");
			return -1;
	}

    char buff[BUFF_LEN];
    char file_path[64];
	time_t interval_s=0;
	long interval_ns=0;
	struct timespec startTime;
	struct timespec endTime;
	struct stat statBuf;
	
	memset(buff, 0, sizeof(buff));
    memset(file_path, 0, sizeof(file_path));
	
	memcpy(file_path, argv[1], strlen(argv[1]));
	stat(file_path, &statBuf);
	printf("file path:[%s], file size:[%ld]Bytes\n", argv[1], statBuf.st_size);

	//使用read函数拷贝文件的耗时时间
	printf("==============read==============\n");
	timeStart(&startTime);
	read_function(file_path, buff, BUFF_LEN);
	timeEnd(&startTime, &endTime);
	printf("================================\n");

	//使用fread函数拷贝文件的耗时时间
	#if 1
	memset(buff, 0, sizeof(buff));
	printf("==============fread=============\n");
	timeStart(&startTime);
	fread_function(file_path, buff, BUFF_LEN);
	timeEnd(&startTime, &endTime);
	printf("================================\n");
	#endif

	//使用mmap函数拷贝文件的耗时时间
	#if 1
	memset(buff, 0, sizeof(buff));
	printf("==============mmap==============\n");
	timeStart(&startTime);
	mmap_function(file_path, buff, BUFF_LEN);
	timeEnd(&startTime, &endTime);
	printf("================================\n");
	#endif
	
    return 0;
}

