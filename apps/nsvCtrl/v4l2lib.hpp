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
#include <cstdlib> 


struct CameraParams {
    unsigned int width;
    unsigned int height;
    unsigned int bitDepth;
    float exposure;
    float frameRate;
    std::string pixelFormat;   
};

CameraParams getCameraParams();

int openCamera(const char* device);
int closeCamera();

int initCamera(int width, int height, int bitDepth);
int requestBuffers(int requested_buf_count);
int queryBuffers();
int queryBuffer(int buf_index);
int queueBuffers();
int queueBuffer(int buf_index);

int getPowerStatus();
int getCamInput();

// returns the index of the buffer dequeued or -1 for failure
int dequeueBuffer();   
int startStreaming(); 
int stopStreaming();
void waitForFrame();

void getBufferDetails();

CameraParams params;

bool stream_on;
bool power_on;
int fps;
float exposure;

uint cam_input;

int fd;
std::vector<void*> buffers;

int currentBufIndex;
int bufferCount; // length of queue
int bufferSize;  // how many bytes per buffer index
int camIndex;


int openCamera(const char* device) {
    fd = open(device, O_RDWR);
    if (fd < 0) {
        //throw std::runtime_error("Unable to open camera device");
        return -1;
    }
    getCamInput();
    return 0;
}

int closeCamera() {
    close(fd);
    return 0;
}

int getCamInput() {
    struct v4l2_input input;
    //unsigned int input_count = 0;
    
    /*
    // Enumerate all inputs and print their names and indices
    while (ioctl(fd, VIDIOC_ENUMINPUT, &input) == 0) {
        printf("Input %d: %s\n", input.index, input.name);
        input_count++;
        input.index++;
    }
    */

    // Get the current input index   v4l2-ctl --list-inputs
    if (ioctl(fd, VIDIOC_G_INPUT, &input) == -1) {
        close(fd);
    }

    cam_input = input.index;
    printf("Current active input: %d\n", input.index);
    return 0;
}

int getPowerStatus() {
    struct v4l2_input input;

    //update struct to current camera
    input.index = cam_input;

    if (ioctl(fd, VIDIOC_ENUMINPUT, &input) == 0) {
        printf("Input %d: %s, %d, %d\n", input.index, input.name, input.type, input.status);
    }

    if(input.index != cam_input)
    {
        printf("Wrong camera index\n");
    }

    return input.status; // status 0 is camera ok, all others are failures
}

CameraParams getCameraParams() {
    return params;
}

int initCamera(int width, int height, int bitDepth) {

    v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fmt.fmt.pix.width = width;   
    fmt.fmt.pix.height = height;

    switch (bitDepth) {
        case 8:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB8;  // 8-bit Bayer format
            std::cout << "Setting pixel format to 8-bit SRGGB." << std::endl;
            break;
        case 10:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10;  // 10-bit Bayer format
            std::cout << "Setting pixel format to 10-bit SRGGB." << std::endl;
            break;
        case 16:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB16;  // 16-bit Bayer format
            std::cout << "Setting pixel format to 16-bit SRGGB." << std::endl;
            break;
        default:
            std::cerr << "Invalid bit depth! Supported bit depths are 8, 10, and 16" << std::endl;
            return-1;
    }

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        //throw std::runtime_error("Failed to set pixel format");
        return -1;
    }

    params.width = fmt.fmt.pix.width;
    params.height = fmt.fmt.pix.height;
    params.pixelFormat = fmt.fmt.pix.pixelformat; 
    params.bitDepth = bitDepth;
    return 0;
}

int requestBuffers(int requested_buf_count) {
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = requested_buf_count; 
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        //throw std::runtime_error("Failed to request buffers");
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
        //throw std::runtime_error("Failed to query buffer");
        return -1;
    }

    void* buffer = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer == MAP_FAILED) {
        //throw std::runtime_error("Failed to map buffer");
        return -1;
    }

    buffers[buf.index] = buffer;
    bufferSize = buf.length; 

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
		    //throw std::runtime_error("failed to queue buffer");
            return -1;
	    }
        return 0;
}

int dequeueBuffer(int buf_index){

    struct v4l2_buffer bufdq = {};
	bufdq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufdq.memory = V4L2_MEMORY_MMAP;
	bufdq.index = buf_index; 

	if (ioctl(fd, VIDIOC_DQBUF, &bufdq) < 0) {
		//throw std::runtime_error("Failed to dequeue buffer");
        return -1;
	}
    currentBufIndex = bufdq.index;  //returns index of image dequeued (FIFO)
    return currentBufIndex;
}

int startStreaming() {
    
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        //throw std::runtime_error("Failed to start streaming");
        return -1;
    }
    else {
        stream_on = true;
        printf("Started camera stream\n");
        return 0;
    }
}

int stopStreaming() {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        //throw std::runtime_error("Failed to stop streaming");
        printf("Error while stopping camera stream\n");
        return -1;
    }
    else { 
        stream_on = false;
        printf("Stopped camera stream\n");
        return 0;
    }
}

void waitForFrame() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0,0};
    tv.tv_sec = 0.05;
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

/* power mappings from Neutralino's documentation */
const uint8_t cmd_on = 0x90;
const uint8_t cmd_off = 0x80;
const uint8_t bus[6] = {0x08, 0x08, 0x02, 0x02, 0x01, 0x01};
const uint16_t addr[6] = {0x02bf, 0x02b0, 0x02bf, 0x02b0, 0x02bf, 0x02b0};

void send_i2c_cmd(int channel, uint8_t action) {
    uint8_t addr1 = addr[channel] >> 8;  // Higher byte of address
    uint8_t addr2 = addr[channel] & 0xff;  // Lower byte of address
    uint8_t cmd = (action == 1) ? cmd_on : cmd_off;

    std::string command = "i2ctransfer -f -y " + std::to_string(bus[channel]) +
                          " w3@0x48 " + std::to_string(addr1) + " " + std::to_string(addr2) + " " + std::to_string(cmd);
    std::cout << command << std::endl;
    int ret = system(command.c_str());
    if (ret != 0) {
        printf("Error: Failed to send I2C command.\n");
    }
}

void turn_on_power() {
    //send_i2c_cmd(cam_input, 1); 
    send_i2c_cmd(0, 1); 
    send_i2c_cmd(1, 1); 
    send_i2c_cmd(2, 1); 
    send_i2c_cmd(3, 1); 
    send_i2c_cmd(4, 1); 
    send_i2c_cmd(5, 1); 
    std::cout << "Power turned on for channel " << 0 << std::endl;
}

void turn_off_power() {
    //send_i2c_cmd(cam_input, 0); 
    send_i2c_cmd(0, 0); 
    send_i2c_cmd(1, 0); 
    send_i2c_cmd(2, 0); 
    send_i2c_cmd(3, 0); 
    send_i2c_cmd(4, 0); 
    send_i2c_cmd(5, 0); 
    std::cout << "Power turned off for channel " << 0 << std::endl;
}

#endif


