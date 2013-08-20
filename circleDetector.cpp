#include "stdafx.h"

using namespace std;
using namespace cv;

class CLEyeCameraCapture {
	CHAR _windowName[256];
	GUID _cameraGUID;
	CLEyeCameraInstance _cam;
	CLEyeCameraColorMode _mode;
	CLEyeCameraResolution _resolution;
	float _fps;
	HANDLE _hThread;
	bool _running;
	CvPoint2D32f *corners;
	int corner_count;


public:
	CLEyeCameraCapture(LPSTR windowName, GUID cameraGUID, CLEyeCameraColorMode mode, CLEyeCameraResolution resolution, float fps )
		:_cameraGUID(cameraGUID), _cam(NULL), _mode(mode), _resolution(resolution), _fps(fps), _running(false) {
			strcpy( _windowName, windowName );
	}

	bool StartCapture() {
		_running = true;
		cvNamedWindow( _windowName, CV_WINDOW_AUTOSIZE );

		// Start CLEye image capture thread
		_hThread = CreateThread(NULL, 0, &CLEyeCameraCapture::CaptureThread, this, 0, 0 );
		if ( _hThread == NULL ) {
			MessageBoxA( NULL, "Could not create capture thread", "CLEyeMulticamText", MB_ICONEXCLAMATION );
			return false;
		}

		return true;
	}

	void StopCapture() {
		if ( !_running ) {
			return;
		}
		_running = false;
		WaitForSingleObject( _hThread, 1000 );	// /_\/
		cvDestroyWindow( _windowName );
		cvDestroyWindow( "origin");
	}

	void Run() {
		int w, h;
		IplImage *pCapImage;
		PBYTE pCapBuffer = NULL;

		// Create camera instance
		_cam = CLEyeCreateCamera( _cameraGUID, _mode, _resolution, _fps );
		if ( _cam == NULL ) {	// Can not create
			return;
		}

		// Get camera frame dimensions
		CLEyeCameraGetFrameDimensions( _cam, w, h );

		// Depending on color mode chosen, create the appropriate OpenCV image.
		if ( _mode == CLEYE_COLOR_PROCESSED || _mode == CLEYE_COLOR_RAW ) {
			pCapImage = cvCreateImage( cvSize(w, h), IPL_DEPTH_8U, 4 );
		} else {
			pCapImage = cvCreateImage( cvSize(w, h), IPL_DEPTH_8U, 1 );
		}

		// Set some camera parameters
		CLEyeSetCameraParameter( _cam, CLEYE_GAIN, 20 );
		CLEyeSetCameraParameter( _cam, CLEYE_EXPOSURE, 511 );
		CLEyeSetCameraParameter( _cam, CLEYE_HFLIP, true);

		// Start capturing
		CLEyeCameraStart( _cam );
		cvGetImageRawData( pCapImage, &pCapBuffer );

		IplImage *dst_img, *src_img;

		//////////////////////
		src_img = cvCreateImage(cvGetSize(pCapImage), IPL_DEPTH_32F, 1);
		dst_img = cvCreateImage(cvGetSize(pCapImage), IPL_DEPTH_8U, 1);
		dst_img->origin = pCapImage->origin;

		IplImage* hsv_img = cvCreateImage(cvGetSize(pCapImage), pCapImage->depth, 1);
		hsv_img->origin = pCapImage->origin;


		CvMemStorage* storage = cvCreateMemStorage(0);
		CvFont font1;
		cvInitFont(&font1,CV_FONT_HERSHEY_PLAIN,1.0f,1.0f);
		char ballp[20];
		char fps[20];
		double frameRate = 0.0;
		clock_t timeStamp = clock();
		clock_t freq;
		int n = 0;

		while( _running ) {

			if(n%10 == 0)
			{
				freq = timeStamp;
				timeStamp = clock();
				freq = timeStamp - freq;
			}

			CLEyeCameraGetFrame( _cam, pCapBuffer );

			//flip image
			cvCvtColor(pCapImage, dst_img, CV_BGR2GRAY);
			//cvCvtColor(pCapImage, hsv_img, CV_RGB2HSV);
			//cvCornerHarris(dst_img, src_img, 3, 0.04);
			cvSmooth(dst_img, dst_img, CV_BLUR, 3, 3, 0);
			//cvSmooth(dst_img, dst_img, CV_BLUR, 3, 3, 0);
			//find circles in image


			CvSeq* circles = cvHoughCircles(dst_img, storage, CV_HOUGH_GRADIENT, 1, 100, 150, 40, 0, 50);



			//draw all circles found
			for(int i = 0; i < circles->total; i++)
			{
				float* p = (float*) cvGetSeqElem(circles, i);
				cvCircle(pCapImage, cvPoint(cvRound(p[0]), cvRound(p[1])), 3, CV_RGB(0, 255, 0), -1, 8, 0);
				cvCircle(pCapImage, cvPoint(cvRound(p[0]), cvRound(p[1])), cvRound(p[2]), CV_RGB(0, 0, 255), 3, 8, 0);
				sprintf(ballp, "x%d y%d", cvRound(p[0]), cvRound(p[1]));
				cvPutText(pCapImage, ballp, cvPoint(cvRound(p[0]), cvRound(p[1]) - cvRound(p[2])), &font1, cvScalar(255, 0, 0));
				cout << "x: " << cvRound(p[0]) << ", y: " << cvRound(p[1]) << endl;
			}

			if(freq != 0)
			{
				frameRate = 10000.0/freq;
				sprintf(fps, "fps %.2f", frameRate);
				cvPutText(pCapImage, fps, cvPoint(5, 15), &font1, cvScalar(0, 0, 255));
			}

			cvShowImage( _windowName, pCapImage);
			cout << timeStamp << "  " << frameRate << endl;
			n++;
		}

		// Stop camera capture.
		CLEyeCameraStop( _cam );

		// Destroy camera object
		CLEyeDestroyCamera( _cam );

		// Destroy the callocated OpenCV image
		cvReleaseImage(&dst_img);
		cvReleaseImage( &src_img);
		cvReleaseImage( &pCapImage );
		_cam = NULL;
	}

	static DWORD WINAPI CaptureThread( LPVOID instance ) {
		// seed the rng with current tick count and thread id
		srand( GetTickCount() + GetCurrentThreadId() );

		// forward thread to Capture function
		CLEyeCameraCapture *pThis = ( CLEyeCameraCapture *) instance;

		pThis->Run();	// toggle the Run function

		return 0;
	}
};


int main( int argc, char** argv ) {

	CLEyeCameraCapture *cam[2] = {NULL};
	srand( GetTickCount() );
	cout << "OK, this works" << endl;
	// Query for number fo connected cameras.
	int numCams = CLEyeGetCameraCount();
	if ( numCams == 0 ) {
		printf( "No PS3Eye cameras detected\n" );
		return -1;
	}
	printf( "Found %d cameras\n", numCams );

	for ( int i = 0; i < numCams; i++ ) {
		char windowName[64];
		GUID guid = CLEyeGetCameraUUID(i);	// Get Camera GUID by index
		printf( "Camera %d GUID: [%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x]\n",
			i+1, guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1], guid.Data4[2],
			guid.Data4[3], guid.Data4[4], guid.Data4[5],
			guid.Data4[6], guid.Data4[7]);
		sprintf( windowName, "Camera Window %d", i+1 );

		cam[i] = new CLEyeCameraCapture( windowName, guid, CLEYE_COLOR_PROCESSED,
			CLEYE_QVGA, 150);	// Create a new camera capture.
		printf( "Starting capture on camera %d\n", i+1);
		cam[i]->StartCapture();	// Start the capture.
	}

	printf( "Just for test!\nUse the following keys to change camera parameters:\n"
		"\t'1' - select camera 1\n"
		"\t'2' - select camera 2\n"
		"\t<ESC> key will exit the program.\n");
	CLEyeCameraCapture *pCam = NULL;	// Set the camera pointer to NULL
	int param = -1, key;

	while( (key = cvWaitKey(0)) != 0x1b ) {	// ESC == 27
		switch ( key ) {	// switch the camera
		case '1':		printf( "Selected camera 1\n");	pCam = cam[0];	break;
		case '2':		printf( "Selected camera 2\n");	pCam = cam[1];	break;
		}
	}

	for ( int i = 0; i < numCams; i++ ) {
		printf( "Stopping capture on camera %d\n", i+1 );
		cam[i]->StopCapture();	// Stop capture.
		delete cam[i];			// delete the cam[i];
	}


	return 0;
}