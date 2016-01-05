#pragma comment(lib,"ws2_32")
#define _CRT_SECURE_NO_WARNIGS
#include<winsock2.h>
#include<windows.h>
#include<ws2tcpip.h>
#include <stdio.h>
#include<iostream>
#include<cv.h>
#include <math.h>
#include <time.h>
#include <highgui.h>
using namespace std;
#define BUFSIZE 512

char buf[BUFSIZE + 1] = { 'p', 'e', 'r', 's', 'o', 'n' };//��� ���� ���� ���, ���� buf
char suf[BUFSIZE + 1] = { 'x' };//��� ���� �ȵ��� ���, ���� buf
int retval;
int man_count = 0;
SOCKADDR_IN serveraddr;//�ּ�ü�� ���� serveraddr ����
int check = 0;//������ ���� ���� �Լ�����
bool sil = false;//�ǽð� ���� ������ ���� Ȯ���ϴ� �Լ�

typedef struct RTP
{
	unsigned char version : 2, padding : 1, Extension : 1, CSRCCount : 3, Marker : 1;
	unsigned char Type;
	unsigned short seq;
	float Timestamp;
	unsigned int SSRCid;
	char* payload;
}RTP;

struct Blob
{
	CvPoint start;
	CvPoint end;
};
///////////////////////////////////////////////////
//�Ʒ��� labeling �Լ��� �ܺ��ڵ带 �����Ͽ����ϴ�.
//��ó : http://skkuassa.tistory.com/203
//////////////////////////////////////////////////

void labeling(IplImage* src, IplImage* a, CvVideoWriter* b)
{
	struct Blob *blob = NULL;
	int cnt = 0, index = 0;
	int i = 0, j = 0, label = 0;

	int* pass1, *pass2, *table1, *table2;

	pass1 = (int*)malloc(sizeof(int)* src->width * src->height);
	pass2 = (int*)malloc(sizeof(int)* src->width * src->height);
	table1 = (int*)malloc(sizeof(int)* src->width * src->height);
	table2 = (int*)malloc(sizeof(int)* src->width * src->height);
	memset(pass1, 0, (sizeof(int)* src->width * src->height));
	memset(pass2, 0, (sizeof(int)* src->width * src->height));

	for (i = 0; i<src->width * src->height; i++)
	{
		table1[i] = i;
	}
	memset(table2, 0, sizeof(int)*(src->width * src->height));

	for (i = 1; i<src->height; i++)         // 1pass
	{
		for (j = 1; j<src->width; j++)
		{
			if (src->imageData[i*src->widthStep + j] != 0)   
			{
				int up, left;
				up = pass1[(i - 1)*(src->width) + (j)];     
				left = pass1[(i)*(src->width) + (j - 1)];  

				if (up == 0 && left == 0)
				{
					cnt++;  
					pass1[i*src->width + j] = cnt;
				}

				else if (up != 0 && left != 0)  
				{
					if (up>left){     
						pass1[i*src->width + j] = left;  
						table1[up] = table1[left];    
					}
					else{           
						pass1[i*src->width + j] = up;  
						table1[left] = table1[up];  
					}
				}

				else   //UP�̳� LEFT �� �� �ϳ��� 0�϶� 0�� �ƴѰ����� �� ��ȣ ����
					pass1[i*src->width + j] = up + left;
			}
		}
	}


	for (i = 1; i < (src->height); i++)      //2pass and draw rectangle
	{
		for (j = 1; j < (src->width); j++)
		{
			if (pass1[i * src->width + j])      //�� ��ȣ�� �ο��� ���� if�� ����
			{
				int v = table1[pass1[i * src->width + j]]; 

				if (table2[v] == 0)        
				{
					label++;
					table2[v] = label;
				}

				pass2[i * src->width + j] = table2[v];
			}
		}
	}

	blob = (Blob*)malloc(sizeof(Blob)*(label + 1));

	for (int i = 1; i < label + 1; i++)
	{
		blob[i].start.x = src->width;
		blob[i].start.y = src->height;
		blob[i].end.x = 0;
		blob[i].end.y = 0;
		for (int h = 0; h < src->height; h++)
		{
			for (int w = 0; w < src->width; w++)
			{
				if (pass2[h * src->width + w] == i)
				{
					if (blob[i].start.x > w)   //���� ��ǥ ���ϱ�
						blob[i].start.x = w;
					if (blob[i].start.y > h)
						blob[i].start.y = h;
					if (blob[i].end.x < w)      //�� ��ǥ ���ϱ�
						blob[i].end.x = w;
					if (blob[i].end.y < h)
						blob[i].end.y = h;
				}
			}
		}
	}
	if (label == 0)
	{
		printf("no \n");
		check = 0;
		man_count = 0;
	}
	else
	{
		printf("Yes \n");
		check = 1;
		man_count++;
	}

	if (man_count > 10)
	{
		cvWriteFrame(b, a);
	}

	for (i = 1; i<label + 1; i++)
	{
		if (blob[i].end.y - blob[i].start.y < 100 && blob[i].end.x - blob[i].start.x < 100)
			cvRectangle(src, cvPoint(blob[i].start.x, blob[i].start.y),
				cvPoint(blob[i].end.x, blob[i].end.y), CV_RGB(255, 0, 255));
	}
	free(blob);
}

void err_display(char *msg)  //error ���°��� ǥ��
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER
		| FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

void err_quit(char *msg) //error�� ���α׷� ���Ḧ ���� ���
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}
unsigned long WINAPI MyThread1(LPVOID arg) //�޴� ������
{
	SOCKET sock = *(SOCKET*)arg; //ServerSocket
	SOCKADDR_IN  FromClient; //from
	int i, j, k;
	IplImage* image;
	image = cvCreateImage(cvSize(176, 120), IPL_DEPTH_8U, 3);
	int imgsize = image->imageSize + 12;
	int   FromClient_Size;
	int Recv_Size;
	char *sockdata = new char[imgsize];
	RTP A;
	A.payload = (char*)malloc(imgsize);
	FromClient_Size = sizeof(FromClient);

	int fourcc = CV_FOURCC('D', 'I', 'V', 'X'); 
	CvVideoWriter* videowrite = cvCreateVideoWriter("test.avi", fourcc, 30.0, cvSize(image->width, image->height), 1);
	//����ó�� ����
	IplImage* frame = image;

	IplImage* save = cvCreateImage(cvSize(image->width, image->height), IPL_DEPTH_8U, 3);

	IplImage *save_gray = cvCreateImage(cvSize(image->width, image->height), IPL_DEPTH_8U, 1);
	IplImage *frame_gray = cvCreateImage(cvSize(image->width, image->height), IPL_DEPTH_8U, 1);
	IplImage *houghImage = cvCreateImage(cvSize(image->width, image->height), IPL_DEPTH_8U, 3);
	IplImage *subImage = cvCreateImage(cvSize(image->width, image->height), IPL_DEPTH_8U, 1);

	IplImage *half_save = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 3);
	IplImage *half_save_gray = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 1);
	IplImage *re_frame = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 3);
	IplImage *grayImage = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 1);
	IplImage *edgeImage = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 1);
	IplImage *sobelImage = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 1);
	IplImage *half_sub = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 3);
	IplImage *test = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 1);
	IplImage *half_hough = cvCreateImage(cvSize(image->width / 2, image->height / 2), IPL_DEPTH_8U, 3);

	IplImage *aaa = cvCreateImage(cvSize(image->width * 2, image->height * 2), IPL_DEPTH_8U, 1);
	IplImage *bbb = cvCreateImage(cvSize(image->width * 2, image->height * 2), IPL_DEPTH_8U, 1);
	IplImage *ccc = cvCreateImage(cvSize(image->width * 2, image->height * 2), IPL_DEPTH_8U, 1);
	IplImage *ddd = cvCreateImage(cvSize(image->width * 2, image->height * 2), IPL_DEPTH_8U, 3);
	IplImage *eee = cvCreateImage(cvSize(image->width * 2, image->height * 2), IPL_DEPTH_8U, 3);


	IplConvKernel *element = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT, NULL);
	IplConvKernel *element1 = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_ELLIPSE, NULL);

	CvMemStorage* storage = cvCreateMemStorage(0);
	CvSeq* lines;

	int ch = houghImage->nChannels;

	int *arr;
	arr = (int*)malloc(sizeof(int)* image->height * image->width);


	CvPoint point1, point2;
	CvPoint point3, point4;

	float rho1 = 0.0;
	float theta1 = 0.0;
	double c1 = cos(theta1);
	double s1 = sin(theta1);
	double x1 = rho1*c1;
	double y1 = rho1*s1;

	point1.x = cvRound(x1 + 1000 * (-s1));
	point1.y = cvRound(y1 + 1000 * (c1));
	point2.x = cvRound(x1 - 1000 * (-s1));
	point2.y = cvRound(y1 - 1000 * (c1));

	float rho2 = 0.0;
	float theta2 = 0.0;
	double c2 = cos(theta2);
	double s2 = sin(theta2);
	double x2 = rho2*c2;
	double y2 = rho2*s2;

	point3.x = cvRound(x2 + 1000 * (-s2));
	point3.y = cvRound(y2 + 1000 * (c2));
	point4.x = cvRound(x2 - 1000 * (-s2));
	point4.y = cvRound(y2 - 1000 * (c2));

	int count1 = 0;
	int count2 = 0;
	int frcount = 0;
	while (1)
	{
		Recv_Size = 0;
		int size = 0;
		for (i = 0; i < imgsize; i += Recv_Size) //get row data
		{
			if ((Recv_Size = recvfrom(sock, sockdata + i, imgsize - i, 0, (struct sockaddr*)&FromClient, &FromClient_Size)) == -1)
			{
				printf("���� ���� ����\n");
			}
		}
		memcpy(&A, sockdata, 12);
		size += 12;
		memcpy((char*)A.payload, (char*)(sockdata + size), imgsize - 12);

		for (i = 0, k = 0; i < image->height; i++)
		{
			for (j = 0; j < image->width * 3; j++)
			{
				((unsigned char*)(image->imageData + i * image->widthStep))[j] = A.payload[k++];//sockdata[k++];
			}
		}
		frame = image;
		if (!frame)
			break;

		cvResize(frame, re_frame, 1);
		cvResize(grayImage, frame_gray);

		cvCvtColor(re_frame, grayImage, CV_BGR2GRAY);

		cvSmooth(grayImage, grayImage, CV_MEDIAN, 3, 3, 0, 0);
		cvSobel(grayImage, sobelImage, 2, 0, 7);
		cvCanny(grayImage, edgeImage, 50, 200, 3);


		lines = cvHoughLines2(edgeImage, storage, CV_HOUGH_STANDARD, 1, CV_PI / 180, 15, 0, 0);


		for (int i = 0; i < lines->total; i++)
		{
			float* line = (float*)cvGetSeqElem(lines, i); //������ seq���� i��° ��Ҹ� ã�� �����͸� ��ȯ
			float rho = line[0];
			float theta = line[1];
			double c = cos(theta);
			double s = sin(theta);
			double x0 = rho*c;
			double y0 = rho*s;

			CvPoint pt1, pt2;
			pt1.x = cvRound(x0 + 1000 * (-s));
			pt1.y = cvRound(y0 + 1000 * (c));
			pt2.x = cvRound(x0 - 1000 * (-s));
			pt2.y = cvRound(y0 - 1000 * (c));


			if ((theta * 180 / 3.14 > 30 && theta * 180 / 3.14 < 70))
			{
				count1++;

				if (count1 == 1)
				{
					point1 = pt1;
					point2 = pt2;
					theta1 = theta;
					rho1 = rho * 2;
					c1 = c;
					s1 = s;
				}

			}

			else if ((theta * 180 / 3.14 > 140 && theta * 180 / 3.14 < 150))
			{
				count2++;

				if (count2 == 1)
				{
					point3 = pt1;
					point4 = pt2;
					theta2 = theta;
					rho2 = rho * 2;
					c2 = c;
					s2 = s;
				}
			}
			else
				continue; //���� ����� �׸��� �ʰڴ�.

		}

		for (int h = 0; h < image->height; h++)    ////////ROI ����
		{
			for (int w = 0; w < image->width; w++)
			{
				int temp = h*frame->widthStep + ch*w;
				int temp_hough = h*houghImage->widthStep + ch*w;

				if (h < (-(c2 / s2)*w + (rho2 / s2)) || h < (-(c1 / s1)*w + (rho1 / s1)))
				{
					houghImage->imageData[temp_hough] = unsigned char(0); //����ȭ��
					houghImage->imageData[temp_hough + 1] = unsigned char(0);
					houghImage->imageData[temp_hough + 2] = unsigned char(0);
				}

				else
				{
					houghImage->imageData[temp_hough] = frame->imageData[temp]; //��������
					houghImage->imageData[temp_hough + 1] = frame->imageData[temp + 1];
					houghImage->imageData[temp_hough + 2] = frame->imageData[temp + 2];
				}

			}
		}
		cvLine(edgeImage, point1, point2, CV_RGB(255, 0, 255), 1, 8, 0); //���α׸���
		cvLine(edgeImage, point3, point4, CV_RGB(255, 0, 255), 1, 8, 0);

		if (frcount == 0) ///������ ù�������� �������� ������� ��
		{
			cvSaveImage("background.jpg", houghImage);
			frcount++;
			save = cvLoadImage("background.jpg");
			cvCvtColor(save, save_gray, CV_BGR2GRAY);
			cvResize(save_gray, half_save_gray);
			cvResize(save, half_save);
			continue;
		}

		cvResize(houghImage, half_hough);

		for (int h = 0; h < image->height / 2; h++)
		{
			for (int w = 0; w < image->width / 2; w++)
			{
				int sum1 = 0;//�Է¿���
				int b = (int)half_hough->imageData[h*half_hough->widthStep + w*ch];
				int g = (int)half_hough->imageData[h*half_hough->widthStep + w*ch + 1];
				int r = (int)half_hough->imageData[h*half_hough->widthStep + w*ch + 2];
				sum1 = r + g + b;

				int sum2 = 0;//���
				int b2 = (int)half_save->imageData[h*half_save->widthStep + w*ch];
				int g2 = (int)half_save->imageData[h*half_save->widthStep + w*ch + 1];
				int r2 = (int)half_save->imageData[h*half_save->widthStep + w*ch + 2];
				sum2 = r2 + g2 + b2;

				arr[h*test->widthStep + w] = abs(sum1 - sum2);

				if (arr[h*half_sub->width + w] > 100)
					test->imageData[h*test->width + w] = unsigned char(255);

				else
					test->imageData[h*test->width + w] = unsigned char(0);
			}
		}
		//cvSmooth(test, test, CV_MEDIAN, 5, 5, 0, 0);

		cvErode(test, test, element, 1);
		cvDilate(test, test, element, 1);
		cvDilate(test, test, element, 1);
		cvDilate(test, test, element, 1);
		//cvErode(half_sub, half_sub, element, 1);

		//cvShowImage("������ ����", test);
		if (sil)
		{
			cvResize(frame, eee);
			cvShowImage("����", eee);
		}
		else
		{
			cvResize(test, ccc);
			cvShowImage("������ ����", ccc);
			cvResize(frame, eee);
			cvShowImage("����", eee);
			labeling(test, image, videowrite); //�󺧸� ����
			//cvShowImage("Blob Labeling", test);
			cvResize(test, aaa);
			cvShowImage("�󺧸�", aaa);

			//cvShowImage("���� ����", frame);
			//cvShowImage("edgeImage", edgeImage);
			cvResize(edgeImage, bbb);
			cvShowImage("�����̹���", bbb);
			//cvShowImage("ROI ����", houghImage);
			cvResize(houghImage, ddd);
			cvShowImage("ROI ����", ddd);
		}
		if (cvWaitKey(1) == 27)
			break;
	}

	cvReleaseMemStorage(&storage);
	cvReleaseImage(&test);
	cvReleaseImage(&edgeImage);
	cvReleaseImage(&houghImage);
	cvDestroyAllWindows();

	free(arr);

	ZeroMemory(sockdata, sizeof(sockdata));
	cvWaitKey(30);
	return 0;
}

unsigned long WINAPI MyThread2(LPVOID arg) // ������ ������
{
	int len, b;
	SOCKET sock = *(SOCKET*)arg;

	SOCKADDR_IN remoteaddr;
	ZeroMemory(&remoteaddr, sizeof(remoteaddr));
	remoteaddr.sin_family = AF_INET;
	remoteaddr.sin_addr.s_addr = inet_addr("192.168.0.161"); //�޴� .229.
	remoteaddr.sin_port = htons(9000); //�ٸ� 

	while (1)
	{
		if (check)
		{
			len = strlen(buf);
			if (buf[len - 1] == '\n') buf[len - 1] = '\0';
			if (strlen(buf) == 0) continue;
			len = strlen(buf);

			b = sendto(sock, (char*)&len, sizeof(int), 0, (SOCKADDR*)&remoteaddr, sizeof(remoteaddr)); //send�Լ��� ����������
			if (b == SOCKET_ERROR)
			{
				err_display("send()");
				continue;
			}
			//�����ͺ�����
			retval = sendto(sock, buf, len, 0, (SOCKADDR*)&remoteaddr, sizeof(remoteaddr)); //send�Լ��� ����������
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				continue;
			}
		}
		else
		{
			len = strlen(suf);
			if (suf[len - 1] == '\n') suf[len - 1] = '\0';
			if (strlen(suf) == 0) continue;
			len = strlen(suf);

			b = sendto(sock, (char*)&len, sizeof(int), 0, (SOCKADDR*)&remoteaddr, sizeof(remoteaddr)); //send�Լ��� ����������
			if (b == SOCKET_ERROR)
			{
				err_display("send()");
				continue;
			}
			//�����ͺ�����
			retval = sendto(sock, suf, len, 0, (SOCKADDR*)&remoteaddr, sizeof(remoteaddr)); //send�Լ��� ����������
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				continue;
			}
		}
		Sleep(1);
	}
	return 0;
}

int main()
{
	//�����ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	//socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	//bind()
	//BOOL bEnable = TRUE;
	//retval = setsockopt(listen_sock, SOL_SOCKET, SO_BROADCAST, (char*)&bEnable, sizeof(bEnable));
	//SOCKADDR_IN serveraddr; 
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr("192.168.0.88");
	serveraddr.sin_port = htons(9001);

	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr)); //bind�Լ��� �ּ������� ���Ͽ� �Է�
	if (retval == SOCKET_ERROR) err_quit("bind()"); //�����˻�

	printf("bind\n");

	HANDLE hTread[2];
	hTread[0] = CreateThread(NULL, 0, MyThread1, &listen_sock, 0, NULL);
	hTread[1] = CreateThread(NULL, 0, MyThread2, &listen_sock, 0, NULL);
	WaitForMultipleObjects(2, hTread, TRUE, INFINITE);

	closesocket(listen_sock); //���ϴ���
	WSACleanup();
	return 0;
}
