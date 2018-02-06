#include"iostream"
#include "Main.h"
#include "pthread.h"
#include <cstdlib>
#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<stdlib.h>
#include <Windows.h>

using namespace std;
using namespace cv;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

struct thread_args
{
	Mat extracted_image;

};
String numberplate = "";
String Tempnumberplate = "";
struct thread_args threadimage;
string type = ".png";
string path = "numplates/";
stringstream ss;

bool compareContourAreas(vector<Point> contour1, vector<Point> contour2)
{
	double i = fabs(contourArea(Mat(contour1)));
	double j = fabs(contourArea(Mat(contour2)));
	return (i < j);
}

void *ReadImage(void *threadid) {
	long tid;
	tid = (long)threadid;
	int lck;
	lck = pthread_mutex_lock(&mtx);

	Mat imgOriginalScene = imread("NP.jpg");

	if (imgOriginalScene.empty()) {
		cout << "Image not found." << endl;
		lck = pthread_mutex_unlock(&mtx);
		pthread_exit(NULL);
	}

	vector<PossiblePlate> vectorOfPossiblePlates = detectPlatesInScene(imgOriginalScene);

	vectorOfPossiblePlates = detectCharsInPlates(vectorOfPossiblePlates);


	if (vectorOfPossiblePlates.empty()) {
	}
	else {



		sort(vectorOfPossiblePlates.begin(), vectorOfPossiblePlates.end(), PossiblePlate::sortDescendingByNumberOfChars);

		PossiblePlate licPlate = vectorOfPossiblePlates.front();
		PossiblePlate licPlate2 = vectorOfPossiblePlates.back();


		if (licPlate.strChars.length() == 0) {
			Sleep(1000);
			lck = pthread_mutex_unlock(&mtx);
			pthread_exit(NULL);
		}


		if (licPlate.strChars[0] >= 'A' && licPlate.strChars[0] <= 'Z')
		{
			if (licPlate.strChars == licPlate2.strChars) {
				Sleep(1000);
				lck = pthread_mutex_unlock(&mtx);
				pthread_exit(NULL);
			}
			else {
				numberplate = licPlate.strChars + " " + licPlate2.strChars;
			}
		}

		else
		{
			if (licPlate.strChars == licPlate2.strChars)
			{
				Sleep(1000);
				lck = pthread_mutex_unlock(&mtx);
				pthread_exit(NULL);
			}
			else
			{
				numberplate = licPlate2.strChars + " " + licPlate.strChars;
			}
		}

		if (numberplate != Tempnumberplate) {

			Tempnumberplate = numberplate;
			cout << "Number Plate = " << numberplate << endl;
			cout << "************************************" << endl;
			ss << path << numberplate << type;
			string filename = ss.str();
			ss.str("");
			imwrite(filename, imgOriginalScene);

			Sleep(5000);
			lck = pthread_mutex_unlock(&mtx);
			pthread_exit(NULL);
		}
		else
		{
			Sleep(5000);
			lck = pthread_mutex_unlock(&mtx);
			pthread_exit(NULL);
		}
	}

	Sleep(1000);
	lck = pthread_mutex_unlock(&mtx);
	pthread_exit(NULL);
	return 0;
}


int main(void) {

	bool blnKNNTrainingSuccessful = loadKNNDataAndTrainKNN();
	if (blnKNNTrainingSuccessful == false) {
		cout << "Training failed." << endl;
		return(0);
	}

	pthread_t thread;
	int r = 0, c = 0;
	Mat imgOriginalScene;
	VideoCapture capture(0);
	int q;

	while (cvWaitKey(30) != 'q')
	{
		capture >> imgOriginalScene;
		if (true)
		{



			Mat frame;

			Mat gray;
			cvtColor(imgOriginalScene, gray, CV_BGR2GRAY);

			Mat clear;
			bilateralFilter(gray, clear, 9, 75, 75);

			Mat hist;
			equalizeHist(clear, hist);

			Mat morph;


			for (int i = 1; i < 10; i++)
			{

				morphologyEx(hist, morph, MORPH_OPEN, getStructuringElement(MORPH_RECT, Size(5, 5)), Point(-1, -1), i);
			}



			Mat morph_image = hist - morph;

			Mat thresh;
			threshold(morph_image, thresh, 0, 255, THRESH_OTSU);

			Mat edge;
			Canny(thresh, edge, 250, 255, 3);

			Mat dil;
			dilate(edge, dil, getStructuringElement(MORPH_CROSS, Size(3, 3)));


			vector<vector<Point>> contours;
			vector<Vec4i> hierarchy;


			findContours(dil, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));
			sort(contours.begin(), contours.end(), compareContourAreas);

			vector<Point> approx;
			vector<Point> screenCnt;

			for (size_t i = 0; i < contours.size(); i++) {
				approxPolyDP((Mat(contours[i])), approx, arcLength(Mat(contours[i]), true) * 0.06, true);
				double a = contourArea(contours[i], false);


				if (a < 30000 && a>8000 && approx.size() == 4) {
					screenCnt = approx;

					drawContours(imgOriginalScene, vector<vector<Point>>{screenCnt}, 0, Scalar(0, 255, 255), 3);

					Mat mask = Mat::zeros(imgOriginalScene.rows, imgOriginalScene.cols, CV_8UC1);
					r = imgOriginalScene.rows;
					c = imgOriginalScene.cols;

					Rect re = boundingRect(contours[i]);

					drawContours(mask, vector<vector<Point>>{screenCnt}, -1, Scalar(255), CV_FILLED);

					Mat crop(imgOriginalScene.rows, imgOriginalScene.cols, CV_8UC3);

					crop.setTo(Scalar(255, 255, 255));

					imgOriginalScene.copyTo(crop, mask);

					normalize(mask.clone(), mask, 0.0, 255.0, CV_MINMAX, CV_8UC3);

					Rect cr = Rect(re.x, re.y, re.width, re.height);
					Mat cut = crop(cr);

					cvtColor(cut, cut, CV_BGR2GRAY);
					imwrite("NP.jpg", cut);

					threadimage.extracted_image = cut;

					int rc = pthread_create(&thread, NULL, ReadImage, (void *)1);


					if (rc) {
						cout << "Thread creation failed." << rc << endl;
						exit(-1);
					}

					break;

				}
			}


			imshow("Video Stream", imgOriginalScene);


		}
	}
	pthread_exit(NULL);
}

