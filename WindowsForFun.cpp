#include "WindowsForFun.h"
#include <QFileDialog>
#include <QFile>
#include <QDebug>
#include <QMessageBox>
#include <QScrollArea>
#include <QDateTime>


#include <allheaders.h> // leptonica main header for image io
#include <baseapi.h> // tesseract main header
//#include "Python.h"
/***************************************
**UI部分
**程序开始于此
**
***************************************/


bool Debug_show_process = TRUE;//FALSE TRUE

#ifdef _WIN64
#pragma comment(lib,"msc_x64.lib")//x64
#else
#pragma comment(lib,"msc.lib")//x86
#endif

/* wav音频头部格式 */
typedef struct _wave_pcm_hdr
{
	char            riff[4];                // = "RIFF"
	int				size_8;                 // = FileSize - 8
	char            wave[4];                // = "WAVE"
	char            fmt[4];                 // = "fmt "
	int				fmt_size;				// = 下一个结构体的大小 : 16

	short int       format_tag;             // = PCM : 1
	short int       channels;               // = 通道数 : 1
	int				samples_per_sec;        // = 采样率 : 8000 | 6000 | 11025 | 16000
	int				avg_bytes_per_sec;      // = 每秒字节数 : samples_per_sec * bits_per_sample / 8
	short int       block_align;            // = 每采样点字节数 : wBitsPerSample / 8
	short int       bits_per_sample;        // = 量化比特数: 8 | 16

	char            data[4];                // = "data";
	int				data_size;              // = 纯数据长度 : FileSize - 44 
} wave_pcm_hdr;

/* 默认wav音频头部数据 */
wave_pcm_hdr default_wav_hdr =
{
	{ 'R', 'I', 'F', 'F' },
	0,
	{ 'W', 'A', 'V', 'E' },
	{ 'f', 'm', 't', ' ' },
	16,
	1,
	1,
	16000,
	32000,
	2,
	16,
	{ 'd', 'a', 't', 'a' },
	0
};
/* 文本合成 */
int HelloThread::text_to_speech(const char* src_text,  char* des_path,  char* params)
{
	int          ret = -1;
	FILE*        fp = NULL;
	const char*  sessionID = NULL;
	unsigned int audio_len = 0;
	wave_pcm_hdr wav_hdr = default_wav_hdr;
	int          synth_status = MSP_TTS_FLAG_STILL_HAVE_DATA;
	qDebug() << src_text;
	if (NULL == src_text || NULL == des_path)
	{
		printf("params is error!\n");
		return ret;
	}
	fp = fopen(des_path, "wb");
	if (NULL == fp)
	{
		printf("open %s error.\n", des_path);
		return ret;
	}
	/* 开始合成 */
	sessionID = QTTSSessionBegin(params, &ret);
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSSessionBegin failed, error code: %d.\n", ret);
		fclose(fp);
		return ret;
	}
	ret = QTTSTextPut(sessionID, src_text, (unsigned int)strlen(src_text), NULL);
	src_text = NULL;
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSTextPut failed, error code: %d.\n", ret);
		QTTSSessionEnd(sessionID, "TextPutError");
		fclose(fp);
		return ret;
	}
	printf("正在合成 ...\n");
	fwrite(&wav_hdr, sizeof(wav_hdr), 1, fp); //添加wav音频头，使用采样率为16000
	while (1)
	{
		/* 获取合成音频 */
		const void* data = QTTSAudioGet(sessionID, &audio_len, &synth_status, &ret);
		if (MSP_SUCCESS != ret)
			break;
		if (NULL != data)
		{
			fwrite(data, audio_len, 1, fp);
			wav_hdr.data_size += audio_len; //计算data_size大小
		}
		if (MSP_TTS_FLAG_DATA_END == synth_status)
			break;
	}//合成状态synth_status取值请参阅《讯飞语音云API文档》
	//printf("\n");
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSAudioGet failed, error code: %d.\n", ret);
		QTTSSessionEnd(sessionID, "AudioGetError");
		fclose(fp);
		return ret;
	}
	/* 修正wav文件头数据的大小 */
	wav_hdr.size_8 += wav_hdr.data_size + (sizeof(wav_hdr) - 8);

	/* 将修正过的数据写回文件头部,音频文件为wav格式 */
	fseek(fp, 4, 0);
	fwrite(&wav_hdr.size_8, sizeof(wav_hdr.size_8), 1, fp); //写入size_8的值
	fseek(fp, 40, 0); //将文件指针偏移到存储data_size值的位置
	fwrite(&wav_hdr.data_size, sizeof(wav_hdr.data_size), 1, fp); //写入data_size的值
	fclose(fp);
	fp = NULL;
	/* 合成完毕 */
	ret = QTTSSessionEnd(sessionID, "Normal");
	qDebug() << "QTTSSessionEnd";
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSSessionEnd failed, error code: %d.\n", ret);
	}

	return ret;
}

WindowsForFun::WindowsForFun(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	//初始化成员变量
	setFixedSize(this->width(), this->height());
	m_pPixMap = NULL;
	//设置log区为只读
	ui.plainTextEdit->setReadOnly(true);
	mythread = new HelloThread;
	
	connect(mythread, SIGNAL(updateui(QString)), this, SLOT(apendtext(QString)));
	connect(this, SIGNAL(starttess(cv::String)), mythread, SLOT(recognize(cv::String)));
	connect(mythread, SIGNAL(opensig()), this, SLOT(OpenWav()));
	//connect(ui.pushButton_start, SIGNAL(clicked()), this, SLOT(on_pushButton_start_clicked()));
	//connect(ui.pushButton_stop, SIGNAL(clicked()), this, SLOT(on_pushButton_stop_clicked()));


}
/***************************************
**点击Browse按钮信号的槽函数
**
**
***************************************/
void WindowsForFun::on_pushButton_browse_clicked()
{
	   //文件名
	strFileName = QFileDialog::getOpenFileName(this, tr("Open the image to be processed"), "",
		"Pictures (*.bmp *.jpg *.jpeg *.png *.xpm);;All files(*)");
	if (strFileName.isEmpty())
	{
		//文件名为空，返回
		return;
	}
	//清空显示图片区域的图片
	ClearOldShow();
	//打印文件名
	qDebug() << strFileName;

	//新建像素图
	m_pPixMap = new QPixmap();
	QPixmap m_pPixMap_scale;
	//加载
	if (m_pPixMap->load(strFileName))
	{
		int w;
		int h;
		w = m_pPixMap->width();
		h = m_pPixMap->height();
		if (w >= h){
			if (w>450){
				m_pPixMap_scale = m_pPixMap->scaledToWidth(450);
			}
			else{
				m_pPixMap_scale = *m_pPixMap;

			}
		}
		else
		{
			if (h>350){
				m_pPixMap_scale = m_pPixMap->scaledToHeight(350);
			}
			else{
				m_pPixMap_scale = *m_pPixMap;

			}

		}
		//加载成功
		//设置给标签
		ui.labelshow->setPixmap(m_pPixMap_scale);

		//设置标签的新大小，与像素图一样大
		//ui->labelshow->setGeometry( m_pPixMap->rect() );
		//strLog += tr("Load image success\r\n");
		QString  file_name, file_path;
		
		//file_full = QFileDialog::getOpenFileName(this);

		fi = QFileInfo(strFileName);
		file_name = fi.fileName();
		file_path = fi.absolutePath();
		qDebug() << tr("The current image name is：%1\r\n").arg(file_name);
		qDebug() << tr("The image path is：%1\r\n").arg(file_path);

	}
	else
	{
		qDebug() << tr("Failed to load image\r\n");
		//加载失败，删除图片对象，返回
		delete m_pPixMap;   m_pPixMap = NULL;
		//提示失败
		QMessageBox::critical(this, tr("Open failed"),
			tr("Failed to open the image, the file name is：\r\n%1").arg(strFileName));
	}
	//display_time();

}
/***************************************
**功能：清除图片区域原图片
**
**
***************************************/
void WindowsForFun::ClearOldShow()
{
	//清空标签内容
	ui.labelshow->clear();
	//像素图不空就删除
	if (m_pPixMap != NULL)
	{
		//删除像素图
		delete m_pPixMap;   m_pPixMap = NULL;
	}

}
void WindowsForFun::OpenWav()
{

	qDebug() << tr("播放音频");
	mediaPlayer.setMedia(QUrl::fromLocalFile("tts_sample.wav"));
	mediaPlayer.play();
	//mediaPlayer.stateChanged(stopTimer);
	//sound = new QSound("tts_sample.wav", this); //构建对象
	//sound->play();//播放
	qDebug() << tr("play");
	
	//sound->stop();//停止
	//sound->setLoops(1);
}
/***************************************
**功能：细化字符
**
**
***************************************/
cv::Mat HelloThread::thinImage( cv::Mat src_img,  int maxIterations = -1)
{
	assert(src_img.type() == CV_8UC1);
	cv::Mat dst;
	int width = src_img.cols;
	int height = src_img.rows;
	src_img.copyTo(dst);
	int count = 0;  //记录迭代次数
	while (true)
	{
		count++;
		if (maxIterations != -1 && count > maxIterations) //限制次数并且迭代次数到达
			break;
		std::vector<uchar *> mFlag; //用于标记需要删除的点
									//对点标记
		for (int i = 0; i < height; ++i)
		{
			uchar * p = dst.ptr<uchar>(i);
			for (int j = 0; j < width; ++j)
			{
				//如果满足四个条件，进行标记
				//  p9 p2 p3
				//  p8 p1 p4
				//  p7 p6 p5
				uchar p1 = p[j];
				if (p1 != 1) continue;
				uchar p4 = (j == width - 1) ? 0 : *(p + j + 1);
				uchar p8 = (j == 0) ? 0 : *(p + j - 1);
				uchar p2 = (i == 0) ? 0 : *(p - dst.step + j);
				uchar p3 = (i == 0 || j == width - 1) ? 0 : *(p - dst.step + j + 1);
				uchar p9 = (i == 0 || j == 0) ? 0 : *(p - dst.step + j - 1);
				uchar p6 = (i == height - 1) ? 0 : *(p + dst.step + j);
				uchar p5 = (i == height - 1 || j == width - 1) ? 0 : *(p + dst.step + j + 1);
				uchar p7 = (i == height - 1 || j == 0) ? 0 : *(p + dst.step + j - 1);
				if ((p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9) >= 2 && (p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9) <= 6)
				{
					int ap = 0;
					if (p2 == 0 && p3 == 1) ++ap;
					if (p3 == 0 && p4 == 1) ++ap;
					if (p4 == 0 && p5 == 1) ++ap;
					if (p5 == 0 && p6 == 1) ++ap;
					if (p6 == 0 && p7 == 1) ++ap;
					if (p7 == 0 && p8 == 1) ++ap;
					if (p8 == 0 && p9 == 1) ++ap;
					if (p9 == 0 && p2 == 1) ++ap;

					if (ap == 1 && p2 * p4 * p6 == 0 && p4 * p6 * p8 == 0)
					{
						//标记
						mFlag.push_back(p + j);
					}
				}
			}
		}

		//将标记的点删除
		for (std::vector<uchar *>::iterator i = mFlag.begin(); i != mFlag.end(); ++i)
		{
			**i = 0;
		}

		//直到没有点满足，算法结束
		if (mFlag.empty())
		{
			break;
		}
		else
		{
			mFlag.clear();//将mFlag清空
		}

		//对点标记
		for (int i = 0; i < height; ++i)
		{
			uchar * p = dst.ptr<uchar>(i);
			for (int j = 0; j < width; ++j)
			{
				//如果满足四个条件，进行标记
				//  p9 p2 p3
				//  p8 p1 p4
				//  p7 p6 p5
				uchar p1 = p[j];
				if (p1 != 1) continue;
				uchar p4 = (j == width - 1) ? 0 : *(p + j + 1);
				uchar p8 = (j == 0) ? 0 : *(p + j - 1);
				uchar p2 = (i == 0) ? 0 : *(p - dst.step + j);
				uchar p3 = (i == 0 || j == width - 1) ? 0 : *(p - dst.step + j + 1);
				uchar p9 = (i == 0 || j == 0) ? 0 : *(p - dst.step + j - 1);
				uchar p6 = (i == height - 1) ? 0 : *(p + dst.step + j);
				uchar p5 = (i == height - 1 || j == width - 1) ? 0 : *(p + dst.step + j + 1);
				uchar p7 = (i == height - 1 || j == 0) ? 0 : *(p + dst.step + j - 1);

				if ((p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9) >= 2 && (p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9) <= 6)
				{
					int ap = 0;
					if (p2 == 0 && p3 == 1) ++ap;
					if (p3 == 0 && p4 == 1) ++ap;
					if (p4 == 0 && p5 == 1) ++ap;
					if (p5 == 0 && p6 == 1) ++ap;
					if (p6 == 0 && p7 == 1) ++ap;
					if (p7 == 0 && p8 == 1) ++ap;
					if (p8 == 0 && p9 == 1) ++ap;
					if (p9 == 0 && p2 == 1) ++ap;

					if (ap == 1 && p2 * p4 * p8 == 0 && p2 * p6 * p8 == 0)
					{
						//标记
						mFlag.push_back(p + j);
					}
				}
			}
		}

		//将标记的点删除
		for (std::vector<uchar *>::iterator i = mFlag.begin(); i != mFlag.end(); ++i)
		{
			**i = 0;
		}

		//直到没有点满足，算法结束
		if (mFlag.empty())
		{
			break;
		}
		else
		{
			mFlag.clear();//将mFlag清空
		}
	}
	return dst;
}

/***************************************
**功能：基于索引表的细化算法
** 参数：lpDIBBits：代表图象的一维数组
**       lWidth：图象高度
**       lHeight：图象宽度
**       无返回值
**
***************************************/
bool HelloThread::thiningDIBSkeleton(unsigned char* lpDIBBits, int lWidth, int lHeight)
{
	//循环变量
	long i;
	long j;
	long lLength;

	unsigned char deletemark[256] = {      // 这个即为前人据8领域总结的是否可以被删除的256种情况
		0,0,0,0,0,0,0,1,	0,0,1,1,0,0,1,1,
		0,0,0,0,0,0,0,0,	0,0,1,1,1,0,1,1,
		0,0,0,0,0,0,0,0,	1,0,0,0,1,0,1,1,
		0,0,0,0,0,0,0,0,	1,0,1,1,1,0,1,1,
		0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,	0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,	1,0,0,0,1,0,1,1,
		1,0,0,0,0,0,0,0,	1,0,1,1,1,0,1,1,
		0,0,1,1,0,0,1,1,	0,0,0,1,0,0,1,1,
		0,0,0,0,0,0,0,0,	0,0,0,1,0,0,1,1,
		1,1,0,1,0,0,0,1,	0,0,0,0,0,0,0,0,
		1,1,0,1,0,0,0,1,	1,1,0,0,1,0,0,0,
		0,1,1,1,0,0,1,1,	0,0,0,1,0,0,1,1,
		0,0,0,0,0,0,0,0,	0,0,0,0,0,1,1,1,
		1,1,1,1,0,0,1,1,	1,1,0,0,1,1,0,0,
		1,1,1,1,0,0,1,1,	1,1,0,0,1,1,0,0
	};//索引表

	unsigned char p0, p1, p2, p3, p4, p5, p6, p7;
	unsigned char *pmid, *pmidtemp;    // pmid 用来指向二值图像  pmidtemp用来指向存放是否为边缘
	unsigned char sum;
	bool bStart = true;
	lLength = lWidth * lHeight;
	unsigned char *pTemp = new uchar[sizeof(unsigned char) * lWidth * lHeight]();  //动态创建数组 并且初始化

																				   //    P0 P1 P2
																				   //    P7    P3
																				   //    P6 P5 P4

	while (bStart)
	{
		bStart = false;

		//首先求边缘点
		pmid = (unsigned char *)lpDIBBits + lWidth + 1;
		memset(pTemp, 0, lLength);
		pmidtemp = (unsigned char *)pTemp + lWidth + 1; //  如果是边缘点 则将其设为1
		for (i = 1; i < lHeight - 1; i++)
		{
			for (j = 1; j < lWidth - 1; j++)
			{
				if (*pmid == 0)                   //是0 不是我们需要考虑的点
				{
					pmid++;
					pmidtemp++;
					continue;
				}
				p3 = *(pmid + 1);
				p2 = *(pmid + 1 - lWidth);
				p1 = *(pmid - lWidth);
				p0 = *(pmid - lWidth - 1);
				p7 = *(pmid - 1);
				p6 = *(pmid + lWidth - 1);
				p5 = *(pmid + lWidth);
				p4 = *(pmid + lWidth + 1);
				sum = p0 & p1 & p2 & p3 & p4 & p5 & p6 & p7;
				if (sum == 0)
				{
					*pmidtemp = 1;       // 这样周围8个都是1的时候  pmidtemp==1 表明是边缘     					
				}

				pmid++;
				pmidtemp++;
			}
			pmid++;
			pmid++;
			pmidtemp++;
			pmidtemp++;
		}

		//现在开始删除
		pmid = (unsigned char *)lpDIBBits + lWidth + 1;
		pmidtemp = (unsigned char *)pTemp + lWidth + 1;

		for (i = 1; i < lHeight - 1; i++)   // 不考虑图像第一行 第一列 最后一行 最后一列
		{
			for (j = 1; j < lWidth - 1; j++)
			{
				if (*pmidtemp == 0)     //1表明是边缘 0--周围8个都是1 即为中间点暂不予考虑
				{
					pmid++;
					pmidtemp++;
					continue;
				}

				p3 = *(pmid + 1);
				p2 = *(pmid + 1 - lWidth);
				p1 = *(pmid - lWidth);
				p0 = *(pmid - lWidth - 1);
				p7 = *(pmid - 1);
				p6 = *(pmid + lWidth - 1);
				p5 = *(pmid + lWidth);
				p4 = *(pmid + lWidth + 1);

				p1 *= 2;
				p2 *= 4;
				p3 *= 8;
				p4 *= 16;
				p5 *= 32;
				p6 *= 64;
				p7 *= 128;

				sum = p0 | p1 | p2 | p3 | p4 | p5 | p6 | p7;
				//	sum = p0 + p1 + p2 + p3 + p4 + p5 + p6 + p7;
				if (deletemark[sum] == 1)
				{
					*pmid = 0;
					bStart = true;      //  表明本次扫描进行了细化
				}
				pmid++;
				pmidtemp++;
			}

			pmid++;
			pmid++;
			pmidtemp++;
			pmidtemp++;
		}
	}
	delete[]pTemp;
	return true;
}
/***************************************
**HelloThread::detect(cv::Mat gray)
**功能：
**
***************************************/

cv::Mat HelloThread::detect(cv::Mat img, cv::Mat gray)
{
	cv::Mat dilation, original, result;
	cv::Mat preGray=gray.clone();

	// 1.  转化成灰度图
	if (img.channels() == 3) {
		cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
	}
	original = img.clone();

	std::vector< std::vector<cv::Point> > region,graypic;
	// 2. 形态学变换的预处理，得到可以查找矩形的图片
	dilation = preprocess(original);
	if (Debug_show_process) {
		cv::namedWindow("dilation", CV_WINDOW_NORMAL);
		cv::imshow("dilation", dilation);
		cv::waitKey(0);
	}
	//通过dilation区域与原图作与运算,去除不做检测的区域。
	bitwise_and(gray, dilation, result);
	//cv::namedWindow("result", cv::WINDOW_NORMAL);
	//cv::imshow("result", result);
	//cv::waitKey(0);

	//3. 查找和筛选文字区域
	//std::cout << "findTextRegion";
	cv::Mat original_img = img.clone();
	cv::Mat preGray0 = gray.clone();
	if (Debug_show_process) {
		cv::namedWindow("original_img", cv::WINDOW_NORMAL);
		cv::imshow("original_img", preGray0);
		cv::waitKey(0);
	}
	cv::Mat  element, dilation2;
	element = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(8, 8));//(30,5)
	cv::dilate(preGray0, dilation2, element);
	graypic = cutPicRegion(dilation2);
	region = findTextRegion(dilation);
	//在一张全黑的图片上将文字区域用白色块填充，得到文字区域。
	cv::Mat blackimg(result.rows, result.cols, CV_8UC1, cv::Scalar(0, 0, 0));//255
	//for (int k= 0;k < region.size();k++) {

		cv::drawContours(blackimg, region,-1, (255, 255, 255), CV_FILLED);
	//}
	

	//fillPoly(gray, region, -1, 1, Scalar(255));
	if (Debug_show_process) {
			cv::namedWindow("drawContours", cv::WINDOW_NORMAL);
			cv::imshow("drawContours", blackimg);
			cv::waitKey(0);
	}
	// 4. 用绿线画出这些找到的轮廓
	//std::vector<std::vector<cv::Point>> conpoint(region.size());
	//std::vector<cv::Rect> boundRect(region.size());
	cv::Mat whiteimg(result.rows, result.cols, CV_8UC1, cv::Scalar(255,255, 255));//255
	for (int l = 0; l < graypic.size(); l++) {
		cv::drawContours(whiteimg, graypic,l, (0, 0, 0), CV_FILLED);
	}

	//cv::imwrite("rectangle.png", gray);
	//cv::drawContours(gray, region, -1, (0, 0, 255), 3);
	if (Debug_show_process) {
		cv::namedWindow("whiteimg", cv::WINDOW_NORMAL);
		cv::imshow("whiteimg", whiteimg);
		cv::waitKey(0);
	}
	cv::Mat textFind;
	bitwise_and(preGray, blackimg, textFind);
	if (Debug_show_process) {
		cv::namedWindow("textFind", cv::WINDOW_NORMAL);
		cv::imshow("textFind", textFind);
		cv::waitKey(0);
	}
	bitwise_and(textFind, whiteimg, result);
	// 带轮廓的图片
	return result;

}
/***************************************
**HelloThread::preprocess(cv::Mat gray)
**功能：调用tesseract API进行字符识别的方法
**
***************************************/
cv::Mat HelloThread::preprocess(cv::Mat gray) {
	// 1. Sobel算子，x方向求梯度
	Sobel(gray, gray, CV_8U, 1, 0, 3);
	// 2. 二值化
	cv::threshold(gray, gray, 0, 255, cv::THRESH_OTSU | cv::THRESH_BINARY);
	cv::Mat element1, element2, dilation, erosion, dilation2;
	// 3. 膨胀和腐蚀操作的核函数
	element1 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(15, 4));//(10,3)(x,y)  x左右方向，y上下方向
	element2 = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(30, 4));//(30,5)

	// 4. 膨胀一次，让轮廓突出
	cv::dilate(gray, dilation, element2);

	// 5. 腐蚀一次，去掉细节，如表格线等。注意这里去掉的是竖直的线
	cv::erode(dilation, erosion, element1);

	// 6. 再次膨胀，让轮廓明显一些
	cv::dilate(erosion, dilation2, element2, cv::Point(-1, -1), 3);

	// 7. 存储中间图片
	//cv::imwrite("binary.png", gray);
	//cv::imwrite("dilation.png", dilation);
	//cv::imwrite("erosion.png", erosion);
	//cv::imwrite("dilation2.png", dilation2);

	return dilation2;

}


/***************************************
**HelloThread::run()
**功能：子线程核心方法
**
***************************************/
void HelloThread::run()
{
	qDebug() << tr("Run");
	if (!mypath.empty())
	{
		//emit updateui("更新界面");
		cv::Mat im = cv::imread(mypath);
		if (im.empty())
		{

			//strLog += tr("opencv 读取图片出错！\r\n");
			emit updateui("Error loading image！\r\n");
			return;
		}
		qDebug() << tr("准备识别中");
		cv::Mat gray;
		qDebug() << tr("cvtColor");
		cv::cvtColor(im, gray, CV_BGR2GRAY);

		//去除阴影
		gray = removeShadow(gray);
		//识别前显示图片
		if (Debug_show_process) {
			cv::namedWindow("result0", CV_WINDOW_NORMAL);
			cv::imshow("result0", gray);
			cv::waitKey(0);
		}
		//去除图片中非文字部分。
		cv::Mat grayROI = detect(im, gray);

		if (Debug_show_process) {
			cv::namedWindow("result1", CV_WINDOW_NORMAL);
			cv::imshow("result1", grayROI);
			cv::waitKey(0);
			cv::imwrite("result.png", grayROI);
		}

		//开始识别
		cv::Mat resizeROI;
		cv::resize(grayROI, resizeROI, cv::Size((int)grayROI.cols*1.2, (int)grayROI.rows*1.2), (0, 0), (0, 0), cv::INTER_LINEAR);
		ocrMain(resizeROI);

	}
	qDebug() << tr("exit");
	
}
/***************************************
**HelloThread::OcrMain(cv::Mat gray) 
**功能：调用tesseract API进行字符识别的方法
**
***************************************/
void HelloThread::ocrMain(cv::Mat gray) 
{
	
	//detect(gray);
	// Pass it to Tesseract API
	tesseract::TessBaseAPI tess;
	qDebug() << tr("Init");
	tess.Init("./res/tessdata/", "eng+chi_sim2+chi_sim7", tesseract::OEM_DEFAULT);//chi_sim2+
	qDebug() << tr("SetPageSegMode");
	tess.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
	qDebug() << tr("SetImage");
	tess.SetImage((uchar*)gray.data, gray.cols, gray.rows, 1, gray.cols);
	qDebug() << tr("GetUTF8Text");
	// Get the text
	clock_t start0, finish0;
	double totaltime0;
	start0 = clock();
	char* out = tess.GetUTF8Text();
	finish0 = clock();
	totaltime0 = (double)(finish0 - start0) / CLOCKS_PER_SEC;
	qDebug() << "\nInit运行时间为" << totaltime0 << "秒！" << endl;
	int values = tess.MeanTextConf();
	qDebug() << tr("MeanValues==") << values;
	int * confidences = tess.AllWordConfidences();
	int k = 0;
	while (*confidences != '\0')
	{
		k++;
		qDebug() << tr("第") << k << tr("个==") << *confidences;
		confidences++;
	}
	//for(int i =0;i<)
	//qDebug() << confidences;
	//wchar_t*unicode;
	//char* outunicode;
	//CN2Unicode(out, unicode);
	//outunicode =WcharToChar(unicode);
	//std::string outstring, outgtk;
	//outstring = out;
	//size_t *unicode_number;
	//outgtk = UTF8_To_GBK(outstring);
	//char* outgtk =U2G(out);
	char *p = out;
	while (*p)//字符串不结束就循环
	{
		//qDebug() <<*p;
		if (*p >= 'A' && *p <= 'Z') //判断大写字母
		{
			*p += 32; //转小写
		}
		if (*p == '0' && (('a' <= *(p - 1)&& *(p - 1) <= 'z')||('a' <= *(p + 1)&& *(p + 1) <= 'z')) ){
			//qDebug() << myout.at(i - 1);
			*p = 'o';
			//continue;
		}

		p++; //指针后指，准备处理下一个字母
	}

	qDebug() << tr("updateui");
	//const char *k = outgtk.c_str();
	//char *result = k;
	emit updateui(out);
	mypath = "";
	tts(out);
	//delete out;

}
char* HelloThread::WcharToChar(wchar_t* wc)
{
	//Release();
	int len = WideCharToMultiByte(CP_ACP, 0, wc, wcslen(wc), NULL, 0, NULL, NULL);
	char* m_char = new char[len + 1];
	WideCharToMultiByte(CP_ACP, 0, wc, wcslen(wc), m_char, len, NULL, NULL);
	m_char[len] = '\0';
	return m_char;
}
int HelloThread::CN2Unicode(char *input, wchar_t *output)
{
	int len = strlen(input);

	//wchar_t *out = (wchar_t *) malloc(len*sizeof(wchar_t));

	len = MultiByteToWideChar(CP_ACP, 0, input, -1, output, MAX_PATH);

	return 1;
}
char* HelloThread::U2G(const char* utf8)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1];
	memset(wstr, 0, len + 1);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, len);
	len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	char* str = new char[len + 1];
	memset(str, 0, len + 1);
	WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, len, NULL, NULL);
	if (wstr) delete[] wstr;
	return str;
}
std::string HelloThread::UTF8_To_GBK(const std::string &source)
{
	enum { GB2312 = 936 };

	unsigned long len = ::MultiByteToWideChar(CP_UTF8, NULL, source.c_str(), -1, NULL, NULL);
	if (len == 0)
		return std::string();
	wchar_t *wide_char_buffer = new wchar_t[len];
	::MultiByteToWideChar(CP_UTF8, NULL, source.c_str(), -1, wide_char_buffer, len);

	len = ::WideCharToMultiByte(GB2312, NULL, wide_char_buffer, -1, NULL, NULL, NULL, NULL);
	if (len == 0)
	{
		delete[] wide_char_buffer;
		return std::string();
	}
	char *multi_byte_buffer = new char[len];
	::WideCharToMultiByte(GB2312, NULL, wide_char_buffer, -1, multi_byte_buffer, len, NULL, NULL);

	std::string dest(multi_byte_buffer);
	delete[] wide_char_buffer;
	delete[] multi_byte_buffer;
	return dest;
}
 char* HelloThread::QXUtf82Unicode(const char* utf, size_t *unicode_number)
{
	if (!utf || !strlen(utf))
	{
		*unicode_number = 0;
		return NULL;
	}
	int dwUnicodeLen = MultiByteToWideChar(CP_UTF8, 0, utf, -1, NULL, 0);
	size_t num = dwUnicodeLen * sizeof(wchar_t);
	wchar_t *pwText = (wchar_t*)malloc(num);
	memset(pwText, 0, num);
	MultiByteToWideChar(CP_UTF8, 0, utf, -1, pwText, dwUnicodeLen);
	*unicode_number = dwUnicodeLen - 1;
	return (char*)pwText;
}
/***************************************
**HelloThread::preprocess(cv::Mat gray)
**功能：调用tesseract API进行字符识别的方法
**
***************************************/
void HelloThread::tts(const char* text) {
	qDebug() << tr("tts");
	int         ret = MSP_SUCCESS;
	const char* login_params = "appid = 58abe889, work_dir = .";//登录参数,appid与msc库绑定,请勿随意改动
																/*
																* rdn:           合成音频数字发音方式
																* volume:        合成音频的音量
																* pitch:         合成音频的音调
																* speed:         合成音频对应的语速
																* voice_name:    合成发音人
																* sample_rate:   合成音频采样率
																* text_encoding: 合成文本编码格式
																*
																* 详细参数说明请参阅《iFlytek MSC Reference Manual》
																*/
	char* session_begin_params = "engine_type = local, voice_name = xiaoyan, text_encoding = utf8, tts_res_path = fo|res\\tts\\xiaoyan.jet;fo|res\\tts\\common.jet, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 2";
	char* filename = "tts_sample.wav"; //合成的语音文件名称
	//const char* text = "智能机器人工程。亲爱的用户，您好，这是一个语音合成示例，感谢您对科大讯飞语音技术的支持！科大讯飞是亚太地区最大的语音上市公司，股票代码：002230"; //合成文本
																									 /* 用户登录 */
	ret = MSPLogin(NULL, NULL, login_params); //第一个参数是用户名，第二个参数是密码，第三个参数是登录参数，用户名和密码可在http://open.voicecloud.cn注册获取
	if (MSP_SUCCESS != ret)
	{
		qDebug() << tr("MSPLogin failed, error code: %d.\n"+ret);
		goto exit;//登录失败，退出登录
	}

	printf("\n###########################################################################\n");
	printf("## 语音合成（Text To Speech，TTS）技术能够自动将任意文字实时转换为连续的 ##\n");
	printf("## 自然语音，是一种能够在任何时间、任何地点，向任何人提供语音信息服务的  ##\n");
	printf("## 高效便捷手段，非常符合信息时代海量数据、动态更新和个性化查询的需求。  ##\n");
	printf("###########################################################################\n\n");

	/* 文本合成 */
	qDebug() << tr("开始合成 ...\n");
	ret = text_to_speech(text, filename, session_begin_params);
	//delete text;
	qDebug() << ret;
	if (MSP_SUCCESS != ret)
	{
		qDebug() << tr("text_to_speech failed, error code: %d.\n"+ret);
	}
	qDebug() << tr("合成完毕\n");

exit:
	qDebug() << tr("按任意键退出 ...\n");
	//_getch();
	MSPLogout(); //退出登录
	QFile fileOut("tts_sample.wav");
	//打开
	if (!fileOut.exists())
	{
		//QMessageBox::warning(this, tr("Open a file"), tr("Failed to open the destination file：") + fileOut.errorString());
		return;
	}else if(MSP_SUCCESS == ret){
		qDebug() << tr("播放音频");
		emit opensig();
	}
	//fileOut.close();
	return;
}

/***************************************
**HelloThread::findTextRegion(cv::Mat gray)
**功能：
**
***************************************/

std::vector< std::vector<cv::Point> >  HelloThread::findTextRegion(cv::Mat gray) {
	std::vector< std::vector<cv::Point> > contours, region,regionmin;
	//CvPoint2D32f *region;
	std::vector<cv::Vec4i> hierarchy;
	//CvMemStorage * contours = cvCreateMemStorage(0);
	// 1. 查找轮廓
	cv::findContours(gray, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
	//CvScalar color = CvScalar(0, 0, 255);
	int k = 0;
	double sumHeight = 0;
	double sumAngle = 0;
	double sumArea = 0;
	int maxId;
	//cv::Mat height_width;
	// 2. 筛选那些面积小的
	for (int i = 0; i <contours.size(); i++) {
		std::vector<cv::Point> cnt, approx;
		cnt = contours[i];
		cv::RotatedRect  rect;
		// 找到最小的矩形，该矩形可能有方向
		rect = cv::minAreaRect(cnt);
		std::vector<cv::Point>  box;
		cv::Point2f rect_points[4];
		//cv::boxPoints(rect, box);
		rect.points(rect_points);
		double angle = rect.angle;
		for (int j = 0; j < 4; j++) {
			box.push_back(rect_points[j]);
		}

		//int mod = 6;
		//printf("box==%d\n", box);
		//printf("rect_points==%s", rect_points.y);
		//cv::drawContours(gray, box, 0, (0, 255, 0), 3);

		//printf("box==", box[0].x);
		//box = std::int(box);

		float height, width;
		// 计算高和宽
		height = abs(box[0].y - box[2].y);
		width = abs(box[0].x - box[2].x);

		//height_width.at<int>(i,0) = height;
		//height_width.at<int>(i, 1) = width;
		//sumHeight = sumHeight + height;
		// 计算该轮廓的面积
		int area;
		area = cv::contourArea(cnt);
		// 面积小的都筛选掉
		if (area < 60 * height) {
			continue;
		}
		if (area <5000) {
			continue;
		}
		// 筛选那些太细的矩形，留下扁的
		if (height > width * 1.2) {
			continue;
		}


		sumHeight = sumHeight + height;
		sumAngle = sumAngle + angle;
		sumArea = sumArea + area;
		region.push_back(box);
		box.clear();
		k++;

	}
	//float meanHeight = sumHeight / k;
	//printf("meanHeight==%f", meanHeight);
	//cvNamedWindow("cvRectangleR", CV_WINDOW_NORMAL);
	//cvShowImage("cvRectangleR", src);
	//cvSaveImage("test.jpg", src);
	//cvWaitKey(0);
	///regionmin = region;
	for (int s = 0; s < region.size(); s++) {
		if (cv::contourArea(region[s]) < (sumArea / k) * 5) {
			regionmin.push_back(region[s]);
		}
	
	}

	return regionmin;

}
/***************************************
**HelloThread::findTextRegion(cv::Mat gray)
**功能：
**
***************************************/

std::vector< std::vector<cv::Point> >  HelloThread::cutPicRegion(cv::Mat gray) {
	std::vector< std::vector<cv::Point> > contours, region, regionmin;
	//CvPoint2D32f *region;
	std::vector<cv::Vec4i> hierarchy;
	//CvMemStorage * contours = cvCreateMemStorage(0);
	// 1. 查找轮廓
	cv::Mat gray0 = gray;
	cv::findContours(gray0, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
	//CvScalar color = CvScalar(0, 0, 255);
	int k = 0;
	double sumHeight = 0;
	double sumWidth = 0;
	double sumAngle = 0;
	double sumArea = 0;
	int maxId;
	//cv::Mat height_width;
	// 2. 筛选那些面积小的
	for (int i = 0; i <contours.size(); i++) {
		std::vector<cv::Point> cnt, approx;
		cnt = contours[i];
		//printf("cnt==%d", cnt[0].x);
		//printf("cnt==%d", cnt[0].y);

		// 轮廓近似，作用很小
		//double  epsilon;
		//epsilon = 0.001 * cv::arcLength(cnt, true);
		//cv::approxPolyDP(cnt, approx, epsilon, true);
		cv::RotatedRect  rect;
		// 找到最小的矩形，该矩形可能有方向
		rect = cv::minAreaRect(cnt);
		std::vector<cv::Point>  box;
		cv::Point2f rect_points[4];
		//cv::boxPoints(rect, box);
		rect.points(rect_points);
		double angle = rect.angle;
		for (int j = 0; j < 4; j++) {
			box.push_back(rect_points[j]);
		}

		//int mod = 6;
		//printf("box==%d\n", box);
		//printf("rect_points==%s", rect_points.y);
		//cv::drawContours(gray, box, 0, (0, 255, 0), 3);

		//printf("box==", box[0].x);
		//box = std::int(box);

		float height, width;
		// 计算高和宽
		height = abs(box[0].y - box[2].y);
		width = abs(box[0].x - box[2].x);

		//height_width.at<int>(i,0) = height;
		//height_width.at<int>(i, 1) = width;
		//sumHeight = sumHeight + height;
		// 计算该轮廓的面积
		int area;
		area = cv::contourArea(cnt);
		// 面积小的都筛选掉
		if (area < 60 * height) {
			continue;
		}
		if (area <10000) {
			continue;
		}
		// 筛选那些太细的矩形，留下扁的
		if (height > width * 1.2) {
			continue;
		}


		sumHeight = sumHeight + height;
		sumWidth = sumWidth + width;
		sumAngle = sumAngle + angle;
		sumArea = sumArea + area;
		region.push_back(box);
		box.clear();
		k++;

	}
	//float meanHeight = sumHeight / k;
	//printf("meanHeight==%f", meanHeight);
	//cvNamedWindow("cvRectangleR", CV_WINDOW_NORMAL);
	//cvShowImage("cvRectangleR", src);
	//cvSaveImage("test.jpg", src);
	//cvWaitKey(0);
	///regionmin = region;
	
	for (int s = 0; s < region.size(); s++) {
		if ((cv::contourArea(region[s]) > (sumArea / k) *4)) {
			if (k >= 4) {
				if ((abs(region[s][0].y - region[s][2].y) > 3 * sumHeight / k)&& abs(region[s][0].x - region[s][2].x)>0.25*gray.cols) {
					regionmin.push_back(region[s]);
				}

			}
			else {
				regionmin.push_back(region[s]);
			}
		}
	}
	
	return regionmin;

}

/***************************************
**HelloThread::removeShadow(cv::Mat img)
**功能：去除图片中的阴影部分
**
***************************************/
cv::Mat HelloThread::removeShadow(cv::Mat img)
{
	cv::Mat img_filter;
	///////////////////////开始
	
	//cv::Mat element = cv::getStructuringElement(0,
		//cvSize(2 * 1 + 1, 2 * 1 + 1),
		//cvPoint(-1, -1));
	//cv::erode(img, img, element);
	//cv::bilateralFilter(img_filter, img, 11, 11, 11);
	cv::bilateralFilter(img, img_filter, 10, 7, 10);//9->11（10,9,11）->10, 7, 10
	cv::GaussianBlur(img_filter, img, cv::Size(5, 5), 2);
	cv::adaptiveThreshold(img, img_filter, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY_INV, 7, 2);
	//二值化后再进行双边滤波、中指滤波、均值滤波，充分除去噪点。
	cv::bilateralFilter(img_filter, img, 10, 9, 11);//(10, 9, 11)
	cv::medianBlur(img, img, 3);
	
	cv::blur(img, img, cvSize(3, 3), cvPoint(-1, -1));
	//cv::medianBlur(img, img, 3);
	//cv::blur(img, img, cvSize(3, 3), cvPoint(-1, -1));
	return img;

}
/***************************************
**cv::Mat HelloThread::mser_img(cv::Mat img)
**功能：纠正原图文字有旋转角度的算法
**可以实现正负0~90度（不包括90度）的角度纠正。
***************************************/
cv::Mat HelloThread::mser_img(cv::Mat img)
{

	cv::Point center(img.cols / 2, img.rows / 2);
	cv::Mat padded;
	int opWidth = cv::getOptimalDFTSize(img.rows);
	int opHeight = cv::getOptimalDFTSize(img.cols);
	copyMakeBorder(img, padded, 0, opWidth - img.rows, 0, opHeight - img.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0));

	cv::Mat planes[] = { cv::Mat_<float>(padded), cv::Mat::zeros(padded.size(), CV_32F) };
	cv::Mat comImg, dftimg;
	//Merge into a double-channel image
	merge(planes, 2, comImg);

	//Use the same image as input and output,
	//so that the results can fit in Mat well
	try {
		dft(comImg, dftimg, 0);
	}
	catch (cv::Exception &e) 
	{

		return img;
	}
	//Compute the magnitude
	//planes[0]=Re(DFT(I)), planes[1]=Im(DFT(I))
	//magnitude=sqrt(Re^2+Im^2)
	split(dftimg, planes);
	cv::magnitude(planes[0], planes[1], planes[0]);

	//Switch to logarithmic scale, for better visual results
	//M2=log(1+M1)
	cv::Mat magMat = planes[0];
	magMat += cv::Scalar::all(1);
	log(magMat, magMat);

	//Crop the spectrum
	//Width and height of magMat should be even, so that they can be divided by 2
	//-2 is 11111110 in binary system, operator & make sure width and height are always even
	magMat = magMat(cv::Rect(0, 0, magMat.cols & -2, magMat.rows & -2));

	//Rearrange the quadrants of Fourier image,
	//so that the origin is at the center of image,
	//and move the high frequency to the corners
	int cx = magMat.cols / 2;
	int cy = magMat.rows / 2;

	cv::Mat q0(magMat, cv::Rect(0, 0, cx, cy));
	cv::Mat q1(magMat, cv::Rect(0, cy, cx, cy));
	cv::Mat q2(magMat, cv::Rect(cx, cy, cx, cy));
	cv::Mat q3(magMat, cv::Rect(cx, 0, cx, cy));

	cv::Mat tmp;
	q0.copyTo(tmp);
	q2.copyTo(q0);
	tmp.copyTo(q2);

	q1.copyTo(tmp);
	q3.copyTo(q1);
	tmp.copyTo(q3);

	//Normalize the magnitude to [0,1], then to[0,255]
	normalize(magMat, magMat, 0, 1, CV_MINMAX);
	cv::Mat magImg(magMat.size(), CV_8UC1);
	magMat.convertTo(magImg, CV_8UC1, 255, 0);
	//cv::namedWindow("magnitude", CV_WINDOW_NORMAL);
	//imshow("magnitude", magImg);
	//imwrite("imageText_mag.jpg",magImg);

	//Turn into binary image
	threshold(magImg, magImg, GRAY_THRESH, 255, CV_THRESH_BINARY);
	//cv::namedWindow("mag_binary", CV_WINDOW_NORMAL);
	//imshow("mag_binary", magImg);
	//imwrite("imageText_bin.jpg",magImg);

	//Find lines with Hough Transformation
	std::vector<cv::Vec2f> lines;
	float pi180 = (float)CV_PI / 180;
	cv::Mat linImg(magImg.size(), CV_8UC3);
	HoughLines(magImg, lines, 1, pi180, HOUGH_VOTE, 0, 0);
	int numLines = lines.size();
	for (int l = 0; l<numLines; l++)
	{
		float rho = lines[l][0], theta = lines[l][1];
		cv::Point pt1, pt2;
		double a = cos(theta), b = sin(theta);
		double x0 = a*rho, y0 = b*rho;
		pt1.x = cvRound(x0 + 1000 * (-b));
		pt1.y = cvRound(y0 + 1000 * (a));
		pt2.x = cvRound(x0 - 1000 * (-b));
		pt2.y = cvRound(y0 - 1000 * (a));
		line(linImg, pt1, pt2, cv::Scalar(255, 0, 0), 3, 8, 0);
	}
	
	//cv::namedWindow("lines", CV_WINDOW_NORMAL);
	//imshow("lines", linImg);
	
	//Find the proper angel from the three found angels
	float angel = 0;
	float piThresh = (float)CV_PI / 90;
	float pi2 = CV_PI / 2;
	for (int l = 0; l<numLines; l++)
	{
		float theta = lines[l][1];
		if (abs(theta) < piThresh || abs(theta - pi2) < piThresh)
			continue;
		else {
			angel = theta;
			break;
		}
	}

	//Calculate the rotation angel
	//The image has to be square,
	//so that the rotation angel can be calculate right
	angel = angel<pi2 ? angel : angel - CV_PI;
	if (angel != pi2) {
		float angelT = img.rows*tan(angel) / img.cols;
		angel = atan(angelT);
	}
	float angelD = angel * 180 / (float)CV_PI;
	//cout << "the rotation angel to be applied:" << endl << angelD << endl << endl;
	float angel2 = angel * 180 / 3.1415;
	if (angel2 > 80) {
		return img;
	}
	float col_change;
		float scale;
	if (img.cols <= img.rows) {
		col_change = img.cols * 1 / cos(abs(angel)) + sin(abs(angel))*(img.rows - img.cols*tan(abs(angel)));
		scale = img.cols / col_change;
	}
	else {
		col_change = img.rows * 1 / cos(abs(angel)) + sin(abs(angel))*(img.cols - img.rows*tan(abs(angel)));
		scale = img.rows / col_change;

	}
	if (scale >= 0.9&&scale < 1.2) {
		scale = scale / (float)1.4;
	}
	scale = scale * 1;
	//Rotate the image to recover
	cv::Mat rotMat = getRotationMatrix2D(center, angelD, scale*1.4);
	//cv::Mat rotMat = HelloThread::angleRectify( img,  angelD);
	cv::Mat dstImg = cv::Mat::ones(img.size(), CV_8UC3);
	warpAffine(img, dstImg, rotMat,img.size(), 1, 0, cv::Scalar(255, 255, 255));
	/*
	cv::Mat kernel = cv::getStructuringElement(CV_SHAPE_RECT, cv::Size(20, 20));
	cv::morphologyEx(matGray, matGray, Imgproc.MORPH_OPEN, kernel);
	Imgproc.morphologyEx(matGray, matGray, Imgproc.MORPH_CLOSE, kernel);
	cv::namedWindow("result", CV_WINDOW_NORMAL);
	imshow("result", dstImg);

	cv::waitKey(0);
	*/
	
	return dstImg;
}
/***************************************
** HelloThread::recognize(cv::String path) 
**
**功能：子程序控制参数：mypath有值则子程序会对图片进行预处理。
**
***************************************/
void HelloThread::recognize(cv::String path) 
{
	qDebug() << tr("recognize");
	mypath = path;
	//mycond.wakeAll(); 

}
void WindowsForFun::on_pushButton_start_clicked()
{
	/*
	if (sound!=NULL) {
		if (!sound->isFinished()) {
			qDebug() << tr("sound->stop");
			sound->stop();
			//sound = NULL;
			delete sound;
			sound = NULL;

		}

	};
	*/
	if (mediaPlayer.state() == QMediaPlayer::PlayingState) {

		mediaPlayer.stop();
		mediaPlayer.setMedia(QUrl::fromLocalFile(""));
	}else if (mediaPlayer.mediaStatus() != QMediaPlayer::NoMedia) {
		mediaPlayer.setMedia(QUrl::fromLocalFile(""));
	}
	//设置Browse按钮不可点击
	ui.pushButton_browse->setEnabled(false);
	//设置start按钮不可点击
	ui.pushButton_start->setEnabled(false);
	qDebug() << tr("Processing images ......\r\n");
	//display_time();
	QString file_name, file_path;
	cv::String file_path_cv, file_name_cv;
	//do some thing
	file_name = fi.fileName();
	file_path = fi.absolutePath();
	file_path_cv = file_path.toStdString();
	file_name_cv = file_name.toStdString();
	qDebug() << tr("调用识别函数starttess");
	mythread->start();
	qDebug() << tr("mythread->start");
	//mythread->wait();
	emit starttess(file_path_cv + '/' + file_name_cv);

	

}

void WindowsForFun::on_pushButton_stop_clicked()
{
	/*
	if (sound != NULL) {
		if (!sound->isFinished()) {
			sound->stop();
			//sound = NULL;
			delete sound;
			sound = NULL;
			mythread->terminate();
			mythread->wait();
		}

	};
	*/
	if (mediaPlayer.state() == QMediaPlayer::PlayingState) {

		mediaPlayer.stop();
		mediaPlayer.setMedia(QUrl::fromLocalFile(""));
	}else if (mediaPlayer.mediaStatus() != QMediaPlayer::NoMedia) {
		mediaPlayer.setMedia(QUrl::fromLocalFile(""));
	}
	if (ui.pushButton_start->isEnabled()){
		qDebug() << tr("No images are currently being processed\r\n");
		//display_time();
		return;
	}
	//Browse按钮恢复可点击状态
	ui.pushButton_browse->setEnabled(true);
	//恢复start按钮可点击状态
	ui.pushButton_start->setEnabled(true);
	mythread->terminate();
	mythread->wait();
	qDebug() << tr("Stop processing the image\r\n");
	//display_time();
	

}
void WindowsForFun::display_text()
{

	//QDateTime time = QDateTime::currentDateTime();//获取系统现在的时间
	//QString str = time.toString("yyyy-MM-dd hh:mm:ss \r\n"); //设置显示格式
	strLog += tr("------------------------------------\r\n");
	//strLog += str;
	//    label->setText(str);//在标签上显示时间
	ui.plainTextEdit->setPlainText(strLog);
	QTextCursor cursor = ui.plainTextEdit->textCursor();
	cursor.movePosition(QTextCursor::End);
	ui.plainTextEdit->setTextCursor(cursor);
	//save_output_log();
}
void WindowsForFun::save_output_log()
{
	//定义文件对象,output_log.txt 为文件名
	QFile fileOut("output_log.txt");
	//打开
	if( ! fileOut.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
	{
	QMessageBox::warning(this, tr("Open a file"),tr("Failed to open the destination file：") + fileOut.errorString());
	return;
	}
	fileOut.write( strLog.toLocal8Bit() );
	fileOut.close();
	qDebug() << tr("写入文件成功");

}
void WindowsForFun::apendtext(QString text) {

	//Browse按钮恢复可点击状态
	ui.pushButton_browse->setEnabled(true);
	//恢复start按钮可点击状态
	ui.pushButton_start->setEnabled(true);
	/*
	for (int k = 0; k < text.size(); k++) {
		qDebug() << k;
		if (text[k]== '00\ufb01') {
			qDebug() << tr("发现时");

		}else {
			qDebug() << text[k];

		}

	}
	*/
	strLog += text;
	display_text();
	/*if (sound != NULL) {
		sound->stop();
		delete sound;
		sound = NULL;

	}
	*/
	//strLog += tr("Processing is complete\r\n");
	//display_time();
}