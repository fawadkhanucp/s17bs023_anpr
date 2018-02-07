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

// Initializing mutex to lock thread during OCR.
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

String numberplate = "";
String Tempnumberplate = "";

// Strings to save detected number plates in a seperate folder.
String type = ".png";
String path = "numplates/";
stringstream ss;

// Function to compare the area of the contours and sort them by size to find number plates.
bool compareContourAreas(vector<Point> contour1, vector<Point> contour2)
{
	double i = fabs(contourArea(Mat(contour1)));
	double j = fabs(contourArea(Mat(contour2)));
	return (i < j);
}

// Function that performs the OCR.
void *ReadImage(void *threadid) {
	long tid;
	tid = (long)threadid;
	int lck;

	// Locks the thread.
	lck = pthread_mutex_lock(&mtx);

	// Reads the number plate detected by the videoStream function.
	Mat imgOriginalScene = imread("NP.jpg");

	// If the image is empty, exit the function.
	if (imgOriginalScene.empty()) {
		lck = pthread_mutex_unlock(&mtx);
		pthread_exit(NULL);
	}

	// Vector to store all the vectors of possible plates in the image which goes into the DetectPlates file.
	vector<PossiblePlate> vectorOfPossiblePlates = detectPlatesInScene(imgOriginalScene);

	// After detecting possible plates, it detects characters in those plates.
	vectorOfPossiblePlates = detectCharsInPlates(vectorOfPossiblePlates);

	// If no possible vectors found, exit the function.
	if (vectorOfPossiblePlates.empty()) {
		Sleep(1000);
		lck = pthread_mutex_unlock(&mtx);
		pthread_exit(NULL);
	}


	else {
		// Sorts the possible plates by descending number of characters.
		sort(vectorOfPossiblePlates.begin(), vectorOfPossiblePlates.end(), PossiblePlate::sortDescendingByNumberOfChars);

		// Possible plate objects to store the detected number plates.
		PossiblePlate licPlate = vectorOfPossiblePlates.front();
		PossiblePlate licPlate2 = vectorOfPossiblePlates.back();

		// If the length of the detected plate is 0, exit the function.
		if (licPlate.strChars.length() == 0) {
			Sleep(1000);
			lck = pthread_mutex_unlock(&mtx);
			pthread_exit(NULL);
		}

		// Conditions to arrange number plate based on letters first and numbers afterwards.
		if (licPlate.strChars[0] >= 'A' && licPlate.strChars[0] <= 'Z')
		{
			if (licPlate.strChars == licPlate2.strChars) {
				Sleep(1000);
				lck = pthread_mutex_unlock(&mtx);
				pthread_exit(NULL);
			}
			else {
				numberplate = licPlate.strChars + licPlate2.strChars;
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
				numberplate = licPlate2.strChars + licPlate.strChars;
			}
		}

		// Condition that checks if the detected number plate is the same as the number plate from previous thread.
		if (numberplate != Tempnumberplate) {
			Tempnumberplate = numberplate;
			cout << "Number Plate = " << numberplate << endl;
			cout << "************************************" << endl;

			// Stores the number plates in a new folder with the detected characters as the name.
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

// Function that streams the video and detects number plates from the stream.
void videoStream() {
	pthread_t thread;
	Mat imgOriginalScene;
	VideoCapture capture(0);

	while (cvWaitKey(30) != 'q')
	{
		// Stores the output from the video stream in a mat.
		capture >> imgOriginalScene;
		if (true)
		{
			// Original image and all the different images created.
			Mat frame;
			Mat gray;
			Mat clear;
			Mat hist;
			Mat morph;
			Mat thresh;
			Mat edge;
			Mat dil;

			//Decleration of vectors for contours.
			vector<vector<Point>> ptContours;
			vector<Vec4i> v4iHierarchy;

			// Image converted to grayscale.
			cvtColor(imgOriginalScene, gray, CV_BGR2GRAY);

			// Applies a bilateral filter which reduces noise and smoothens the image. Similar to using guassian blur.
			bilateralFilter(gray, clear, 9, 75, 75);

			// Equalizes the histogram which stretches the intensity range, improving the contrast.
			equalizeHist(clear, hist);

			for (int i = 1; i < 10; i++)
			{
				// Morphological opening transformation used to remove small objects in an image.
				morphologyEx(hist, morph, MORPH_OPEN, getStructuringElement(MORPH_RECT, Size(5, 5)), Point(-1, -1), i);
			}

			// Subtracting the morph image from the equalized one gives us the equalized image but with a uniform background.
			Mat morph_image = hist - morph;

			// Thresholding to convert the grayscale image to a binary one using the Otsu Method to calculate the optimum threshold.
			threshold(morph_image, thresh, 0, 255, THRESH_OTSU);

			// Canny edge detection technique to detect the edges in the image.
			Canny(thresh, edge, 250, 255, 3);

			// Dilation increases the size of the foreground object.
			dilate(edge, dil, getStructuringElement(MORPH_CROSS, Size(3, 3)));

			// Finds contours in the image to recognize possible number plates.
			findContours(dil, ptContours, v4iHierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));

			// Sorts all contours in the image by their size.
			sort(ptContours.begin(), ptContours.end(), compareContourAreas);

			vector<Point> approx;
			vector<Point> screenCnt;

			// For loop to go through all contours and determine which is the number plate.
			for (size_t i = 0; i < ptContours.size(); i++) {

				// Approximates a polygonal curve which is a connected series of line segments.
				approxPolyDP((Mat(ptContours[i])), approx, arcLength(Mat(ptContours[i]), true) * 0.06, true);

				// Area of contour is calculated and stored in a variable.
				double cArea = contourArea(ptContours[i], false);

				// If the area is within a certain range, it is considered to be our number plate.
				if (cArea < 30000 && cArea>8000 && approx.size() == 4) {

					// Stores the detected contour area in a vector.
					screenCnt = approx;

					// Draws the contours on the screen.
					drawContours(imgOriginalScene, vector<vector<Point>>{screenCnt}, 0, Scalar(0, 255, 255), 3);

					Mat mask = Mat::zeros(imgOriginalScene.rows, imgOriginalScene.cols, CV_8UC1);

					// Creates a rectangle around the contour.
					Rect re = boundingRect(ptContours[i]);

					drawContours(mask, vector<vector<Point>>{screenCnt}, -1, Scalar(255), CV_FILLED);

					Mat crop(imgOriginalScene.rows, imgOriginalScene.cols, CV_8UC3);

					crop.setTo(Scalar(255, 255, 255));

					imgOriginalScene.copyTo(crop, mask);

					normalize(mask.clone(), mask, 0.0, 255.0, CV_MINMAX, CV_8UC3);

					Rect cr = Rect(re.x, re.y, re.width, re.height);
					Mat cut = crop(cr);

					// Saves the detected number plate in an image.
					cvtColor(cut, cut, CV_BGR2GRAY);
					imwrite("NP.jpg", cut);

					// Loads training data for OCR.
					bool blnKNNTrainingSuccessful = loadKNNDataAndTrainKNN();
					if (blnKNNTrainingSuccessful == false) {
						cout << "Training failed." << endl;
					}

					// Calls a thread that performs the OCR.
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

int main(void) {

	videoStream();

}

