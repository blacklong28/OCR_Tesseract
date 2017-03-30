// This is a demo introduces you to reading a video and camera 
#include <iostream>
#include <string>
#include <sstream>
#include <windows.h>
using namespace std;

// OpenCV includes
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp> // for camera
using namespace cv;

// OpenCV command line parser functions
// Keys accepted by command line parser
const char* keys =
{
	"{help h usage ? | | print this message}"
	"{@video | | Video file, if not defined try to use webcamera}"
};
#define TWO_PI 6.2831853071795864769252866
//盐噪声  
void salt(cv::Mat image, int n) {

	int i, j;
	for (int k = 0; k<n / 2; k++) {

		// rand() is the random number generator  
		i = std::rand() % image.cols; // % 整除取余数运算符,rand=1022,cols=1000,rand%cols=22  
		j = std::rand() % image.rows;

		if (image.type() == CV_8UC1) { // gray-level image  

			image.at<uchar>(j, i) = 255; //at方法需要指定Mat变量返回值类型,如uchar等  

		}
		else if (image.type() == CV_8UC3) { // color image  

			image.at<cv::Vec3b>(j, i)[0] = 0; //cv::Vec3b为opencv定义的一个3个值的向量类型  
			image.at<cv::Vec3b>(j, i)[1] = 0; //[]指定通道，B:0，G:1，R:2  
			image.at<cv::Vec3b>(j, i)[2] = 0;
		}
	}
}
//椒噪声  
void pepper(cv::Mat image, int n) {

	int i, j;
	for (int k = 0; k<n; k++) {

		// rand() is the random number generator  
		i = std::rand() % image.cols; // % 整除取余数运算符,rand=1022,cols=1000,rand%cols=22  
		j = std::rand() % image.rows;

		if (image.type() == CV_8UC1) { // gray-level image  

			image.at<uchar>(j, i) = 0; //at方法需要指定Mat变量返回值类型,如uchar等  

		}
		else if (image.type() == CV_8UC3) { // color image  

			image.at<cv::Vec3b>(j, i)[0] = 0; //cv::Vec3b为opencv定义的一个3个值的向量类型  
			image.at<cv::Vec3b>(j, i)[1] = 0; //[]指定通道，B:0，G:1，R:2  
			image.at<cv::Vec3b>(j, i)[2] = 0;
		}
	}
}
double generateGaussianNoise()
{
	static bool hasSpare = false;
	static double rand1, rand2;

	if (hasSpare)
	{
		hasSpare = false;
		return sqrt(rand1) * sin(rand2);
	}

	hasSpare = true;

	rand1 = rand() / ((double)RAND_MAX);
	if (rand1 < 1e-100) rand1 = 1e-100;
	rand1 = -2 * log(rand1);
	rand2 = (rand() / ((double)RAND_MAX)) * TWO_PI;

	return sqrt(rand1) * cos(rand2);
}


void AddGaussianNoise(Mat& I)
{
	// accept only char type matrices
	CV_Assert(I.depth() != sizeof(uchar));

	int channels = I.channels();

	int nRows = I.rows;
	int nCols = I.cols * channels;

	if (I.isContinuous()) {
		nCols *= nRows;
		nRows = 1;
	}

	int i, j;
	uchar* p;
	for (i = 0; i < nRows; ++i) {
		p = I.ptr<uchar>(i);
		for (j = 0; j < nCols; ++j) {
			double val = p[j] + generateGaussianNoise() * 128;
			if (val < 0)
				val = 0;
			if (val > 255)
				val = 255;

			p[j] = (uchar)val;

		}
	}

}


int main(int argc, char** argv)
{
	
	CommandLineParser parser(argc, argv, keys);
	parser.about("Reading a video and camera v1.0.0");

	// If requires help show
	if (parser.has("help"))
	{
		parser.printMessage();
		return 0;
	}
	String videoFile = parser.get<String>(0);

	// Check if params are correctly parsed in his variables
	if (!parser.check())
	{
		parser.printErrors();
		return 0;
	}

	VideoCapture cap;

	if (videoFile != "")
	{
		cap.open(videoFile);// read a video file
	}
	else {
		cap.open(0);// read the default caera
	}
	if (!cap.isOpened())// check if we succeeded
	{
		return -1;
	}
	cap.set(CV_CAP_PROP_FRAME_HEIGHT, 1944);//1536/1944
	cap.set(CV_CAP_PROP_FRAME_WIDTH, 2592);//2592/2048
	//namedWindow("Video", 1);
	int k = 0;
	while (1)
	{
		Mat frame;
		cap >> frame; // get a new frame from camera
		//imshow("Video", frame);
		//waitKey(5000);
		Sleep(2000);
		//_sleep(5 * 1000);
		stringstream ss;
		ss << k;
		string s1 = ss.str();
		cout << s1 << endl;
		String name = s1 + ".jpg";
		imwrite(name, frame);
		k++;
		if (waitKey(30) >= 0) break;
	}

	// Release the camera or video file
	cap.release();
	
	//if (argc != 2)
	//{
	//	cout << " Usage: display_image ImageToLoadAndDisplay" << endl;
	//	return -1;
	//}
	/*
	Mat image;
	image = imread("t1.tif", 1); // Read the file

	if (!image.data) // Check for invalid input
	{
		cout << "Could not open or find the image" << std::endl;
		return -1;
	}

	namedWindow("Display window", WINDOW_AUTOSIZE); // Create a window for display.
	imshow("Display window", image); // Show our image inside it.

									 // Add Gaussian noise here
	//AddGaussianNoise(image);
	pepper(image,10000);
	namedWindow("Noisy image", WINDOW_AUTOSIZE); // Create a window for display.
	imshow("Noisy image", image); // Show our image inside it.
	waitKey(0); // Wait for a keystroke in the window
	imwrite("chi_sim.kaiti.exp0.tif",image);
	*/
	return 0;
}