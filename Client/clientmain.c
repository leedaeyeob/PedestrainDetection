#include <wiringPi.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>
#include <opencv/cv.h>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/opencv.hpp"
#include <netinet/in.h>
#include <iostream>
using namespace std;
using namespace cv;

#define BUFSIZE 1024
#define MSIZE	8
#define	SDI		0	// serial data input <pin#11>
#define	RCLK	1	// memory clock input(STCP) <pin#12>
#define	SRCLK	2	// shift register clock input(SHCP) <pin#13>

unsigned char _col[MSIZE]={0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
unsigned char _zero[MSIZE]={0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned char _man1[MSIZE]={0xe7, 0xdb, 0xe7, 0x67, 0x81, 0xe6, 0xdb, 0xdb};
unsigned char _notdet[MSIZE] = {0x7e, 0xbd, 0xdb, 0xe7, 0xe7, 0xdb, 0xbd, 0x7e};

int retval;
socklen_t len;
int sock;

int dot_count=0;
int dot_i;

bool cam=false;

char re_buf[BUFSIZE+1];
int dotCheck = 0;

CvCapture* capture;
IplImage *image0;
IplImage *image1;

pthread_mutex_t hMutex;

int ready=0;

typedef struct RTP
{
	unsigned char version:2, padding:1, Extension:1, CSRCCount:3, Marker:1;
	unsigned char Type;
	unsigned short seq;
	float Timestamp;
	unsigned int SSRCid;
}RTP;

void _74hc595_init()
{
	pinMode(SDI, OUTPUT);
	pinMode(RCLK, OUTPUT);
	pinMode(SRCLK, OUTPUT);

	digitalWrite(SDI, 0);
	digitalWrite(RCLK, 0);
	digitalWrite(SRCLK, OUTPUT);
}

void _74hc595_in(unsigned char _direction)
{
	int i;
	for(i=0; i<8; i++)
	{
		digitalWrite(SDI, 0x80&(_direction << i));
		digitalWrite(SRCLK, 1);
		digitalWrite(SRCLK, 0);
	}
}

void _74hc595_out()
{
	digitalWrite(RCLK, 1);
	digitalWrite(RCLK, 0);
}

 
void *rev(void *arg)
{
 
   int b;
   char buf[BUFSIZE+1];
 
   struct sockaddr_in remoteaddr;
   bzero(&remoteaddr, sizeof(remoteaddr));
   socklen_t sizeaddr = sizeof(remoteaddr);
   struct sockaddr_in peeraddr;
   bzero(&peeraddr, sizeof(peeraddr));
   peeraddr.sin_family = AF_INET;
   peeraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
   peeraddr.sin_port = htons(9000);   //서버에서 send하는 port와 똑같게 설정
   bind(sock, (struct sockaddr*)&peeraddr, sizeof(peeraddr));/////////
	
 while(1)
   {
		b=recvfrom(sock, (char*)&len, sizeof(int), 0,(struct sockaddr *)&remoteaddr,&sizeaddr);

        if(b==0) continue;
		
		   //	enter critical section
          retval=recvfrom(sock,buf,BUFSIZE,0,(struct sockaddr *)&remoteaddr,&sizeaddr);
         		
		  if(retval==0) continue;
		  else{
			if(strcmp(buf,"person")==0)
				dotCheck = 1;
			else
				dotCheck = 0;
		    }

         buf[retval]='\0';
       //  printf("\n [UDP server] %s \n",buf);
		//	reave critical section


   }
   return 0;
}
 
void *send(void *arg)
{
	//clientSocket = sock	
	//sockaddr = ToServer
    //int b;
   	int y,x;
   	int imgsize=image1->imageSize;
  	char sFrame[640*480*3+13];
   	int Send_Size;
	RTP Frame;
	int seq=0;
   struct sockaddr_in sockaddr;
   bzero(&sockaddr, sizeof(sockaddr));
   sockaddr.sin_family = AF_INET;
   sockaddr.sin_addr.s_addr = inet_addr("192.168.0.163"); //서버 ip
   sockaddr.sin_port = htons(9001);
	
	//이미지전송구간
	while(1)
	{
		pthread_mutex_lock(&hMutex);
		if(ready)
		{	
				int size=0;
				Frame.version=1;
				Frame.padding=0;
				Frame.Extension=0;
				Frame.CSRCCount=0;
				Frame.Marker=0;
				Frame.Type=26;
				Frame.seq=seq++;
				Frame.Timestamp=0.4;
				Frame.SSRCid=1;
				memcpy((char*)sFrame,&Frame,12);
				memcpy((char*)(sFrame+12),image1->imageData,imgsize);
				Send_Size=sendto(sock, (char*)sFrame, imgsize+12, 0, (struct sockaddr*) &sockaddr, sizeof(sockaddr));
			//	printf("이미지싸이즈: %d\n",imgsize);
			//	printf("sFrame 크기 : %d\n",sizeof(sFrame));
				if(Send_Size==-1) printf("전송실패\n");
				ready=0;
				usleep(1000);
		}
		pthread_mutex_unlock(&hMutex);
	}
	return 0;
}

void *dot(void *arg)
{
	if(wiringPiSetup()<0)
	{
		printf("error wiringPi\n");
	}

	_74hc595_init();
	

	while(1){
		if(dotCheck){
			for(dot_i = 0; dot_i<MSIZE; dot_i++)
			{
				//printf("person\n"); //도는지 체크 하기위해

				_74hc595_in(_man1[dot_i]);
				_74hc595_in(_col[dot_i]);
				_74hc595_out();
			}	
		}
		else 
		{
			for(dot_i=0; dot_i<MSIZE; dot_i++)
			{
			 	//printf("x\n");
				//x표시하기
				_74hc595_in(_notdet[dot_i]);
			_74hc595_in(_col[dot_i]);
			_74hc595_out(); 
			}
		}
	}
}

int main(int argc, char **argv)
{
 	int key;
   int udpsock=socket(AF_INET, SOCK_DGRAM, 0);
   sock=socket(AF_INET, SOCK_DGRAM, 0); 
   pthread_t thread_t;
   pthread_t thread_a;
   pthread_t thread_b;
 	
   struct sockaddr_in remoteaddr;
   bzero(&remoteaddr, sizeof(remoteaddr));
   remoteaddr.sin_family = AF_INET;
   remoteaddr.sin_addr.s_addr = inet_addr("192.168.0.161");//자기ip
   remoteaddr.sin_port = htons(9000);
 	//영상 전송
 	if(cam==true)
 	{
 		capture=cvCaptureFromCAM(0); //실시간
		cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_WIDTH,176);
		cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_HEIGHT,120);
 	}
 	else 
 		capture = cvCaptureFromFile("/home/pi/Desktop/New/TEST/77.avi");//파일불러오기
 		

	image0=cvQueryFrame(capture);
	image1=cvCreateImage(cvGetSize(image0),IPL_DEPTH_8U,3);
	cvZero(image1);
	cvNamedWindow("Client",0);
	
   pthread_create(&thread_t,NULL,rev,NULL);
   pthread_create(&thread_a,NULL,send,NULL);
   pthread_create(&thread_b,NULL,dot,NULL);
   	
	while(1)
	{	
		if(cam==true){
		pthread_mutex_lock(&hMutex);
		if(ready==0)
		{	
			cvGrabFrame(capture);
			image0=cvRetrieveFrame(capture);
			if(!image0) break;
			
			cvCopy(image0,image1);
			ready=1;
			cvShowImage("Client",image0);
		
			usleep(1000);
		}
		pthread_mutex_unlock(&hMutex);
		key=cvWaitKey(30);
		}
		else{
		pthread_mutex_lock(&hMutex);
		if(ready==0)
		{	
			image0=cvQueryFrame(capture);
			if(!image0) break;
			
			cvCopy(image0,image1);
			ready=1;
			cvShowImage("Client",image0);
		
			usleep(1000);
		}
		pthread_mutex_unlock(&hMutex);
		key=cvWaitKey(30);
		}
	}		

   pthread_join(thread_t,0);
   pthread_join(thread_a,0);
   pthread_join(thread_b,0);

   cvReleaseCapture(&capture);
   cvDestroyWindow("Client");
   close(sock);
   return 0;
}
