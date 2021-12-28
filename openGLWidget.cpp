﻿#include <QtCore/QCoreApplication>
#include <QDebug>
#include<stdlib.h>
//#include <QWidget>
#include <string.h>
#include <math.h>
#include <helper_gl.h>
#include <GL/freeglut.h>
//#include <QDebug>
#include <conio.h>
#include "cuda_ProcHeader.cuh" 
#include <QApplication>
//#include "imagecapturewindow.h"
#include "cuda_ProcHeader.cuh" 
#include "util.h"
//#include <qprocess.h>
//#include "aftercapturewindow.h"
//#include "AppaGalvo.h"
#include"intensityProfile.h"
#include"Qcustomplot.h"
#include <opencv2\core.hpp>
//unsigned short *buffer1;
float dispValCu;
float disp_value;
float dispValCu_DCI;
float disp_value_DCI;
float dispersion_Val = 0;
int signalstrength = 0;
#define DATAFOLDER1  "..\\data\\"
// includes, cuda
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
// Utilities and timing functions
#include <helper_functions.h>    // includes cuda.h and cuda_runtime_api.h
#include <timer.h>               // timing functions

// CUDA helper functions
#include <helper_cuda.h>         // helper functions for CUDA error check
#include <helper_cuda_gl.h>      // helper functions for CUDA/GL interop
#include <vector_types.h>
#include "openglwidget.h"
#include <stdio.h>
//#include <stdlib.h>
#include<iostream>
//#define QCUSTOMPLOT_USE_OPENGL
#include <qmessagebox.h>
#include"octCMOSCamera.h"
#define	BUFFNUM	1
using namespace std;
#define MAX_TIMEOUT_ACQ_IN_MS		3000
#define	LAMBDA_MIN					810.0f               //835.0f original
#define	D_LAMBDA					0.0511571f
#define	SECOND_ORDER_CORRECTION		-2.57900E-06f
#define	THIRD_ORDER_CORRECTION		-6.99779E-09f
#define	FOURTH_ORDER_CORRECTION		-8.5E-16f
#define IMAGE_HEIGHT				640
#define IMAGE_WIDTH					480
#define IMAGESIZE					640*480
float brightness = 0;
float contrast = 0;
int scanset = 2000;
QPointF startPoint;
QCustomPlot *plt;
OpenGLWidget *openglwidget1;
float ss = 0;
int signal;
float A2 = 3.1, A3 = 6.8;
QTimer *timer;
std::vector<cv::Mat> OpenGLWidget::finalStack;
float* volume_Buffer_Host;
float* volume_Buffer_Device;
cudaError_t err;
void enface(float* d_A, float* d_x, float*d_y, const int row, const int col);
void writeMatToFile(cv::Mat m, const char* filename);
QPixmap MatToPixmap(cv::Mat src);
int ctr = 0;
float* d_y_GL;
float* d_x;  //device vector
float* d_y;  //device result
using namespace cv;
OpenGLWidget::OpenGLWidget(QWidget *parent)
	: QOpenGLWidget(parent)
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
#endif	
	finalStack.clear();
	color = 0;
	bufferCtrOpengl = 0;
	showStream = true;
	hCamera = octCMOSCamera::hCamera;
	RAWDATA = (unsigned short*)malloc(2048 * 500 * sizeof(unsigned short));
	cameraDataRaw = (unsigned short*)malloc(numberOfImages * 2048 * 500 * sizeof(unsigned short));
	plt = new QCustomPlot();


	err = cudaMalloc((void**)&d_FrameBufferFloat1, 1200 * 500 * sizeof(float));
	cudaMemset(d_FrameBufferFloat1, 0, 1200 * 500 * sizeof(float));
	if (err != cudaSuccess)
	{
		fprintf(stderr, "Failed to allocate device memory - DFrameBufferFloat(error code %s)!\n", cudaGetErrorString(err));
	}


	//cudaMemset(d_FrameBufferFloat, 0, bscanWidth * bscanHeight * sizeof(float));

	// Buffer Memory Allocation_ Host Memory

	volume_Buffer_Host = (float*)malloc(1200*128 * sizeof(float));

	//Buffer Memory Allocation_ Device Memory

	err = cudaMallocHost((void**)&volume_Buffer_Device, 128 * 1200 * 500 * sizeof(float));
	if (err != cudaSuccess)
	{
		fprintf(stderr, "Failed to allocate device memory- Device Code Failed (error code %s)!\n", cudaGetErrorString(err));
	}


	cudaError_t cudastat;
	cublasStatus_t stat;
	int size = 500 * 1200 * 128;
	//cublasHandle_t handle;



	cudastat = cudaMalloc((void**)&d_x, 500 * sizeof(float));
	cudastat = cudaMalloc((void**)&d_y, 1200 * 128 * sizeof(float));
}

void OpenGLWidget::flushBuffer()
{
	PRINTFUNCTIONNAME
		printf("USB3_FlushBuffers  in opengl\n");
	nError = USB3_FlushBuffers(hCamera);
	if (nError != CAM_ERR_SUCCESS){
	}
}
//
OpenGLWidget::~OpenGLWidget()
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
	PRINTFUNCTIONNAME
#endif	
		//finalStack.clear();
		glDisable(GL_TEXTURE_2D);
	if (bscanPBO) {
		cudaGraphicsUnregisterResource(bscanCudaRes);
		glDeleteBuffersARB(1, &bscanPBO);
		glDeleteTextures(1, &bscanTEX);
	}
	cudaFree(RAWDATA);
	cudaHostUnregister(buffer1);
	cudaFree(buffer1);

	cudaHostUnregister(d_FrameBuffer);
	cudaFree(d_FrameBuffer);

	cudaHostUnregister(d_FrameBufferushort);
	cudaFree(d_FrameBufferushort);
	cudaHostUnregister(d_FrameBufferFloat1);
	cudaFree(d_FrameBufferFloat1);
	cudaHostUnregister(d_FrameBuffer1);
	cudaFree(d_FrameBuffer1);
	cudaFree(rawImageUshort);
	cudaFree(imageUshort);
	cudaFreeHost(volume_Buffer_Device);
	free(volume_Buffer_Host);
	this->close();

}

void OpenGLWidget::setProcess(bool proc)
{
	processing = proc;
}

void OpenGLWidget::InitCameraRead()//buffer *h_buffer)// , int *buffLen)
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
#endif

	bufferLen = frameWidthGl * 500 * framesPerBufferCu;//scanSetting.lineNumber; change
	buffer *h_buffer = new buffer[BUFFNUM];
	for (int i = 0; i<BUFFNUM; i++) {
		h_buffer[i].data = (unsigned short *)malloc(2048 * 500 * sizeof(unsigned short));
		h_buffer[i].regHost = false;
	}
	h_buff = h_buffer;

}

bool OpenGLWidget::startOCTLiveStream()
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
#endif
	//read_DispVal();
	disp_value = A2;
	disp_value_DCI = A3;
	qDebug() << disp_value;
	qDebug() << "disp_value_DCI= " << disp_value_DCI;

	dispValCu = (disp_value)* pow(0.1f, 5);
	dispValCu_DCI = (disp_value_DCI)* pow(0.1f, 5);
	runProcess();
	runCamera();
	return true;
}


void OpenGLWidget::runCamera()
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
#endif	

	makeCurrent();
	bufferCtrOpengl = (bufferCtrOpengl + 1) % BUFFNUM;
	//Get the buffer data from camera handle.Getting image parameters and aw data in ImageInfos.
	nError = USB3_GetBuffer(hCamera, &ImageInfos, MAX_TIMEOUT_ACQ_IN_MS);
	if (nError != CAM_ERR_SUCCESS && nError != CAM_WARNING_FRAME_COMMPLETED_WITH_ZEROS || nError == CAM_ERR_INVALID_HANDLE)
	{
		showStream = false;
		qDebug() << "cameraerror";
		//IntensityProfile* intensityprofile =new IntensityProfile();
		//intensityprofile->stopOCTCameraAcquisitionFromOpenglWidget(nError);
	}
	if (nError == CAM_ERR_TIMEOUT)
	{
	}

	//Extract raw data.
	RAWDATA = (unsigned short*)ImageInfos.pDatas;
	emit senddata1(RAWDATA);


	if (showStream == true)
	{

		memcpy(h_buff[bufferCtrOpengl].data, RAWDATA, 2048 * 500 * sizeof(unsigned short));//bufferLen
		setBufferPtr(h_buff[0].data);
		USB3_RequeueBuffer(hCamera, ImageInfos.hBuffer);
	}
}
void OpenGLWidget::on_CustomPlot1()
{

}
void OpenGLWidget::runProcess()
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
#endif
	if (initProcess == false)
	{
		initGLVarAndPtrs(windowWidth, windowHeight);
		/* Initialize CUDA Variables and Pointers */
		if (deepChoroidalImaging == true)
		{
			dispersion_Val = dispValCu_DCI;
		}
		else
		{
			dispersion_Val = dispValCu;
		}

		initCudaProcVar(frameWidthGl,
			frameHeightGl,
			framesPerBufferCu,
			LAMBDA_MIN,
			lambda_Max,
			dispMagCu,
			dispValCu,
			0,
			samplingMethodCu,
			fftLenMult);
		qDebug() << "frameWidthGl: " << windowWidth;
		qDebug() << "frameHeightGl: " << windowHeight;
		qDebug() << "framesPerBufferCu: " << framesPerBufferCu;
		qDebug() << "LAMBDA_MIN: " << LAMBDA_MIN;
		qDebug() << "lambda_Max: " << lambda_Max;
		qDebug() << "dispMagCu: " << dispMagCu;
		qDebug() << "dispValCu: " << dispValCu;
		qDebug() << "dispValThirdCu: " << dispValThirdCu;
		qDebug() << "samplingMethodCu: " << samplingMethodCu;
		qDebug() << "fftLenMult: " << fftLenMult;
		initProcess = true;
	}
}

void OpenGLWidget::initGLVarAndPtrs(int winWid, int winHei)
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
	PRINTFUNCTIONNAME
#endif
		cudaGLSetGLDevice(0);

	widthGl = frameWidthGl;
	heightGl = frameHeightGl;
	frames = framesTotal;
	if (frames%framesPerBufferCu != 0)
		frames -= framesPerBufferCu;

	windowWidth = winWid;
	windowHeight = winHei;
	//bscanWidth = widthGl;
	bscanWidth = 1200;
	bscanHeight = heightGl;
}

void OpenGLWidget::setBufferPtr(unsigned short *h_buffer)
{
	//PRINTFUNCTIONNAME
	buffer1 = h_buffer;

}

void OpenGLWidget::registerCudaHost()
{
	cudaHostRegister(buffer1, widthGl * heightGl * frames * sizeof(unsigned short), cudaHostRegisterDefault);
	qDebug() << "registerCudaHost" << widthGl << "," << heightGl << "," << buffer1;
}
//
void OpenGLWidget::initializeGL()
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
	PRINTFUNCTIONNAME
#endif	

		initializeOpenGLFunctions();
	makeCurrent();
	glutInitDisplayMode(GLUT_DOUBLE);
	glewInit();

	// Using ARB Method also works
	glGenBuffersARB(1, &bscanPBO);
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, bscanPBO);

	//Color B-Scan
	if (color == 1)
	{
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, bscanWidth * bscanHeight* sizeof(float), 0, GL_DYNAMIC_DRAW_ARB);
		qDebug() << "initializeGL bscanWidth " << bscanWidth;
	}
	//Gray Scale Bscan
	if (color == 0)
	{
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, 500* 1200 * sizeof(unsigned short), 0, GL_DYNAMIC_DRAW_ARB);
	}

	glGenTextures(1, &bscanTEX);				//Generate the Open GL texture
	glBindTexture(GL_TEXTURE_2D, bscanTEX); //Tell OpenGL which texture to edit
	if (color == 0)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 1200, 500, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
	}
	if (color == 1)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bscanWidth, bscanHeight, 0, GL_RGB, GL_FLOAT, NULL);
		qDebug() << "initializeGL bscanWidth " << bscanHeight;
	}
	//This means that when zooming into the image, the zoomed display will be much smoother
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);
	registerBuffer();
	minVal = 2.9;
	maxVal = 1.6;

	qDebug() << "InitializeGL ended";
}

void OpenGLWidget::registerBuffer()
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
	PRINTFUNCTIONNAME
#endif
		cudaGraphicsGLRegisterBuffer(&bscanCudaRes, bscanPBO, cudaGraphicsMapFlagsNone);
}


void OpenGLWidget::cudaEvent()
{
#ifdef QT_DEBUG
	PRINTFUNCTIONNAME
#else
	//PRINTFUNCTIONNAME
#endif
	if (showStream == true)
	{
		size_t size;
		cudaGraphicsMapResources(1, &bscanCudaRes, 0);
		//printf("showStream\n");

		if (color == 0)
		{
			cudaGraphicsResourceGetMappedPointer((void**)&d_FrameBufferushort, &size, bscanCudaRes);
		}

		cudaPipeline(buffer1, NULL, NULL, 0, 1, cropOffset, bscanWidth, false);// , brightness, contrast);// , brightness, contrast/*, referenceImage*/); //deep choroidal
		copySingleFrameushort(NULL, d_FrameBufferushort, bscanWidth, bscanHeight, 0);
		copySingleFrame(NULL, d_FrameBufferFloat1, bscanWidth, bscanHeight, 0);

		//	printf("B-Scan Height: %d", bscanHeight);
		//printf("B-Scan Width: %d", bscanWidth);
		//Stack Image - Module

		if (ctr < 128)
		{
			//printf("Counter %d\n", ctr);
			//printf("Device to Device Memory Transfer");
			err = cudaMemcpy(&volume_Buffer_Device[ctr * 1200 * 500], d_FrameBufferFloat1, 1200 * 500 * sizeof(float), cudaMemcpyDeviceToDevice);
			if (err != cudaSuccess)
			{
				fprintf(stderr, "Failed to copy vector from device to devicexxxxxxxxx - Code Check(error code %s\n", cudaGetErrorString(err));
			}
		}
		else
		{
			//Enface Projection
			//showStream == false;
			float* x = new float[500];
			for (int i = 0; i < 500; i++)
			{
				x[i] = 1;
			}





			cudaMemcpy(d_x, x, sizeof(float) * 500, cudaMemcpyHostToDevice);   //copy x to device d_x




			for (int i = 0; i < 128; i++)
			{

				enface(&volume_Buffer_Device[i * 1200 * 500], d_x, &d_y[i * 1200], 1200, 500);
				//printf("Hi");	
			}


			//cudaGraphicsResourceGetMappedPointer((void**)&d_y_GL, &size, bscanCudaRes);
			//copyFunction(d_y, d_FrameBufferGL);




			err = cudaMemcpy(volume_Buffer_Host, d_y,1200 * 128 * sizeof(float), cudaMemcpyDeviceToHost);
			if (err != cudaSuccess)
			{
				fprintf(stderr, "Failed to copy vector from device to host(error code %s\n", cudaGetErrorString(err));
			}

			Mat Image(128, 1200, CV_32FC1);
			memcpy(Image.data, volume_Buffer_Host, 128 * 1200 * sizeof(float));
			//const char* fileName = "TEXT.txt";
			//writeMatToFile(Image, fileName);
			ctr = 0;
			double min2, max2;
			minMaxLoc(Image, &min2, &max2);

			double divVal = 255 / max2;
			Mat ConvImage = Image * divVal;
			ConvImage.convertTo(ConvImage, CV_8UC1);
			Mat data_Image(128, 1200, CV_8UC1);
			data_Image = ConvImage.clone();
			cv::resize(data_Image, data_Image, cv::Size(200, 200));
			QPixmap Result;
			Result = MatToPixmap(data_Image);

			emit sendImage(Result);

			//printf("HostMemory Done");



			//imshow("Image", data_Image);
			//waitKey(0);
		}






		cudaThreadSynchronize();
	}

	ctr++;

	cudaGraphicsUnmapResources(1, &bscanCudaRes, 0);


	//showStream = false;
	//cudaFree(d_FrameBufferushort);
	//cudaFree(d_FrameBufferFloat);
}

QPixmap MatToPixmap(cv::Mat src)
{
	QImage::Format format = QImage::Format_Grayscale8;
	int bpp = src.channels();
	if (bpp == 3)format = QImage::Format_RGB888;
	QImage img(src.cols, src.rows, format);
	uchar *sptr, *dptr;
	int linesize = src.cols*bpp;
	for (int y = 0; y<src.rows; y++){
		sptr = src.ptr(y);
		dptr = img.scanLine(y);
		memcpy(dptr, sptr, linesize);
	}
	if (bpp == 3)return QPixmap::fromImage(img.rgbSwapped());
	return QPixmap::fromImage(img);
}


void OpenGLWidget::paintGL()

{
	//PRINTFUNCTIONNAME
	makeCurrent();
	if (showStream == true)
	{
		if (processing == true){
			runProcess();
			runCamera();
			cudaEvent();
			processCompleted = true;

		}

		glLoadIdentity();
		glRotatef(-180.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		glBindTexture(GL_TEXTURE_2D, 0);
		glLoadIdentity();
		glRotatef(-180.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, bscanPBO);
		glBindTexture(GL_TEXTURE_2D, bscanTEX);

		//GrayColor B-Scan
		if (color == 0) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1200, 500, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
		}
		//Color B-Scan
		if (color == 1) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bscanWidth, bscanHeight, GL_RGB, GL_FLOAT, NULL);
			//qDebug() << "paintGL bscanWidth" << bscanWidth;
		}
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex2f(1.0f, -1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex2f(-1.0f, 1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
		glEnd();
		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
		update();

	}


}

void OpenGLWidget::resizeGL(int w, int h)
{
	PRINTFUNCTIONNAME
		makeCurrent();
	windowWidth = w;
	windowHeight = h;
	glViewport(0, 0, windowWidth, windowHeight);
}
void OpenGLWidget::loadreference()
{
	acquireDC();
}
void enface(float* d_A, float* d_x, float*d_y, const int row, const int col)
{
	cublasStatus_t stat;
	cublasHandle_t handle;
	float alf = 0.0005;
	float beta = 0;
	stat = cublasCreate(&handle);
	stat = cublasSgemv(handle, CUBLAS_OP_T, col, row, &alf, d_A, col, d_x, 1, &beta, d_y, 1);//swap col and row
	cublasDestroy(handle);
}
void writeMatToFile(cv::Mat m, const char* filename)
{
	ofstream fout(filename);

	if (!fout)
	{
		cout << "File Not Opened" << endl;  return;
	}

	for (int i = 0; i < m.rows; i++)
	{
		for (int j = 0; j < m.cols; j++)
		{
			fout << m.at<float>(i, j) << "\t";
		}
		fout << endl;
	}

	fout.close();
}
void OpenGLWidget::stack()
{
	control = 1;
}