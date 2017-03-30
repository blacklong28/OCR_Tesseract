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
**UI����
**����ʼ�ڴ�
**
***************************************/


bool Debug_show_process = TRUE;//FALSE TRUE

#ifdef _WIN64
#pragma comment(lib,"msc_x64.lib")//x64
#else
#pragma comment(lib,"msc.lib")//x86
#endif

/* wav��Ƶͷ����ʽ */
typedef struct _wave_pcm_hdr
{
	char            riff[4];                // = "RIFF"
	int				size_8;                 // = FileSize - 8
	char            wave[4];                // = "WAVE"
	char            fmt[4];                 // = "fmt "
	int				fmt_size;				// = ��һ���ṹ��Ĵ�С : 16

	short int       format_tag;             // = PCM : 1
	short int       channels;               // = ͨ���� : 1
	int				samples_per_sec;        // = ������ : 8000 | 6000 | 11025 | 16000
	int				avg_bytes_per_sec;      // = ÿ���ֽ��� : samples_per_sec * bits_per_sample / 8
	short int       block_align;            // = ÿ�������ֽ��� : wBitsPerSample / 8
	short int       bits_per_sample;        // = ����������: 8 | 16

	char            data[4];                // = "data";
	int				data_size;              // = �����ݳ��� : FileSize - 44 
} wave_pcm_hdr;

/* Ĭ��wav��Ƶͷ������ */
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
/* �ı��ϳ� */
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
	/* ��ʼ�ϳ� */
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
	printf("���ںϳ� ...\n");
	fwrite(&wav_hdr, sizeof(wav_hdr), 1, fp); //���wav��Ƶͷ��ʹ�ò�����Ϊ16000
	while (1)
	{
		/* ��ȡ�ϳ���Ƶ */
		const void* data = QTTSAudioGet(sessionID, &audio_len, &synth_status, &ret);
		if (MSP_SUCCESS != ret)
			break;
		if (NULL != data)
		{
			fwrite(data, audio_len, 1, fp);
			wav_hdr.data_size += audio_len; //����data_size��С
		}
		if (MSP_TTS_FLAG_DATA_END == synth_status)
			break;
	}//�ϳ�״̬synth_statusȡֵ����ġ�Ѷ��������API�ĵ���
	//printf("\n");
	if (MSP_SUCCESS != ret)
	{
		printf("QTTSAudioGet failed, error code: %d.\n", ret);
		QTTSSessionEnd(sessionID, "AudioGetError");
		fclose(fp);
		return ret;
	}
	/* ����wav�ļ�ͷ���ݵĴ�С */
	wav_hdr.size_8 += wav_hdr.data_size + (sizeof(wav_hdr) - 8);

	/* ��������������д���ļ�ͷ��,��Ƶ�ļ�Ϊwav��ʽ */
	fseek(fp, 4, 0);
	fwrite(&wav_hdr.size_8, sizeof(wav_hdr.size_8), 1, fp); //д��size_8��ֵ
	fseek(fp, 40, 0); //���ļ�ָ��ƫ�Ƶ��洢data_sizeֵ��λ��
	fwrite(&wav_hdr.data_size, sizeof(wav_hdr.data_size), 1, fp); //д��data_size��ֵ
	fclose(fp);
	fp = NULL;
	/* �ϳ���� */
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
	//��ʼ����Ա����
	setFixedSize(this->width(), this->height());
	m_pPixMap = NULL;
	//����log��Ϊֻ��
	ui.plainTextEdit->setReadOnly(true);
	mythread = new HelloThread;
	
	connect(mythread, SIGNAL(updateui(QString)), this, SLOT(apendtext(QString)));
	connect(this, SIGNAL(starttess(cv::String)), mythread, SLOT(recognize(cv::String)));
	connect(mythread, SIGNAL(opensig()), this, SLOT(OpenWav()));
	//connect(ui.pushButton_start, SIGNAL(clicked()), this, SLOT(on_pushButton_start_clicked()));
	//connect(ui.pushButton_stop, SIGNAL(clicked()), this, SLOT(on_pushButton_stop_clicked()));


}
/***************************************
**���Browse��ť�źŵĲۺ���
**
**
***************************************/
void WindowsForFun::on_pushButton_browse_clicked()
{
	   //�ļ���
	strFileName = QFileDialog::getOpenFileName(this, tr("Open the image to be processed"), "",
		"Pictures (*.bmp *.jpg *.jpeg *.png *.xpm);;All files(*)");
	if (strFileName.isEmpty())
	{
		//�ļ���Ϊ�գ�����
		return;
	}
	//�����ʾͼƬ�����ͼƬ
	ClearOldShow();
	//��ӡ�ļ���
	qDebug() << strFileName;

	//�½�����ͼ
	m_pPixMap = new QPixmap();
	QPixmap m_pPixMap_scale;
	//����
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
		//���سɹ�
		//���ø���ǩ
		ui.labelshow->setPixmap(m_pPixMap_scale);

		//���ñ�ǩ���´�С��������ͼһ����
		//ui->labelshow->setGeometry( m_pPixMap->rect() );
		//strLog += tr("Load image success\r\n");
		QString  file_name, file_path;
		
		//file_full = QFileDialog::getOpenFileName(this);

		fi = QFileInfo(strFileName);
		file_name = fi.fileName();
		file_path = fi.absolutePath();
		qDebug() << tr("The current image name is��%1\r\n").arg(file_name);
		qDebug() << tr("The image path is��%1\r\n").arg(file_path);

	}
	else
	{
		qDebug() << tr("Failed to load image\r\n");
		//����ʧ�ܣ�ɾ��ͼƬ���󣬷���
		delete m_pPixMap;   m_pPixMap = NULL;
		//��ʾʧ��
		QMessageBox::critical(this, tr("Open failed"),
			tr("Failed to open the image, the file name is��\r\n%1").arg(strFileName));
	}
	//display_time();

}
/***************************************
**���ܣ����ͼƬ����ԭͼƬ
**
**
***************************************/
void WindowsForFun::ClearOldShow()
{
	//��ձ�ǩ����
	ui.labelshow->clear();
	//����ͼ���վ�ɾ��
	if (m_pPixMap != NULL)
	{
		//ɾ������ͼ
		delete m_pPixMap;   m_pPixMap = NULL;
	}

}
void WindowsForFun::OpenWav()
{

	qDebug() << tr("������Ƶ");
	mediaPlayer.setMedia(QUrl::fromLocalFile("tts_sample.wav"));
	mediaPlayer.play();
	//mediaPlayer.stateChanged(stopTimer);
	//sound = new QSound("tts_sample.wav", this); //��������
	//sound->play();//����
	qDebug() << tr("play");
	
	//sound->stop();//ֹͣ
	//sound->setLoops(1);
}
/***************************************
**���ܣ�ϸ���ַ�
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
	int count = 0;  //��¼��������
	while (true)
	{
		count++;
		if (maxIterations != -1 && count > maxIterations) //���ƴ������ҵ�����������
			break;
		std::vector<uchar *> mFlag; //���ڱ����Ҫɾ���ĵ�
									//�Ե���
		for (int i = 0; i < height; ++i)
		{
			uchar * p = dst.ptr<uchar>(i);
			for (int j = 0; j < width; ++j)
			{
				//��������ĸ����������б��
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
						//���
						mFlag.push_back(p + j);
					}
				}
			}
		}

		//����ǵĵ�ɾ��
		for (std::vector<uchar *>::iterator i = mFlag.begin(); i != mFlag.end(); ++i)
		{
			**i = 0;
		}

		//ֱ��û�е����㣬�㷨����
		if (mFlag.empty())
		{
			break;
		}
		else
		{
			mFlag.clear();//��mFlag���
		}

		//�Ե���
		for (int i = 0; i < height; ++i)
		{
			uchar * p = dst.ptr<uchar>(i);
			for (int j = 0; j < width; ++j)
			{
				//��������ĸ����������б��
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
						//���
						mFlag.push_back(p + j);
					}
				}
			}
		}

		//����ǵĵ�ɾ��
		for (std::vector<uchar *>::iterator i = mFlag.begin(); i != mFlag.end(); ++i)
		{
			**i = 0;
		}

		//ֱ��û�е����㣬�㷨����
		if (mFlag.empty())
		{
			break;
		}
		else
		{
			mFlag.clear();//��mFlag���
		}
	}
	return dst;
}

/***************************************
**���ܣ������������ϸ���㷨
** ������lpDIBBits������ͼ���һά����
**       lWidth��ͼ��߶�
**       lHeight��ͼ����
**       �޷���ֵ
**
***************************************/
bool HelloThread::thiningDIBSkeleton(unsigned char* lpDIBBits, int lWidth, int lHeight)
{
	//ѭ������
	long i;
	long j;
	long lLength;

	unsigned char deletemark[256] = {      // �����Ϊǰ�˾�8�����ܽ���Ƿ���Ա�ɾ����256�����
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
	};//������

	unsigned char p0, p1, p2, p3, p4, p5, p6, p7;
	unsigned char *pmid, *pmidtemp;    // pmid ����ָ���ֵͼ��  pmidtemp����ָ�����Ƿ�Ϊ��Ե
	unsigned char sum;
	bool bStart = true;
	lLength = lWidth * lHeight;
	unsigned char *pTemp = new uchar[sizeof(unsigned char) * lWidth * lHeight]();  //��̬�������� ���ҳ�ʼ��

																				   //    P0 P1 P2
																				   //    P7    P3
																				   //    P6 P5 P4

	while (bStart)
	{
		bStart = false;

		//�������Ե��
		pmid = (unsigned char *)lpDIBBits + lWidth + 1;
		memset(pTemp, 0, lLength);
		pmidtemp = (unsigned char *)pTemp + lWidth + 1; //  ����Ǳ�Ե�� ������Ϊ1
		for (i = 1; i < lHeight - 1; i++)
		{
			for (j = 1; j < lWidth - 1; j++)
			{
				if (*pmid == 0)                   //��0 ����������Ҫ���ǵĵ�
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
					*pmidtemp = 1;       // ������Χ8������1��ʱ��  pmidtemp==1 �����Ǳ�Ե     					
				}

				pmid++;
				pmidtemp++;
			}
			pmid++;
			pmid++;
			pmidtemp++;
			pmidtemp++;
		}

		//���ڿ�ʼɾ��
		pmid = (unsigned char *)lpDIBBits + lWidth + 1;
		pmidtemp = (unsigned char *)pTemp + lWidth + 1;

		for (i = 1; i < lHeight - 1; i++)   // ������ͼ���һ�� ��һ�� ���һ�� ���һ��
		{
			for (j = 1; j < lWidth - 1; j++)
			{
				if (*pmidtemp == 0)     //1�����Ǳ�Ե 0--��Χ8������1 ��Ϊ�м���ݲ��迼��
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
					bStart = true;      //  ��������ɨ�������ϸ��
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
**���ܣ�
**
***************************************/

cv::Mat HelloThread::detect(cv::Mat img, cv::Mat gray)
{
	cv::Mat dilation, original, result;
	cv::Mat preGray=gray.clone();

	// 1.  ת���ɻҶ�ͼ
	if (img.channels() == 3) {
		cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
	}
	original = img.clone();

	std::vector< std::vector<cv::Point> > region,graypic;
	// 2. ��̬ѧ�任��Ԥ�����õ����Բ��Ҿ��ε�ͼƬ
	dilation = preprocess(original);
	if (Debug_show_process) {
		cv::namedWindow("dilation", CV_WINDOW_NORMAL);
		cv::imshow("dilation", dilation);
		cv::waitKey(0);
	}
	//ͨ��dilation������ԭͼ��������,ȥ��������������
	bitwise_and(gray, dilation, result);
	//cv::namedWindow("result", cv::WINDOW_NORMAL);
	//cv::imshow("result", result);
	//cv::waitKey(0);

	//3. ���Һ�ɸѡ��������
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
	//��һ��ȫ�ڵ�ͼƬ�Ͻ����������ð�ɫ����䣬�õ���������
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
	// 4. �����߻�����Щ�ҵ�������
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
	// ��������ͼƬ
	return result;

}
/***************************************
**HelloThread::preprocess(cv::Mat gray)
**���ܣ�����tesseract API�����ַ�ʶ��ķ���
**
***************************************/
cv::Mat HelloThread::preprocess(cv::Mat gray) {
	// 1. Sobel���ӣ�x�������ݶ�
	Sobel(gray, gray, CV_8U, 1, 0, 3);
	// 2. ��ֵ��
	cv::threshold(gray, gray, 0, 255, cv::THRESH_OTSU | cv::THRESH_BINARY);
	cv::Mat element1, element2, dilation, erosion, dilation2;
	// 3. ���ͺ͸�ʴ�����ĺ˺���
	element1 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(15, 4));//(10,3)(x,y)  x���ҷ���y���·���
	element2 = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(30, 4));//(30,5)

	// 4. ����һ�Σ�������ͻ��
	cv::dilate(gray, dilation, element2);

	// 5. ��ʴһ�Σ�ȥ��ϸ�ڣ������ߵȡ�ע������ȥ��������ֱ����
	cv::erode(dilation, erosion, element1);

	// 6. �ٴ����ͣ�����������һЩ
	cv::dilate(erosion, dilation2, element2, cv::Point(-1, -1), 3);

	// 7. �洢�м�ͼƬ
	//cv::imwrite("binary.png", gray);
	//cv::imwrite("dilation.png", dilation);
	//cv::imwrite("erosion.png", erosion);
	//cv::imwrite("dilation2.png", dilation2);

	return dilation2;

}


/***************************************
**HelloThread::run()
**���ܣ����̺߳��ķ���
**
***************************************/
void HelloThread::run()
{
	qDebug() << tr("Run");
	if (!mypath.empty())
	{
		//emit updateui("���½���");
		cv::Mat im = cv::imread(mypath);
		if (im.empty())
		{

			//strLog += tr("opencv ��ȡͼƬ����\r\n");
			emit updateui("Error loading image��\r\n");
			return;
		}
		qDebug() << tr("׼��ʶ����");
		cv::Mat gray;
		qDebug() << tr("cvtColor");
		cv::cvtColor(im, gray, CV_BGR2GRAY);

		//ȥ����Ӱ
		gray = removeShadow(gray);
		//ʶ��ǰ��ʾͼƬ
		if (Debug_show_process) {
			cv::namedWindow("result0", CV_WINDOW_NORMAL);
			cv::imshow("result0", gray);
			cv::waitKey(0);
		}
		//ȥ��ͼƬ�з����ֲ��֡�
		cv::Mat grayROI = detect(im, gray);

		if (Debug_show_process) {
			cv::namedWindow("result1", CV_WINDOW_NORMAL);
			cv::imshow("result1", grayROI);
			cv::waitKey(0);
			cv::imwrite("result.png", grayROI);
		}

		//��ʼʶ��
		cv::Mat resizeROI;
		cv::resize(grayROI, resizeROI, cv::Size((int)grayROI.cols*1.2, (int)grayROI.rows*1.2), (0, 0), (0, 0), cv::INTER_LINEAR);
		ocrMain(resizeROI);

	}
	qDebug() << tr("exit");
	
}
/***************************************
**HelloThread::OcrMain(cv::Mat gray) 
**���ܣ�����tesseract API�����ַ�ʶ��ķ���
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
	qDebug() << "\nInit����ʱ��Ϊ" << totaltime0 << "�룡" << endl;
	int values = tess.MeanTextConf();
	qDebug() << tr("MeanValues==") << values;
	int * confidences = tess.AllWordConfidences();
	int k = 0;
	while (*confidences != '\0')
	{
		k++;
		qDebug() << tr("��") << k << tr("��==") << *confidences;
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
	while (*p)//�ַ�����������ѭ��
	{
		//qDebug() <<*p;
		if (*p >= 'A' && *p <= 'Z') //�жϴ�д��ĸ
		{
			*p += 32; //תСд
		}
		if (*p == '0' && (('a' <= *(p - 1)&& *(p - 1) <= 'z')||('a' <= *(p + 1)&& *(p + 1) <= 'z')) ){
			//qDebug() << myout.at(i - 1);
			*p = 'o';
			//continue;
		}

		p++; //ָ���ָ��׼��������һ����ĸ
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
**���ܣ�����tesseract API�����ַ�ʶ��ķ���
**
***************************************/
void HelloThread::tts(const char* text) {
	qDebug() << tr("tts");
	int         ret = MSP_SUCCESS;
	const char* login_params = "appid = 58abe889, work_dir = .";//��¼����,appid��msc���,��������Ķ�
																/*
																* rdn:           �ϳ���Ƶ���ַ�����ʽ
																* volume:        �ϳ���Ƶ������
																* pitch:         �ϳ���Ƶ������
																* speed:         �ϳ���Ƶ��Ӧ������
																* voice_name:    �ϳɷ�����
																* sample_rate:   �ϳ���Ƶ������
																* text_encoding: �ϳ��ı������ʽ
																*
																* ��ϸ����˵������ġ�iFlytek MSC Reference Manual��
																*/
	char* session_begin_params = "engine_type = local, voice_name = xiaoyan, text_encoding = utf8, tts_res_path = fo|res\\tts\\xiaoyan.jet;fo|res\\tts\\common.jet, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 2";
	char* filename = "tts_sample.wav"; //�ϳɵ������ļ�����
	//const char* text = "���ܻ����˹��̡��װ����û������ã�����һ�������ϳ�ʾ������л���Կƴ�Ѷ������������֧�֣��ƴ�Ѷ������̫���������������й�˾����Ʊ���룺002230"; //�ϳ��ı�
																									 /* �û���¼ */
	ret = MSPLogin(NULL, NULL, login_params); //��һ���������û������ڶ������������룬�����������ǵ�¼�������û������������http://open.voicecloud.cnע���ȡ
	if (MSP_SUCCESS != ret)
	{
		qDebug() << tr("MSPLogin failed, error code: %d.\n"+ret);
		goto exit;//��¼ʧ�ܣ��˳���¼
	}

	printf("\n###########################################################################\n");
	printf("## �����ϳɣ�Text To Speech��TTS�������ܹ��Զ�����������ʵʱת��Ϊ������ ##\n");
	printf("## ��Ȼ��������һ���ܹ����κ�ʱ�䡢�κεص㣬���κ����ṩ������Ϣ�����  ##\n");
	printf("## ��Ч����ֶΣ��ǳ�������Ϣʱ���������ݡ���̬���º͸��Ի���ѯ������  ##\n");
	printf("###########################################################################\n\n");

	/* �ı��ϳ� */
	qDebug() << tr("��ʼ�ϳ� ...\n");
	ret = text_to_speech(text, filename, session_begin_params);
	//delete text;
	qDebug() << ret;
	if (MSP_SUCCESS != ret)
	{
		qDebug() << tr("text_to_speech failed, error code: %d.\n"+ret);
	}
	qDebug() << tr("�ϳ����\n");

exit:
	qDebug() << tr("��������˳� ...\n");
	//_getch();
	MSPLogout(); //�˳���¼
	QFile fileOut("tts_sample.wav");
	//��
	if (!fileOut.exists())
	{
		//QMessageBox::warning(this, tr("Open a file"), tr("Failed to open the destination file��") + fileOut.errorString());
		return;
	}else if(MSP_SUCCESS == ret){
		qDebug() << tr("������Ƶ");
		emit opensig();
	}
	//fileOut.close();
	return;
}

/***************************************
**HelloThread::findTextRegion(cv::Mat gray)
**���ܣ�
**
***************************************/

std::vector< std::vector<cv::Point> >  HelloThread::findTextRegion(cv::Mat gray) {
	std::vector< std::vector<cv::Point> > contours, region,regionmin;
	//CvPoint2D32f *region;
	std::vector<cv::Vec4i> hierarchy;
	//CvMemStorage * contours = cvCreateMemStorage(0);
	// 1. ��������
	cv::findContours(gray, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
	//CvScalar color = CvScalar(0, 0, 255);
	int k = 0;
	double sumHeight = 0;
	double sumAngle = 0;
	double sumArea = 0;
	int maxId;
	//cv::Mat height_width;
	// 2. ɸѡ��Щ���С��
	for (int i = 0; i <contours.size(); i++) {
		std::vector<cv::Point> cnt, approx;
		cnt = contours[i];
		cv::RotatedRect  rect;
		// �ҵ���С�ľ��Σ��þ��ο����з���
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
		// ����ߺͿ�
		height = abs(box[0].y - box[2].y);
		width = abs(box[0].x - box[2].x);

		//height_width.at<int>(i,0) = height;
		//height_width.at<int>(i, 1) = width;
		//sumHeight = sumHeight + height;
		// ��������������
		int area;
		area = cv::contourArea(cnt);
		// ���С�Ķ�ɸѡ��
		if (area < 60 * height) {
			continue;
		}
		if (area <5000) {
			continue;
		}
		// ɸѡ��Щ̫ϸ�ľ��Σ����±��
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
**���ܣ�
**
***************************************/

std::vector< std::vector<cv::Point> >  HelloThread::cutPicRegion(cv::Mat gray) {
	std::vector< std::vector<cv::Point> > contours, region, regionmin;
	//CvPoint2D32f *region;
	std::vector<cv::Vec4i> hierarchy;
	//CvMemStorage * contours = cvCreateMemStorage(0);
	// 1. ��������
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
	// 2. ɸѡ��Щ���С��
	for (int i = 0; i <contours.size(); i++) {
		std::vector<cv::Point> cnt, approx;
		cnt = contours[i];
		//printf("cnt==%d", cnt[0].x);
		//printf("cnt==%d", cnt[0].y);

		// �������ƣ����ú�С
		//double  epsilon;
		//epsilon = 0.001 * cv::arcLength(cnt, true);
		//cv::approxPolyDP(cnt, approx, epsilon, true);
		cv::RotatedRect  rect;
		// �ҵ���С�ľ��Σ��þ��ο����з���
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
		// ����ߺͿ�
		height = abs(box[0].y - box[2].y);
		width = abs(box[0].x - box[2].x);

		//height_width.at<int>(i,0) = height;
		//height_width.at<int>(i, 1) = width;
		//sumHeight = sumHeight + height;
		// ��������������
		int area;
		area = cv::contourArea(cnt);
		// ���С�Ķ�ɸѡ��
		if (area < 60 * height) {
			continue;
		}
		if (area <10000) {
			continue;
		}
		// ɸѡ��Щ̫ϸ�ľ��Σ����±��
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
**���ܣ�ȥ��ͼƬ�е���Ӱ����
**
***************************************/
cv::Mat HelloThread::removeShadow(cv::Mat img)
{
	cv::Mat img_filter;
	///////////////////////��ʼ
	
	//cv::Mat element = cv::getStructuringElement(0,
		//cvSize(2 * 1 + 1, 2 * 1 + 1),
		//cvPoint(-1, -1));
	//cv::erode(img, img, element);
	//cv::bilateralFilter(img_filter, img, 11, 11, 11);
	cv::bilateralFilter(img, img_filter, 10, 7, 10);//9->11��10,9,11��->10, 7, 10
	cv::GaussianBlur(img_filter, img, cv::Size(5, 5), 2);
	cv::adaptiveThreshold(img, img_filter, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY_INV, 7, 2);
	//��ֵ�����ٽ���˫���˲�����ָ�˲�����ֵ�˲�����ֳ�ȥ��㡣
	cv::bilateralFilter(img_filter, img, 10, 9, 11);//(10, 9, 11)
	cv::medianBlur(img, img, 3);
	
	cv::blur(img, img, cvSize(3, 3), cvPoint(-1, -1));
	//cv::medianBlur(img, img, 3);
	//cv::blur(img, img, cvSize(3, 3), cvPoint(-1, -1));
	return img;

}
/***************************************
**cv::Mat HelloThread::mser_img(cv::Mat img)
**���ܣ�����ԭͼ��������ת�Ƕȵ��㷨
**����ʵ������0~90�ȣ�������90�ȣ��ĽǶȾ�����
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
**���ܣ��ӳ�����Ʋ�����mypath��ֵ���ӳ�����ͼƬ����Ԥ����
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
	//����Browse��ť���ɵ��
	ui.pushButton_browse->setEnabled(false);
	//����start��ť���ɵ��
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
	qDebug() << tr("����ʶ����starttess");
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
	//Browse��ť�ָ��ɵ��״̬
	ui.pushButton_browse->setEnabled(true);
	//�ָ�start��ť�ɵ��״̬
	ui.pushButton_start->setEnabled(true);
	mythread->terminate();
	mythread->wait();
	qDebug() << tr("Stop processing the image\r\n");
	//display_time();
	

}
void WindowsForFun::display_text()
{

	//QDateTime time = QDateTime::currentDateTime();//��ȡϵͳ���ڵ�ʱ��
	//QString str = time.toString("yyyy-MM-dd hh:mm:ss \r\n"); //������ʾ��ʽ
	strLog += tr("------------------------------------\r\n");
	//strLog += str;
	//    label->setText(str);//�ڱ�ǩ����ʾʱ��
	ui.plainTextEdit->setPlainText(strLog);
	QTextCursor cursor = ui.plainTextEdit->textCursor();
	cursor.movePosition(QTextCursor::End);
	ui.plainTextEdit->setTextCursor(cursor);
	//save_output_log();
}
void WindowsForFun::save_output_log()
{
	//�����ļ�����,output_log.txt Ϊ�ļ���
	QFile fileOut("output_log.txt");
	//��
	if( ! fileOut.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
	{
	QMessageBox::warning(this, tr("Open a file"),tr("Failed to open the destination file��") + fileOut.errorString());
	return;
	}
	fileOut.write( strLog.toLocal8Bit() );
	fileOut.close();
	qDebug() << tr("д���ļ��ɹ�");

}
void WindowsForFun::apendtext(QString text) {

	//Browse��ť�ָ��ɵ��״̬
	ui.pushButton_browse->setEnabled(true);
	//�ָ�start��ť�ɵ��״̬
	ui.pushButton_start->setEnabled(true);
	/*
	for (int k = 0; k < text.size(); k++) {
		qDebug() << k;
		if (text[k]== '00\ufb01') {
			qDebug() << tr("����ʱ");

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