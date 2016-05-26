#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include <time.h>  
#include "filesys.h"

#define yudebug
#undef yudebug

#define RevByte(low,high) ((high)<<8|(low))
#define RevWord(lowest,lower,higher,highest) ((highest)<< 24|(higher)<<16|(lower)<<8|lowest) 

/*
*功能：打印启动项记录
*/
void ScanBootSector()
{
	unsigned char buf[SECTOR_SIZE];
	int ret,i;

	if((ret = read(fd,buf,SECTOR_SIZE))<0)
		perror("read boot sector failed");
	for(i = 0; i < 8; i++)
		bdptor.Oem_name[i] = buf[i+0x03];
	bdptor.Oem_name[i] = '\0';

	bdptor.BytesPerSector = RevByte(buf[0x0b],buf[0x0c]);
	bdptor.SectorsPerCluster = buf[0x0d];
	bdptor.ReservedSectors = RevByte(buf[0x0e],buf[0x0f]);
	bdptor.FATs = buf[0x10];
	bdptor.RootDirEntries = RevByte(buf[0x11],buf[0x12]);    
	bdptor.LogicSectors = RevByte(buf[0x13],buf[0x14]);
	bdptor.MediaType = buf[0x15];
	bdptor.SectorsPerFAT = RevByte( buf[0x16],buf[0x17] );
	bdptor.SectorsPerTrack = RevByte(buf[0x18],buf[0x19]);
	bdptor.Heads = RevByte(buf[0x1a],buf[0x1b]);
	bdptor.HiddenSectors = RevByte(buf[0x1c],buf[0x1d]);


	printf("Oem_name \t\t%s\n"
		"BytesPerSector \t\t%d\n"
		"SectorsPerCluster \t%d\n"
		"ReservedSector \t\t%d\n"
		"FATs \t\t\t%d\n"
		"RootDirEntries \t\t%d\n"
		"LogicSectors \t\t%d\n"
		"MedioType \t\t%d\n"
		"SectorPerFAT \t\t%d\n"
		"SectorPerTrack \t\t%d\n"
		"Heads \t\t\t%d\n"
		"HiddenSectors \t\t%d\n",
		bdptor.Oem_name,
		bdptor.BytesPerSector,
		bdptor.SectorsPerCluster,
		bdptor.ReservedSectors,
		bdptor.FATs,
		bdptor.RootDirEntries,
		bdptor.LogicSectors,
		bdptor.MediaType,
		bdptor.SectorsPerFAT,
		bdptor.SectorsPerTrack,
		bdptor.Heads,
		bdptor.HiddenSectors);
}

/*日期*/
void findDate(unsigned short *year,
			  unsigned short *month,
			  unsigned short *day,
			  unsigned char info[2])
{
	int date;
	date = RevByte(info[0],info[1]);
	*year = ((date & MASK_YEAR)>> 9 )+1980;
	*month = ((date & MASK_MONTH)>> 5);
	*day = (date & MASK_DAY);
}

/*时间*/
void findTime(unsigned short *hour,
			  unsigned short *min,
			  unsigned short *sec,
			  unsigned char info[2])
{
	int time;
	time = RevByte(info[0],info[1]);

	*hour = ((time & MASK_HOUR )>>11);
	*min = (time & MASK_MIN)>> 5;
	*sec = (time & MASK_SEC) * 2;
}

/*
*文件名格式化，便于比较
*/
void FileNameFormat(unsigned char *name)
{
	unsigned char *p = name;
	while(*p!='\0')
		p++;
	p--;
	while(*p==' ')
		p--;
	p++;
	*p = '\0';
}

/*参数：entry，类型：struct Entry*
*返回值：成功，则返回偏移值；失败：返回负值
*功能：从根目录或文件簇中得到文件表项
*/
int GetEntry(struct Entry *pentry)
{
	int ret,i;
	int count = 0;
	unsigned char buf[DIR_ENTRY_SIZE], info[2];

	/*读一个目录表项，即32字节*/
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	count += ret;

	if(buf[0]==0xe5 || buf[0]== 0x00)
		return -1*count;
	else
	{
		/*长文件名，忽略掉*/
		while (buf[11]== 0x0f) 
		{
			if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
				perror("read root dir failed");
			count += ret;
		}

		/*命名格式化，主义结尾的'\0'*/
		for (i=0 ;i<=10;i++)
			pentry->short_name[i] = buf[i];
		pentry->short_name[i] = '\0';

		FileNameFormat(pentry->short_name); 



		info[0]=buf[22];
		info[1]=buf[23];
		findTime(&(pentry->hour),&(pentry->min),&(pentry->sec),info);  

		info[0]=buf[24];
		info[1]=buf[25];
		findDate(&(pentry->year),&(pentry->month),&(pentry->day),info);

		pentry->FirstCluster = RevByte(buf[26],buf[27]);
		pentry->size = RevWord(buf[28],buf[29],buf[30],buf[31]);

		pentry->readonly = (buf[11] & ATTR_READONLY) ?1:0;
		pentry->hidden = (buf[11] & ATTR_HIDDEN) ?1:0;
		pentry->system = (buf[11] & ATTR_SYSTEM) ?1:0;
		pentry->vlabel = (buf[11] & ATTR_VLABEL) ?1:0;
		pentry->subdir = (buf[11] & ATTR_SUBDIR) ?1:0;
		pentry->archive = (buf[11] & ATTR_ARCHIVE) ?1:0;

		return count;
	}
}
/*
*功能：显示当前目录的内容
*返回值：1，成功；-1，失败
*/
int fd_ls()
{
	int ret, offset,cluster_addr;
	struct Entry entry;
	unsigned char buf[DIR_ENTRY_SIZE];
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	if(curdir==NULL)
		printf("Root_dir\n");
	else
		printf("%s_dir\n",curdir->short_name);
	printf("\tname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");

	if(curdir==NULL)  /*显示根目录区*/
	{
		/*将fd定位到根目录区的起始地址*/
		if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");

		offset = ROOTDIR_OFFSET;

		/*从根目录区开始遍历，直到数据区起始地址*/
		while(offset < (DATA_OFFSET))
		{
			ret = GetEntry(&entry);

			offset += abs(ret);
			if(ret > 0)
			{
				printf("%12s\t"
					"%d:%d:%d\t"
					"%d:%d:%d   \t"
					"%d\t"
					"%d\t\t"
					"%s\n",
					entry.short_name,
					entry.year,entry.month,entry.day,
					entry.hour,entry.min,entry.sec,
					entry.FirstCluster,
					entry.size,
					(entry.subdir) ? "dir":"file");
			}
		}
	}

	else /*显示子目录*/
	{
		unsigned short cluster = curdir->FirstCluster;	//真正的坑

		while(cluster!=0xffff){					//yu这里有坑！！是0xffffffff，不是0xffff  //ffffffff是因为没有声明成unsigned。。。
#ifdef yudebug
	printf("clusterold=%x\n",cluster);
#endif
			cluster_addr = DATA_OFFSET + (cluster-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
					printf("%12s\t"
						"%d:%d:%d\t"
						"%d:%d:%d   \t"
						"%d\t"
						"%d\t\t"
						"%s\n",
						entry.short_name,
						entry.year,entry.month,entry.day,
						entry.hour,entry.min,entry.sec,
						entry.FirstCluster,
						entry.size,
						(entry.subdir) ? "dir":"file");
				}
			}
			cluster = GetFatCluster(cluster);
#ifdef yudebug
	printf("clusternew=%x\n",cluster);
#endif
		}
	}
	return 0;
} 

/*
*参数：entryname 类型：char
：pentry    类型：struct Entry*
：mode      类型：int，mode=1，为目录表项；mode=0，为文件
*返回值：偏移值大于0，则成功；-1，则失败
*功能：搜索当前目录，查找文件或目录项
*/
int ScanEntry (char *entryname,struct Entry *pentry,int mode)
{
	int ret,offset,i;
	int cluster_addr;
	char uppername[80];
	for(i=0;i< strlen(entryname);i++)
		uppername[i]= toupper(entryname[i]);
	uppername[i]= '\0';


	/*扫描根目录*/
	if(curdir ==NULL)  
	{

		if((ret = lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror ("lseek ROOTDIR_OFFSET failed");
		offset = ROOTDIR_OFFSET;


		while(offset<DATA_OFFSET)
		{
			ret = GetEntry(pentry);
			offset +=abs(ret);

			if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername))

				return offset;

		}
		return -1;
	}

	/*扫描子目录*/
	else  
	{
		unsigned short cluster = curdir->FirstCluster;

		while(cluster!=0xffff){
			cluster_addr = DATA_OFFSET + (cluster -2)*CLUSTER_SIZE;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset= cluster_addr;

			while(offset<cluster_addr + CLUSTER_SIZE)
			{
				ret= GetEntry(pentry);
				offset += abs(ret);
				if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername))
					return offset;

			}
			cluster = GetFatCluster(cluster);
		}
		
		return -1;
	}
}



/*
*参数：dir，类型：char
*返回值：1，成功；-1，失败
*功能：改变目录到父目录或子目录
*/
int fd_cd(char *dir)
{
	struct Entry *pentry;
	struct Entry* fatherdircopy[10];
	struct Entry *curdircopy=NULL;
	char dircopy[100];
	int dirnocopy=0;
	int ret;
	
	//yu
	if(!strcmp(dir,"/")){
		dirno=0;
		curdir = fatherdir[dirno];
		return 1;
	}
	

	if(!strcmp(dir,"."))
	{
		return 1;
	}
	if(!strcmp(dir,"..") && curdir==NULL)
		return 1;
	/*返回上一级目录*/
	if(!strcmp(dir,"..") && curdir!=NULL)
	{
		curdir = fatherdir[dirno];				
		dirno--;
		return 1;
	}
	

	//yu
	int i=0,j=0,count=0;
	for(i=0;i<strlen(dir)-1;i++){			//最后一项为'\0'
		if(dir[i]=='/'&& dir[i+1]!='\0')	//形如：/dev/  第二个'/'不计入
			count++;
	}
	strcpy(dircopy,dir);				//保存dir信息
	dirnocopy = dirno;				//保存dirno
	curdircopy = curdir;				//保存curdir
	for(j=0;j<10;j++)				//保存fatherdir信息
		fatherdircopy[j]=fatherdir[j];

	if(dir[0]=='/'){				//绝对路径
						
		dirno = 0;				
		curdir = NULL;
		if(count>=(10-1-dirno)){		//目录太深，越界保护
			printf("too deep dir\n");
			return -1;
		}
		char tmp[100]={0};
		
		for(i=0;i<count;i++){
			pentry = (struct Entry*)malloc(sizeof(struct Entry));
			int num=0,dircount=0;
			for(int len=1;len<strlen(dircopy);len++)
				tmp[len-1]=dircopy[len];
			for(j=0;j<strlen(tmp);j++){
				if(num==i && tmp[j]!='/')
					dir[dircount++]=tmp[j];
				if(tmp[j]=='/')
					num++;
				if(num==i+1){		
					tmp[j]='\0';
					dir[dircount++]=tmp[j];
					break;
				}
			}
			dir[dircount++]='\0';
			//strcpy(dir,tmp);		
	#ifdef yudebug
printf("cd:count=%d,i=%d,dir=%s\n",count,i,dir);
printf("cd:tmp=%s\n",tmp);
#endif		
			ret = ScanEntry(dir,pentry,1);
#ifdef yudebug
printf("cd:ret=%d,curdir=%s,pentry=%s\n",ret,curdir,pentry);
#endif
			if(ret < 0)
			{
				printf("no such dir\n");
				free(pentry);
				strcpy(dir,dircopy);	//恢复
				dirno = dirnocopy;
				curdir = curdircopy;
				for(j=0;j<10;j++)			
					fatherdir[j]=fatherdircopy[j];
				return -1;
			}		
			dirno ++;
			fatherdir[dirno] = curdir;	
			curdir = pentry;
#ifdef yudebug
printf("cd2:ret=%d,curdir=%s,pentry=%s\n",ret,curdir,pentry);
#endif
		}
		#ifdef yudebug
fflush(stdout);
printf("cd from root success.\n");
#endif
	}else{						//相对路径
		if(count>=(10-1-dirno)){		//目录太深，越界保护
			printf("too deep dir\n");
			return -1;
		}
		char tmp[100];
		
		for(i=0;i<count+1;i++){
			pentry = (struct Entry*)malloc(sizeof(struct Entry));
			int num=0,dircount=0;
			strcpy(tmp,dircopy);
			for(j=0;j<strlen(tmp);j++){
				if(num==i && tmp[j]!='/')
					dir[dircount++]=tmp[j];
				if(tmp[j]=='/')
					num++;
				if(num==i+1){
					tmp[j]='\0';
					dir[dircount++]=tmp[j];
					break;
				}
			}
			dir[dircount++]='\0';
			//strcpy(dir,tmp);		
			
			ret = ScanEntry(dir,pentry,1);
#ifdef yudebug
printf("cd:ret=%d\n",ret);
#endif
			if(ret < 0)
			{
				printf("no such dir\n");
				free(pentry);
				strcpy(dir,dircopy);	//恢复
				dirno = dirnocopy;
				curdir = curdircopy;
				for(j=0;j<10;j++)			
					fatherdir[j]=fatherdircopy[j];
				return -1;
			}
			dirno ++;
			fatherdir[dirno] = curdir;	
			curdir = pentry;
		}
#ifdef yudebug
fflush(stdout);
printf("cd from root by interact success.\n");
#endif
	}
#ifdef yudebug
fflush(stdout);
printf("cd success.\n");
#endif
	return 1;
}

/*
*参数：prev，类型：unsigned char
*返回值：下一簇
*在fat表中获得下一簇的位置
*/
unsigned short GetFatCluster(unsigned short prev)
{
	unsigned short next;
	int index;

	index = prev * 2;
	next = RevByte(fatbuf[index],fatbuf[index+1]);

	return next;
}

/*
*参数：cluster，类型：unsigned short
*返回值：void
*功能：清除fat表中的簇信息
*/
void ClearFatCluster(unsigned short cluster)
{
	int index;
	index = cluster * 2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;

}


/*
*将改变的fat表值写回fat表
*/
int WriteFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{	
#ifdef yudebug
	printf("lseek1");
#endif
		perror("lseek failed");
		return -1;
	}
	if(write(fd,fatbuf,512*250)<0)
	{
#ifdef yudebug
	printf("read1");
#endif
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
#ifdef yudebug
	printf("lseek2");
#endif
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,512*250))<0)
	{
#ifdef yudebug
	printf("read2");
#endif
		perror("read failed");
		return -1;
	}
	return 1;
}

/*
*读fat表的信息，存入fatbuf[]中
*/
int ReadFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(read(fd,fatbuf,512*250)<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}


/*
*参数：filename，类型：char
*返回值：1，成功；-1，失败
*功能;删除当前目录下的文件
*/
int fd_df(char *filename,int mode)
{
	struct Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;

	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	/*扫描当前目录查找文件*/
	ret = ScanEntry(filename,pentry,mode);
	if(ret<0)
	{
		printf("......................no such file\n");
		free(pentry);
		return -1;
	}

	/*清除fat表项*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;

	}

	ClearFatCluster( seed );

	/*清除目录表项*/
	c=0xe5;


	if(lseek(fd,ret-0x20,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");  


	/*if(lseek(fd,ret-0x40,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");*/

	free(pentry);
	if(WriteFat()<0)
		exit(1);
	printf(".........................jm:delete file %s succeed\n",filename);//jm
	return 1;
}


/*
 * 从标准输入中读入文件内容，并保存到char*中 neal
*/
char *getFileContent()
{
	char input[512] = {'\0'};
	unsigned int bufsz = 512;
	char *buffer = (char *)calloc(bufsz, 1);
	if(buffer == NULL)
	{
		perror("getFileContent mem error");
		exit(1);
	}
	*buffer = '\0';
	
	fgets(input, 512, stdin);//jump the initial newline charactor
	printf("Please input file content ended with a blank line.\n");
	while(fgets(input, 512, stdin)!=NULL && strlen(input)>2)
	{
		if(strlen(input) + strlen(buffer) +2 > bufsz)
		{
			bufsz += 512;
			buffer = (char *)realloc(buffer, bufsz);
			if(buffer == NULL)
			{
				perror("getFileContent mem error");
				exit(1);
			}
		}
		strcat(buffer, input);
	}
	
	return buffer;//记住释放内存！！！
}
/*
*参数：filename，类型：char，创建文件的名称
size，    类型：int，文件的大小     int，mode=1，为目录表项；mode=0，为文件
*返回值：1，成功；-1，失败
*功能：在当前目录下创建文件
*/
int fd_cf(char *filename,int size,int mode)
{
	struct Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	char *fcontent = NULL;//file content --neal
	pentry = (struct Entry*)malloc(sizeof(struct Entry));


	//for file    --neal
	if(mode == 0)
	{
		fcontent = getFileContent();
		size = strlen(fcontent);
	}

	clustersize = (size / (CLUSTER_SIZE));

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	if(mode==0)
		ret = ScanEntry(filename,pentry,0);
	else
		ret = ScanEntry(filename,pentry,1);

	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<10000;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;

				i++;
				if(i==clustersize)
					break;

			}

		}

		/*在fat表中写入下一簇信息*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*最后一簇写入0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		/*将文件写入数据区 --neal*/
		if(mode == 0)
		{
			for(int i=0; i<clustersize-1; ++i)
			{
				if((ret= lseek(fd, DATA_OFFSET + (clusterno[i] - 2) * CLUSTER_SIZE, SEEK_SET))<0)
					perror("lseek DATA_OFFSET failed");
				
				if(write(fd, fcontent + i * CLUSTER_SIZE, CLUSTER_SIZE)<0)
					perror("file content write failed");
			}
			//the last cluster
			if((ret= lseek(fd, DATA_OFFSET + (clusterno[i] - 2) * CLUSTER_SIZE, SEEK_SET))<0)
				perror("lseek DATA_OFFSET failed");
			
			if(write(fd, fcontent + i * CLUSTER_SIZE, size % CLUSTER_SIZE)<0)
				perror("file content write failed");
				
			free(fcontent);
		}

		if(curdir==NULL)  /*往根目录下写文件*/
		{ 

			if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			offset = ROOTDIR_OFFSET;
			while(offset < DATA_OFFSET)
			{
				if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset += abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset +=abs(ret);
					}
				}


				/*找出空目录项或已删除的目录项*/ 
				else
				{       
					offset = offset-abs(ret);     
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}			
					for(;i<=10;i++)
						c[i]=' ';
					
					if(mode==0)
						c[11] = 0x01;	 //yu 这里有问题，牵扯系统权限等
					else
						c[11] = 0x11;


					//yu
					time_t timep;  
   					struct tm *p;  
   					time(&timep);  
    					p = gmtime(&timep); //把日期和时间转换为格林威治(GMT)时间的函数  

					int datetmp=((1900+p->tm_year-1980)*512+(1+p->tm_mon)*32+ p->tm_mday);	
					char dhc[2];
					dhc[1]=(datetmp/256);	//高位
					dhc[0]=(datetmp%256);	//低位
					c[24]=dhc[0];
					c[25]=dhc[1];

					int timetmp=(((p->tm_hour+8)%24)*2048+p->tm_min*32+p->tm_sec/2);
					char thc[2];
					thc[1]=(timetmp/256);
					thc[0]=(timetmp%256);
					c[22]=thc[0];
					c[23]=thc[1];
					
					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*写文件的大小*/
					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");




					free(pentry);
					if(WriteFat()<0)
						exit(1);

					return 1;
				}

			}
		}
		else 
		{	
			unsigned short cluster = curdir->FirstCluster;	//真正的坑

			while(cluster!=0xffff){					//yu这里有坑！！是0xffffffff，不是0xffff  //ffffffff是因为没有声明成unsigned。。。
#ifdef yudebug
	printf("clusterold=%x\n",cluster);
#endif
				cluster_addr = (cluster -2 )*CLUSTER_SIZE + DATA_OFFSET;
				if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
					perror("lseek cluster_addr failed");
				offset = cluster_addr;
				while(offset < cluster_addr + CLUSTER_SIZE)
				{
					if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
						perror("read entry failed");

					offset += abs(ret);

					if(buf[0]!=0xe5&&buf[0]!=0x00)
					{
						while(buf[11] == 0x0f)
						{
							if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
								perror("read root dir failed");
							offset +=abs(ret);
						}
					}
					else
					{ 
						offset = offset - abs(ret);      
						for(i=0;i<=strlen(filename);i++)
						{
							c[i]=toupper(filename[i]);
						}
						for(;i<=10;i++)
							c[i]=' ';

						if(mode==0)
							c[11] = 0x01;	 //yu 这里有问题，牵扯系统权限等
						else
							c[11] = 0x11;


						//yu
						time_t timep;  
	   					struct tm *p;  
	   					time(&timep);  
	    					p = gmtime(&timep); //把日期和时间转换为格林威治(GMT)时间的函数  

						int datetmp=((1900+p->tm_year-1980)*512+(1+p->tm_mon)*32+ p->tm_mday);	
						char dhc[2];
						dhc[1]=(datetmp/256);	//高位
						dhc[0]=(datetmp%256);	//低位
						c[24]=dhc[0];
						c[25]=dhc[1];

						int timetmp=((p->tm_hour+8)*2048+p->tm_min*32+p->tm_sec/2);
						char thc[2];
						thc[1]=(timetmp/256);
						thc[0]=(timetmp%256);
						c[22]=thc[0];
						c[23]=thc[1];


						c[26] = (clusterno[0] &  0x00ff);
						c[27] = ((clusterno[0] & 0xff00)>>8);

						c[28] = (size &  0x000000ff);
						c[29] = ((size & 0x0000ff00)>>8);
						c[30] = ((size& 0x00ff0000)>>16);
						c[31] = ((size& 0xff000000)>>24);

						if(lseek(fd,offset,SEEK_SET)<0)
							perror("lseek fd_cf failed");
						if(write(fd,&c,DIR_ENTRY_SIZE)<0)
							perror("write failed");




						free(pentry);
						if(WriteFat()<0)
							exit(1);

						return 1;
					}

				}
				cluster = GetFatCluster(cluster);
	#ifdef yudebug
		printf("clusternew=%x\n",cluster);
	#endif
			}
		}
	}
	else
	{
		printf("This filename is exist\n");
		free(pentry);
		return -1;
	}
	return 1;

}
int fd_rm(char *dirNameold,int mode)//mode==0 直接删除
{	
	int i=0;
	char dirName[100];
	for(i=0;i< strlen(dirNameold);i++)
		dirName[i]= toupper(dirNameold[i]);
	dirName[i]= '\0';	
	
	int ret, offset,cluster_addr;
	struct Entry entry;
	unsigned char buf[DIR_ENTRY_SIZE];
	
	///////////////////////////
	char *dir="..";
	
	struct Entry* fatherdir2[10];
	for(i=0;i<10;i++)
		fatherdir2[i]=fatherdir[i];
	i=0;
	printf("Goto %s\n",dirName);
	if(fd_cd(dirName)<0)
		perror("cd entry failed:870");
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	if(curdir==NULL)
		printf("Root_dir\n");
	else
		printf("%s_dir\n",curdir->short_name);
	if(curdir==NULL)  /*显示根目录区*/
	{
		/*将fd定位到根目录区的起始地址*/
		if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");
		offset = ROOTDIR_OFFSET;
		/*从根目录区开始遍历，直到数据区起始地址*/
		while(offset < (DATA_OFFSET))
		{
			ret = GetEntry(&entry);
			offset += abs(ret);
			if(ret > 0)
			{
				if(entry.subdir==1)
					fd_rm(entry.short_name,mode);
				else
				{
					if(mode==0)
					fd_df(entry.short_name,0);
					else
					{
						printf("删除的目录不为空\n");
						fd_df(entry.short_name,0);
					}
				}
				if((ret = lseek(fd,offset,SEEK_SET))<0)
						;
			}
		}
	}

	else /*显示子目录*/
	{
		unsigned short cluster = curdir->FirstCluster;	//真正的坑
#ifdef yudebug
	printf("clustera=%x\n",cluster);
#endif
		while(cluster!=0xffff){					//yu这里有坑！！是0xffffffff，不是0xffff  //ffffffff是因为没有声明成unsigned。。。
			cluster_addr = DATA_OFFSET + (cluster-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("111lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				//printf("%d",ret);
				offset += abs(ret);
				if(ret > 0)
				{
					if(entry.subdir==1)
						fd_rm(entry.short_name,mode);
					else
					{
						//printf("需删除%s\n",entry.short_name);
						if(mode==0)
							fd_df(entry.short_name,0);
						else
						{
							printf("删除的目录不为空\n");
							fd_df(entry.short_name,0);
						}
					}	
					if((ret = lseek(fd,offset,SEEK_SET))<0)
						;
				}
			}
			cluster = GetFatCluster(cluster);
#ifdef yudebug
	printf("clusterb=%x\n",cluster);
#endif
		}
		
	}
	
	fd_cd(dir);
	if(curdir==NULL)  //删除目录
	{
		/*将fd定位到根目录区的起始地址*/
		if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");
		offset = ROOTDIR_OFFSET;
		/*从根目录区开始遍历，直到数据区起始地址*/
		while(offset < (DATA_OFFSET))
		{
			ret=GetEntry(&entry);
			offset += abs(ret);
			if(ret > 0)
			{	//printf("driname=%s,name=%s",dirName,entry.short_name);
				if(!strcmp(dirName,entry.short_name))
				{
					printf("delete entry %s\n",entry.short_name);
					fd_df(entry.short_name,1);////?
					if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
						perror("lseek cluster_addr failed");
					break;
				}
			}
		}
	}

	else 
	{
		unsigned short cluster = curdir->FirstCluster;
		while(cluster!=0xffff){
			cluster_addr = DATA_OFFSET + (cluster-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;
			/*只读一簇的内容*/
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
					if(!strcmp(dirName,entry.short_name))
					{
						printf("delete entry %s\n",entry.short_name);
						fd_df(entry.short_name,1);////?
						if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
							perror("lseek cluster_addr failed");
						break;
					}
				}
			}
			cluster = cluster = GetFatCluster(cluster);;
		}
	}
	return 0;
}


void do_usage()
{
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\tcd <dir>\t\tchange directory\n\tcf <filename> <size>\tcreate a file\n\tdf <file>\t\tdelete a file\n\tmkdir <name> <size>\tcreat a dir\n\trm <directoryname>\tdelete a directory\n\texit\t\t\texit this system\n");
}


int main()
{
	char input[10];
	char content[100];
	int size=0;
	char name[12];
	if((fd = open(DEVNAME,O_RDWR))<0)
		perror("open failed");
	ScanBootSector();
	if(ReadFat()<0)
		exit(1);
	do_usage();
	while (1)
	{
		printf(">");
		scanf("%s",input);

		if (strcmp(input, "exit") == 0)
			break;
		else if (strcmp(input, "ls") == 0)
			fd_ls();
		else if(strcmp(input, "cd") == 0)
		{
			scanf("%s", name);
			fd_cd(name);
		}
		else if(strcmp(input, "df") == 0)
		{
			scanf("%s", name);
			fd_df(name,0);
		}
		else if(strcmp(input, "cf") == 0)
		{
			scanf("%s", name);
		//	scanf("%s", input);
		//	if(!strcmp(input,"-r")){
		//		scanf("%s", content);
		//		size=strlen(content);
			//}
			//else{
			//	size = atoi(input);
			//}
			//fd_cf(name,size,0);
			fd_cf(name,0,0);
		}
		else if(strcmp(input,"mkdir")==0){	//yu
			scanf("%s", name);
			scanf("%s", input);
			size = atoi(input);
			fd_cf(name,size,1);
			//fd_mkdir(name);
		}
		else if(strcmp(input,"rm")==0)
		{
			printf("remove \n");
			scanf("%s",name);
			if(strcmp(name,"-r")!=0)
			fd_rm(name,1);
			else
			{
				scanf("%s",name);
				fd_rm(name,0);//直接删除
			}
		}
		else
			do_usage();
	}	

	return 0;
}



