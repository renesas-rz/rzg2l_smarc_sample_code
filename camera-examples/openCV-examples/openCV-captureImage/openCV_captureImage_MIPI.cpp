#include <iostream>
#include <string>
#include <errno.h>
#include <stdexcept>

#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/imgproc/types_c.h>
#include <opencv4/opencv2/imgproc.hpp>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/videoio.hpp>

using namespace std;
using namespace cv;

int runCommand(string command, string &stdoutput)
{
	size_t charsRead;
	int status;
	char buffer[512];

	FILE *output = popen(command.c_str(), "r");

	if (output == NULL)
		throw invalid_argument("I cannot execute command: " + command);

	stdoutput = "";
	while (true) {
		charsRead = fread(buffer, sizeof(char), sizeof(buffer), output);

		if (charsRead > 0)
			stdoutput += string(buffer, charsRead);

		if (ferror(output)) {
			pclose(output);
			throw invalid_argument("Troubles while getting the output from command");
		}

		if (feof(output))
			break;
	}

	status = pclose(output);
	if (status == -1)
		throw invalid_argument("Command " + command + " termination " +
			"went wrong ([" + to_string(errno) + "] " +
			string(strerror(errno)) + ")");

	return WEXITSTATUS(status);
}

string cameraInitialization = "media-ctl -d /dev/media0 --reset && media-ctl -d /dev/media0 -l \"'rzg2l_csi2 10830400.csi2':1->'CRU output':0 [1]\" && media-ctl -d /dev/media0 -V \"'rzg2l_csi2 10830400.csi2':1 [fmt:UYVY8_2X8/1280x960 field:none]\" && media-ctl -d /dev/media0 -V \"'ov5645 0-003c':0 [fmt:UYVY8_2X8/1280x960 field:none]\"";

int main(int argc, char **argv)
{
	if (argc < 2) {
		cout << "Please give me the device ID" << endl;
		return 1;
	}
	string stdoutput;

	// We need to run this command only once
	if (runCommand(cameraInitialization, stdoutput)) {
		cout << "Cannot initialize the camera" << endl;
		return 1;
	}

	// We are passing the ID of the camera to use from the command line,
	// e.g. 0
	VideoCapture camera(atoi(argv[1]));
	if (!camera.isOpened()) {
		cout << "Cannot open the camera" << endl;
		return 1;
	}

	camera.set(CAP_PROP_FRAME_WIDTH, 1280);
	camera.set(CAP_PROP_FRAME_HEIGHT, 960);
	camera.set(CAP_PROP_FOURCC, VideoWriter::fourcc('U', 'Y', 'V', 'Y'));

	Mat picture;
    //skip first several frames to stabilize camera
	for (int i = 0; i <3 ; i++)
		camera >> picture;
    camera >> picture;

	camera.release();

	imwrite("picture.jpg", picture);

	return 0;
}
