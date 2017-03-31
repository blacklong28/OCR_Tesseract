#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
using namespace cv;
using namespace std;
bool Debug_show_process = FALSE;

cv::Mat removeShadow(cv::Mat img);
void ocrMain(cv::Mat gray);
cv::Mat detect(cv::Mat img, cv::Mat gray);
cv::Mat preprocess(cv::Mat gray);
std::vector< std::vector<cv::Point> >  cutPicRegion(cv::Mat gray);
std::vector< std::vector<cv::Point> >  findTextRegion(cv::Mat gray);

int main()
{

    cv::Mat gray;
	cv::Mat im = cv::imread("/mnt/large4t/longxuezhu_data/Test/OCR/t4.jpg");
	cv::cvtColor(im, gray, CV_BGR2GRAY);
    
	gray = removeShadow(gray);
	//ȥ��ͼƬ�з����ֲ��֡�
	cv::Mat grayROI = detect(im, gray);
	//��ʼʶ��
	cv::Mat resizeROI;
    cv::resize(grayROI, resizeROI, cv::Size((int)grayROI.cols*1.2, (int)grayROI.rows*1.2), (0, 0), (0, 0), cv::INTER_LINEAR);
	ocrMain(resizeROI);
	//ocrMain(gray);
    

    return 0;
}
/***************************************
**HelloThread::removeShadow(cv::Mat img)
**���ܣ�ȥ��ͼƬ�е���Ӱ����
**
***************************************/
cv::Mat removeShadow(cv::Mat img)
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
void ocrMain(cv::Mat gray) {
	char *outText;
	tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
    // Initialize tesseract-ocr with English, without specifying tessdata path
	//"/mnt/large4t/longxuezhu_data/tesseract/tessdata/"
    if (api->Init("/mnt/large4t/longxuezhu_data/tesseract/tessdata/", "chi_sim+eng", tesseract::OEM_DEFAULT)) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        exit(1);
    }
	api->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
    // Open input image with leptonica library
    //Pix *image = pixRead("/mnt/large4t/longxuezhu_data/Test/OCR/t1.jpg");
	api->SetImage((uchar*)gray.data, gray.cols, gray.rows, 1, gray.cols);
    //api->SetImage(image);
    // Get OCR result
    outText = api->GetUTF8Text();
	char *p = outText;
    while (*p)//�ַ�����������ѭ��
	{

		if (*p >= 'A' && *p <= 'Z') //�жϴ�д��ĸ
		{
			*p += 32; //תСд
		}
		if (*p == '0' && (('a' <= *(p - 1)&& *(p - 1) <= 'z')||('a' <= *(p + 1)&& *(p + 1) <= 'z')) ){
			//qDebug() << myout.at(i - 1);
			*p = 'o';
			//continue;
		}
		/*
		if (*p == 'o' && ('0' <= *(p - 1) <='9')&& ('0' <= *(p + 1) <= '9')) {
			//qDebug() << myout.at(i - 1);
			*p = '0';
			//continue;
		}
		*/
		/*
		if (*p == 'o'&& ('\u4E00' <= *(p - 1) <= '\u9FA5') )
		{
			*p = (char)'\u3002';
			//*(p+1) = (char)'\u3002';
			*(p + 1) = '.';
			//p++;
		}
		
		*/
		p++; //ָ���ָ��׼��������һ����ĸ
	}
    printf("OCR output:\n%s", outText);
    std::ofstream f1("/mnt/large4t/longxuezhu_data/Test/OCR/out.txt");
    if(!f1)return;
    f1<<outText;
    f1.close();
    // Destroy used object and release memory
    api->End();
    delete [] outText;
    //pixDestroy(&image);
}
/***************************************
**HelloThread::detect(cv::Mat gray)
**���ܣ������Ҫʶ�������
**
***************************************/

cv::Mat detect(cv::Mat img, cv::Mat gray)
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
cv::Mat preprocess(cv::Mat gray) {
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
**HelloThread::findTextRegion(cv::Mat gray)
**���ܣ�
**
***************************************/

std::vector< std::vector<cv::Point> >  findTextRegion(cv::Mat gray) {
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

std::vector< std::vector<cv::Point> >  cutPicRegion(cv::Mat gray) {
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
