/** \file v4l2lib.hpp
  * \brief The MagAO-X video for linux wrapper header file
  *
  * \ingroup nsvCtrl_files
  */

#ifndef v4l2lib_hpp
#define v4l2lib_hpp

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <ctime>
#include <chrono>


struct CameraParams {
    unsigned int width;
    unsigned int height;
    float exposure;
    float frameRate;
    std::string pixelFormat;    // bit depth?
};

CameraParams getCameraParams();

int initCamera(int width, int height);
int requestBuffers(int requested_buf_count);
int queryBuffers();
int queryBuffer(int buf_index);
int queueBuffers();
int queueBuffer(int buf_index);
int dequeueBuffer();    //returns the index of the buffer dequeued or -1 for failure
int startStreaming(); 
int stopStreaming();
void waitForFrame();

void getBufferDetails();

CameraParams params;

bool stream_on;
bool power_on;
int fps;
float exposure;

int fd;
std::vector<void*> buffers;

int currentBufIndex;
int bufferCount; // length of queue
int bufferSize;  // how many bytes per buffer index
int camIndex;



    // settable params
    /*
    int setGain(float gain);
    int setBlacklevel(float blacklevel);
    int setFrameRate(int framerate);
    int setExposure(float exposure);
    int setROI(int xbin, int ybin, int xstart, int xend, int ystart, int yend);
    int setMode(int modeNum); //0,1,2?
    
    int restartCamStream(); // have to restart cam stream during any change of parameters. 
                            // will be different buffer size when ROI changes, so have to re-create bufs & update bufferSize

    int getLatestFrame();  

    */
/*
Camera::Camera(int cam) {
    camIndex = cam;
}

Camera::~Camera() {
    for (void* buffer : buffers) {
        munmap(buffer, bufferSize);
    }
    close(fd);
}
*/

int openCamera(const char* device) {
    fd = open(device, O_RDWR);
    if (fd < 0) {
        throw std::runtime_error("Unable to open camera device");
        return -1;
    }
    return 0;
}

CameraParams getCameraParams() {
    return params;
}

int initCamera(int width, int height) {

    v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fmt.fmt.pix.width = width;   
    fmt.fmt.pix.height = height;

    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB16;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        throw std::runtime_error("Failed to set pixel format");
        return -1;
    }

    params.width = fmt.fmt.pix.width;
    params.height = fmt.fmt.pix.height;
    params.pixelFormat = "RG16"; 
    return 0;
}

int requestBuffers(int requested_buf_count) {
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = requested_buf_count; 
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        throw std::runtime_error("Failed to request buffers");
        return -1;
    }

    bufferCount = req.count; 
    buffers.resize(bufferCount);
    return 0;
}

int queryBuffers() {   

    for(int i=0; i<bufferCount; i++)
    {
        queryBuffer(i);
    }
    if(bufferCount > 0)
    {
        return 0;
    }
    return -1;
}

int queryBuffer(int buf_index){

    v4l2_buffer buf;
    std::memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = buf_index;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        throw std::runtime_error("Failed to query buffer");
        return -1;
    }

    void* buffer = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer == MAP_FAILED) {
        throw std::runtime_error("Failed to map buffer");
        return -1;
    }

    buffers[buf.index] = buffer;
    bufferSize = buf.length; //assume all the same
    return 0;
}

int queueBuffers() {
    
    for(int i=0; i<bufferCount; i++){
        if(queueBuffer(i) == -1){
            return -1;
        }
        //buffers[buf.index].bytesused = buf.bytesused;
    }
    return 0;
}  

int queueBuffer(int buf_index){

        v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = buf_index;

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
    		throw std::runtime_error("Failed to queue buffer");
            return -1;
	    }
        //printf("Queueuing %d\n", buf.index);
        return 0;
}

int dequeueBuffer(){

    struct v4l2_buffer bufdq = {0};
	bufdq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufdq.memory = V4L2_MEMORY_MMAP;
	bufdq.index = 0;

	if (ioctl(fd, VIDIOC_DQBUF, &bufdq) < 0) {
		throw std::runtime_error("Failed to dequeue");
        return -1;
	}
    currentBufIndex = bufdq.index;  //returns the index of the image dequeued (FIFO)
    //printf("Dequeueing %d\n", currentBufIndex);
    return currentBufIndex;
}

// track queueing and dequeueing images & their indicies. 
//Return the pointer to the image that was dequeued most recently
//int getLatestImage(){
//}

int startStreaming() {
    
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        throw std::runtime_error("Failed to start streaming");
        return -1;
    }
    else {
        stream_on = true;
        return 0;
    }
}

int stopStreaming() {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        throw std::runtime_error("Failed to stop streaming");
        return -1;
    }
    else { 
        stream_on = false;
        return 0;
    }
}

void waitForFrame() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 2;
    int r = select(fd+1, &fds, NULL, NULL, &tv);
    if(-1 == r){
        perror("Waiting for Frame");
        exit(1);
    }
}

void getBufferDetails() {
    printf("Number of buffers: %d\n", bufferCount);
    printf("Size of buffer: %d\n", bufferSize);
}
/*
int main() {
    try {
        Camera camera(0);
        camera.openCamera("dev/video0"); 

        camera.initCamera(6144, 4210);

        CameraParams params = camera.getCameraParams();
        std::cout << "Camera params - Width: " << params.width
                  << ", Height: " << params.height
                  << ", Pixel Format: " << params.pixelFormat << std::endl;


        camera.requestBuffers(2);
        camera.queryBuffers();
        camera.getBufferDetails();
        camera.startStreaming();

        //camera.queueBuffers(); // do this before streaming?
        camera.queueBuffer(0); //start at 0 index. all other buffers are empty

        auto start = std::chrono::steady_clock::now();
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        for(int j=0; j<3; j++)
        {
            for(int i=0; i<2; i++)
            {
                int bufIndex;
                camera.waitForFrame();
                start = std::chrono::steady_clock::now();
                bufIndex = camera.dequeueBuffer();

                end = std::chrono::steady_clock::now();
                elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                std::cout << "  Dequeueing: " << elapsed.count() << " microseconds." << std::endl;
                //camera.writeFile(bufIndex);

                if(bufIndex == 1){
                    camera.queueBuffer(0); //wrap around
                }
                else{
                    camera.queueBuffer(bufIndex + 1); // queue next frame 
                }
            }

            //start = std::chrono::steady_clock::now();
            //camera.queueBuffers();
            //end = std::chrono::steady_clock::now();
            //elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            //std::cout << "  Qequeueing: " << elapsed.count() << " microseconds." << std::endl;
        }

        camera.stopStreaming();
        std::cout << "Streaming stopped." << std::endl;

    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
*/

#endif


