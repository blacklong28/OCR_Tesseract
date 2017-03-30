# pragma execution_character_set("utf-8")

#include <QtWidgets/QWidget>
#include <QMutex>
#include "ui_WindowsForFun.h"
#include "qfileinfo.h"
#include "qtextcursor.h"
#include "Qthread.h"
#include "features2d.hpp"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <QWaitCondition>
#include <QSoundEffect>
#include <QFile>  
#include <QMediaPlayer>  
#include <QAudioOutput>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <conio.h>
#include <errno.h>

#include "../include/qtts.h"
#include "../include/msp_cmn.h"
#include "../include/msp_errors.h"
#include <time.h>

#define GRAY_THRESH 150
#define HOUGH_VOTE 100
//#include "baseapi.h"
class HelloThread : public QThread
{
	Q_OBJECT
private:
	void run();
	cv::Mat mser_img(cv::Mat img);
	void HelloThread::ocrMain(cv::Mat gray);
	cv::Mat HelloThread::removeShadow(cv::Mat img);
	cv::Mat HelloThread::detect(cv::Mat img, cv::Mat gray);
	cv::Mat HelloThread::thinImage(cv::Mat src_img, int maxIterations);
	cv::Mat HelloThread::preprocess(cv::Mat gray);
	std::vector< std::vector<cv::Point> > HelloThread::findTextRegion(cv::Mat gray);
	std::vector< std::vector<cv::Point> >  HelloThread::cutPicRegion(cv::Mat gray);
	cv::Mat HelloThread::detect(cv::Mat gray);
	void HelloThread::tts(const char* text);
	char* HelloThread::U2G(const char* utf8);
	int HelloThread::text_to_speech(const char* src_text, char* des_path,char* params);
	char* HelloThread::QXUtf82Unicode(const char* utf, size_t *unicode_number);
	int HelloThread::CN2Unicode(char *input, wchar_t *output);
	char* HelloThread::WcharToChar(wchar_t* wc);
	std::string HelloThread::UTF8_To_GBK(const std::string &source);
	bool HelloThread::thiningDIBSkeleton(unsigned char* lpDIBBits, int lWidth, int lHeight);
	cv::String mypath = "";
	QMutex mutex;
	QWaitCondition mycond;
	int count;
signals:
	void updateui(QString str);
signals:
	void opensig();
	public  slots:
	void recognize(cv::String path);

};
class WindowsForFun : public QWidget
{
    Q_OBJECT

public:
    WindowsForFun(QWidget *parent = Q_NULLPTR);


private slots:
	void on_pushButton_browse_clicked();

	void on_pushButton_start_clicked();

	void on_pushButton_stop_clicked();
public slots:
	void apendtext(QString text);
public slots:
	void OpenWav();
signals:
	void starttess(cv::String path); 

private:
    Ui::WindowsForFunClass ui;
	//像素图指针
	QPixmap *m_pPixMap;
	//log信息
	QString strLog;
	QFile outputLog;
	QString strFileName;
	QFileInfo fi;
	QMediaPlayer mediaPlayer ;
	//清除函数，在打开新图之前，清空旧的
	void ClearOldShow();
	void display_text();
	void save_output_log();
//public HelloThread  mythread;
private:
	HelloThread  *mythread;

};

