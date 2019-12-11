/*
*名称：v4l2demo
*作者：orangeyang
*时间：20190904
*说明：（1）本demo得到的是yuv422格式的图像
*	   （2）对于图像，我们用宽和高来描述
*      （3）对于得到的yuv帧，使用7yuv等软件进行打开
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include<unistd.h>

#define BUFFER_COUNT 10 //缓存帧数目

typedef struct VideoBuffer { //用来映射缓存帧
	void *start;
	size_t length;
} VideoBuffer;

int VIDEO_WIDTH = 720; //默认宽、高
int VIDEO_HEIGHT = 480;
int n; //定义得到的yuv帧的数量
int continunity; //yuv帧的连续与否

int Open_Camera() //打开摄像头
{
	int ret = open("/dev/video0",O_RDWR/* required */| O_NONBLOCK); //打开方式：read和write，用一个int型来标记这个摄像头
	if (ret < 0)
		printf("opened error\n");
	else
		printf("connected success!  welcome!\n\n");

	return ret;
}

void Close_Camera(int camera) //关闭摄像头
{
	close(camera);
	printf("Thinks for using,goodbye!\n");
}

void Show_Camera_Capability(int camera) //查询摄像头属性
{
	struct v4l2_capability cap;//套路：先定义一个特定的结构体，然后调用v4l2接口函数来将其填充，之后打印出来
	ioctl(camera, VIDIOC_QUERYCAP, &cap);
	printf("Capability Informations:\n");
	printf("driver: %s\n", cap.driver);// 驱动名字
	printf("card: %s\n", cap.card);// 设备名字
	printf("bus_info: %s\n", cap.bus_info);//设备在系统中的位置
	printf("version: %08X\n", cap.version);// 驱动版本号
	printf("capabilities: %08X\n\n", cap.capabilities);//设备支持的操作
}

void Show_Current_Format(int camera) //查询当前的帧的格式
{
	struct v4l2_format fmt;
	char fmtstr[8];

	memset(&fmt,0,sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(camera, VIDIOC_G_FMT, &fmt);
	printf("Stream Format Informations:\n");
	printf("type: %d\n", fmt.type);
	printf("width: %d\n", fmt.fmt.pix.width);
	printf("height: %d\n", fmt.fmt.pix.height);
	
	memset(fmtstr, 0, 8);
	memcpy(fmtstr, &fmt.fmt.pix.pixelformat, 4);
	printf("pixelformat: %s\n", fmtstr);
	printf("field: %d\n", fmt.fmt.pix.field);
	printf("bytesperline: %d\n", fmt.fmt.pix.bytesperline);
	printf("sizeimage: %d\n", fmt.fmt.pix.sizeimage);
	printf("colorspace: %d\n", fmt.fmt.pix.colorspace);
	printf("priv: %d\n", fmt.fmt.pix.priv);
	printf("raw_date: %s\n\n", fmt.fmt.raw_data);
}

void framePerSecond2(int camera, struct v4l2_frmsizeenum* frmsize) //被下面的supportResolution()函数调用
{
	struct v4l2_frmivalenum frmval;
	frmval.index = 0;
	frmval.pixel_format = frmsize->pixel_format;
	frmval.width = frmsize->discrete.width;
	frmval.height = frmsize->discrete.height;
	printf("\t Frame rate:  ");
	//while(!ioctl(camera, VIDIOC_ENUM_FRAMEINTERVALS, &frmval)){
	ioctl(camera, VIDIOC_ENUM_FRAMEINTERVALS, &frmval);
		frmval.index++;
		unsigned int frmrate;
		frmrate = (frmval.discrete.denominator / frmval.discrete.numerator);
		printf("%d  ", frmrate);
	//}
	printf("\n");
}

void supportResolution(int camera, struct v4l2_fmtdesc* format) //被下面的Show_Camera_Supportformat()函数调用
{
	struct v4l2_frmsizeenum frmsize;
	frmsize.index = 0;
	frmsize.pixel_format = format->pixelformat;
	printf("\tSupport fram size:\n");
	//while(!ioctl(camera, VIDIOC_ENUM_FRAMESIZES, &frmsize)){
	ioctl(camera, VIDIOC_ENUM_FRAMESIZES, &frmsize);
		frmsize.index++;
		printf("\t\t%4d width: %4d  height: %d",
		frmsize.index, frmsize.discrete.width, frmsize.discrete.height);
		framePerSecond2(camera, &frmsize);
	//}
}

void Show_Camera_Supportformat(int camera ) //显示该摄像头可以支持的帧的格式
{
	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index=0;
	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("Supportformat:\n");
	//while(ioctl(camera,VIDIOC_ENUM_FMT,&fmtdesc)!=-1)
	//{
		ioctl(camera,VIDIOC_ENUM_FMT,&fmtdesc);
		printf("%d.%s\n",fmtdesc.index+1,fmtdesc.description);
		fmtdesc.index++;
		supportResolution(camera,&fmtdesc);
	//}
	printf("\n");
}

void set_size(int a,int b)
{
	VIDEO_WIDTH=a;
	VIDEO_HEIGHT=b;
	printf("your size is : %d * %d\n",VIDEO_WIDTH, VIDEO_HEIGHT);
	printf("\n");
}

void Set_Frame_Size() //输入你想要的图像的大小
{
	printf("Support size:(width * height):\n");
	printf(" 1: 640 * 480\t 2: 640 * 360\t 3: 544 * 288\n");
	printf(" 4: 432 * 240\t 5: 352 * 288\t 6: 320 * 240\n");
	printf(" 7: 320 * 176\t 8: 176 * 144\t 9: 160 * 120\n");
	printf("10: 752 * 416\t11: 800 * 448\t12: 864 * 480\n");
	printf("13: 800 * 600\t14: 960 * 544\t15:1024 * 576\n");
	printf("16: 960 * 720\t17:1184 * 656\t18:1280 * 720\n");
	printf("19: 720 * 576\t20:720 * 480\n");
	printf("\n");
	int size;
	printf("please choose a size :\n");
	scanf("%d",&size);

	switch(size)
	{
		case 1:set_size(640,480); break;
		case 2:set_size(640,360); break;
		case 3:set_size(544,288); break;
		case 4:set_size(432,240); break;
		case 5:set_size(352,288); break;
		case 6:set_size(320,240); break;
		case 7:set_size(320,176); break;
		case 8:set_size(176,144); break;
		case 9:set_size(160,120); break;
		case 10:set_size(752,416); break;
		case 11:set_size(800,448); break;
		case 12:set_size(864,480); break;
		case 13:set_size(800,600); break;
		case 14:set_size(960,544); break;
		case 15:set_size(1024,576);break;
		case 16:set_size(960,720); break;
		case 17:set_size(1184,656);break;
		case 18:set_size(1280,720);break;
		case 19:set_size(720,576);break;
		case 20:set_size(720,480);break;
	}
}

void Set_Frame_Numbers() //输入你想要的图像的数量
{
	printf("please inter the number of frame :\n");
	scanf("%d",&n);
	printf("\n");
}

void Set_Cuntinity() //把数据写到一个文件或者多个文件
{
	printf("please enter your number for cuntinity:\t0:continusly\t1:dispersed\n");
	scanf("%d",&continunity);
	printf("\n");
}

void Set_Format(int camera,int width,int height) //设置帧的宽和高
{
	struct v4l2_format fmt;
	memset(&fmt,0,sizeof(fmt));
	fmt.type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width		= width;
	fmt.fmt.pix.height		=height;
	fmt.fmt.pix.pixelformat	= V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field		= V4L2_FIELD_ALTERNATE;
	int ret = ioctl(camera, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		printf("VIDIOC_S_FMT failed (%d)\n", ret);
	}
}

void Set_Frames_Pre_Second(int camera) //设置帧的帧率，但是不是你想设多少就是多少，会自动找一个最相近的帧率
{
	int n;
	printf("please enter the number of frames pre second\n");
	scanf("%d",&n);
	printf("\n");
	struct v4l2_streamparm Stream_Parm;
	memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
	Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	Stream_Parm.parm.capture.timeperframe.denominator =n;
	Stream_Parm.parm.capture.timeperframe.numerator = 1;
	ioctl(camera, VIDIOC_S_PARM, &Stream_Parm);
}

void Show_Frames_Per_Second(int camera) //显示帧率
{
	struct v4l2_streamparm Stream_Parm;
	memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
	Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(camera, VIDIOC_G_PARM, &Stream_Parm);
	int fm = Stream_Parm.parm.capture.timeperframe.denominator;
	int fz = Stream_Parm.parm.capture.timeperframe.numerator;
	printf("%dframes per second in current mode\n",fm/fz);
	printf("\n");
}

void Start(int fd) // 申请缓存帧
{
	unsigned int i;
	struct v4l2_requestbuffers reqbuf;
	reqbuf.count = BUFFER_COUNT;// 缓冲区内缓冲帧的数目
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;// 缓冲帧数据格式
	reqbuf.memory = V4L2_MEMORY_MMAP;//区别是内存映射还是用户指针方式
	int ret = ioctl(fd , VIDIOC_REQBUFS, &reqbuf);//执行申请命令
	
	#ifdef debug
	if(ret < 0) {
		printf("VIDIOC_REQBUFS failed (%d)\n", ret);
	}
	#endif
	
	//获取本地空间
	VideoBuffer* framebuf = calloc( reqbuf.count, sizeof(VideoBuffer) );
	struct v4l2_buffer buf;//用来存储数据（缓冲区地址，长度）的结构体
	//获取缓冲区的地址，映射，排入队列
	
	for (i = 0; i < reqbuf.count; i++)
	{
		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(fd , VIDIOC_QUERYBUF, &buf);//获取缓冲区地址，长度
		if(ret < 0) {
			printf("VIDIOC_QUERYBUF (%d) failed (%d)\n", i, ret);
		}
		// mmap buffer
		framebuf[i].length = buf.length;
		framebuf[i].start = (char *) mmap(0, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
		if (framebuf[i].start == MAP_FAILED) {
			printf("mmap (%d) failed: %s\n", i, strerror(errno));
		}// Queen buffer
		ret = ioctl(fd , VIDIOC_QBUF, &buf);
		if (ret < 0) {
			printf("VIDIOC_QBUF (%d) failed (%d)\n", i, ret);
		}
	}
	// 开始录制
	// 启动数据流
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_STREAMON, &type);//启动数据流
	if (ret < 0) {printf("VIDIOC_STREAMON failed (%d)\n", ret);}
	FILE *fp;
	int j;
	char str[80]={"yms666.yuv"};
	for(j=1;j<=n;j++)
	{
		if(continunity==1){
		sprintf(str,"yms%d.yuv",j);
		}
	// Get a frame
	ret = ioctl(fd, VIDIOC_DQBUF, &buf);
	if (ret < 0) {
		printf("VIDIOC_DQBUF failed (%d)\n", ret);
	}
	// Process the frame
	if(continunity==1)
		fp = fopen(str,"wb");
	else
		fp = fopen(str,"ab");
	if (fp < 0) {
		printf("open frame data file failed\n");
	}
	fwrite(framebuf[buf.index].start, 1, buf.length, fp);
	printf("%d------------------------------------------------\n",buf.index);
	fflush(fp);
	fclose(fp);
	system("sync");
	printf("Capture %d frame saved in %s\n",j, str);
	// Re-queen buffer
	ret = ioctl(fd, VIDIOC_QBUF, &buf);
		if (ret < 0) {
		printf("VIDIOC_QBUF failed (%d)\n", ret);
	}
	}
	close(fd);
	//释放本地空间资源
	for (i=0; i< 10; i++)
	{
		munmap(framebuf[i].start, framebuf[i].length);
	}
	printf("\n");
}

int main()
{
	int fd;
	
	fd = Open_Camera();
	Show_Camera_Capability(fd);
	Show_Current_Format(fd);
	Show_Camera_Supportformat(fd);
	Set_Frame_Size();
	Set_Frame_Numbers();
	Set_Cuntinity();
	//Set_Format(fd,640,480);
	Set_Frames_Pre_Second(fd);
	Show_Frames_Per_Second(fd);
	Start(fd);
	Close_Camera(fd);
	
	return 0;
}
