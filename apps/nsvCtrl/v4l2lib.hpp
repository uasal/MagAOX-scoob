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

#include <unordered_map>
#include <string>
#include <sstream>
#include <regex>

/*
struct CameraControl {
    std::string name;
    std::string data_type;
    int64_t min_value = 0;
    int64_t max_value = 0;
    int64_t step = 0;
    int64_t default_value = 0;
    int64_t current_value = 0;
    std::vector<int> dims;
    std::string flags;
};
*/

#define PARAM_NOT_FOUND -999

struct CameraControl {
    __u32 id;
    __u32 type;
    std::string name;
    int64_t minimum;
    int64_t maximum;
    uint64_t step;
    int64_t default_value;
    int64_t current_value;
    __u32 flags;                
    __u32 elem_size;
    __u32 elems;
    std::vector<__u32> dims; // Dimensions array
};

/*
 Camera modes come back in the form: 
   Format: 16-bit Bayer RGRG/GBGB (RG16)
    Bit Depth: 16 bits
    Resolutions:
      9576x6388 (current)
        Intervals: 0.25s (4 fps) 
      8096x6324
        Intervals: 0.0075188s (133 fps)

    Multiple modes exist on the camera, and each mode has a list of resolutions and intervals
*/
struct CameraMode {
    std::string description;
    std::string pixelFormat;
    std::vector<std::pair<__u32, __u32>> resolutions; // Pair of width, height
    std::vector<double> intervals;   // Nested vector for intervals per resolution
    int bitDepth;
    int current_resolution;                 // Index of the current resolution
};

// Note, after adding CameraMode to track resolution changes, width/height, 
//   and bitDepth are redundant. Same with pixelFormat and framerate
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

int setCamImageFormat(int width, int height, int bitDepth);

int requestBuffers(int requested_buf_count);
int queryBuffers();
int queryBuffer(int buf_index);
int queueBuffers();
int queueBuffer(int buf_index);

int getCamInput();

void setupCamera();

// returns the index of the buffer dequeued or -1 for failure
int dequeueBuffer();   
int startStreaming(); 
int stopStreaming();
void waitForFrame();

void getBufferDetails();

void getCameraModes();
void updateCurrentMode();

void getAllCameraControls(); //experimental parsing of camera params
void updateCameraControls();

int getAndUpdateSingleControlVal(std::string search_str);
int writeSingleControlVal(std::string search_str, int value);

std::string parseFlags();
std::string pixelFormatToString();
int getBitDepthFromPixelFormat();
std::string sanitizeName(const char* name);

// Data structure to store camera controls
std::unordered_map<std::string, CameraControl> camera_controls;

std::vector<CameraMode> camera_modes;

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
    setupCamera(); 
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

void setupCamera() {
    getCameraModes();
    getAllCameraControls();
}

int setCamImageFormat(int width, int height, int bitDepth) {

    v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fmt.fmt.pix.width = width;   
    fmt.fmt.pix.height = height;

    /* could support other pixel formats such as V4L2_PIX_FMT_SRGGB8, V4L2_PIX_FMT_SBGGR8, V4L2_PIX_FMT_SGBRG8, V4L2_PIX_FMT_SGRBG8
                                                ,V4L2_PIX_FMT_SRGGB10, V4L2_PIX_FMT_SBGGR10, V4L2_PIX_FMT_SGBRG10, V4L2_PIX_FMT_SGRBG10
                                                ,V4L2_PIX_FMT_SBGGR12, V4L2_PIX_FMT_SGBRG12, V4L2_PIX_FMT_SGRBG12, V4L2_PIX_FMT_SRGGB12
                                                ,V4L2_PIX_FMT_SBGGR14, V4L2_PIX_FMT_SGBRG14, V4L2_PIX_FMT_SGRBG14, V4L2_PIX_FMT_SRGGB14
                                                ,V4L2_PIX_FMT_SRGGB16, V4L2_PIX_FMT_SBGGR16, V4L2_PIX_FMT_SGBRG16, V4L2_PIX_FMT_SGRBG16

                                        only supporting SRGGB for now (unclear whether camera firmware will every be any other format)
    */
    switch (bitDepth) {
        case 8:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB8;  // 8-bit Bayer format
            std::cout << "Setting pixel format to 8-bit SRGGB." << std::endl;
            break;
        case 10:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10;  // 10-bit Bayer format
            std::cout << "Setting pixel format to 10-bit SRGGB." << std::endl;
            break;
        case 12:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB12;  // 10-bit Bayer format
            std::cout << "Setting pixel format to 12-bit SRGGB." << std::endl;
            break;
        case 14:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB14;  // 10-bit Bayer format
            std::cout << "Setting pixel format to 14-bit SRGGB." << std::endl;
            break;
        case 16:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB16;  // 16-bit Bayer format
            std::cout << "Setting pixel format to 16-bit SRGGB." << std::endl;
            break;
        default:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB16;  // 16-bit Bayer format
            std::cerr << "Invalid bit depth! Supported bit depths are 8, 10, and 16" << std::endl;
            break;
    }

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        std::cout << "Error setting pixel format" << std::endl;
        return -1;
    }

    // read back values after attempting to set. Driver may change width/height 
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
        printf("Requesting buffers from camera failed... cnt req: %d\n", requested_buf_count);
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
        printf("Failed to start streaming\n");
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
    send_i2c_cmd(0, 1);                 // currently turning all on and off. TODO verify cam_input corresponds with right power channel
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

CameraParams getCameraParams(){
    return params;
}

//                  Begin experimental v4l2-ctl parsing             //
// TODO add some checking for read-only. write-only before attempting to set values & provide better return info for user if something fails
// these are just readable string flags for now
std::string parseFlags(__u32 flags) {
    std::string flag_str;
    if (flags & V4L2_CTRL_FLAG_DISABLED) flag_str += "disabled, ";
    if (flags & V4L2_CTRL_FLAG_READ_ONLY) flag_str += "read-only, ";
    if (flags & V4L2_CTRL_FLAG_WRITE_ONLY) flag_str += "write-only, ";
    if (flags & V4L2_CTRL_FLAG_VOLATILE) flag_str += "volatile, ";
    if (flags & V4L2_CTRL_FLAG_EXECUTE_ON_WRITE) flag_str += "execute-on-write, ";
    if (flag_str.size() > 2) flag_str.pop_back(), flag_str.pop_back(); // Remove trailing ", "
    return flag_str;
}

// Helper function to convert pixel format to a readable string
std::string pixelFormatToString(__u32 pixelformat) {
    char fourcc[5];
    memcpy(fourcc, &pixelformat, 4);
    fourcc[4] = '\0';
    return std::string(fourcc);
}

// for some reason the object that comes back from the v4l2 driver has spaces and capital letters in it
std::string sanitizeName(const char* input) {
    std::string sanitized;
    sanitized.reserve(32); // Reserve space for up to 32 characters

    for (size_t i = 0; i < 32 && input[i] != '\0'; ++i) {
        char c = input[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            sanitized += '_'; // Replace spaces with underscores
        } else if (c == '.') {
            continue; // Skip periods
        } else {
            sanitized += std::tolower(static_cast<unsigned char>(c)); // Convert to lowercase
        }
    }

    return sanitized;
}

// Helper function to determine bit depth based on pixel format
int getBitDepthFromPixelFormat(__u32 pixelformat) {
    switch (pixelformat) {
        case V4L2_PIX_FMT_SRGGB8:
        case V4L2_PIX_FMT_SBGGR8:
        case V4L2_PIX_FMT_SGBRG8:
        case V4L2_PIX_FMT_SGRBG8:
            return 8;

        case V4L2_PIX_FMT_SRGGB10:
        case V4L2_PIX_FMT_SBGGR10:
        case V4L2_PIX_FMT_SGBRG10:
        case V4L2_PIX_FMT_SGRBG10:
            return 10;
        
        case V4L2_PIX_FMT_SBGGR12:
        case V4L2_PIX_FMT_SGBRG12:
        case V4L2_PIX_FMT_SGRBG12:
        case V4L2_PIX_FMT_SRGGB12:
            return 12;

        case V4L2_PIX_FMT_SBGGR14:
        case V4L2_PIX_FMT_SGBRG14:
        case V4L2_PIX_FMT_SGRBG14:
        case V4L2_PIX_FMT_SRGGB14:
            return 14;

        case V4L2_PIX_FMT_SRGGB16:
        case V4L2_PIX_FMT_SBGGR16:
        case V4L2_PIX_FMT_SGBRG16:
        case V4L2_PIX_FMT_SGRBG16:
            return 16;

        default:
            return -1;
    }
}

// works on global 'mode' assuming has already been created. Need to already have opened the camera and gotten modes
// only call this on mode switch 
// some data structure redundancy right now with CameraParams struct & requires cleanup
void updateCurrentMode() {

    if(camera_modes.empty()){
        getCameraModes();
        return;
    }

    // NOTE: ONLY DOING FIRST MODE FOR NOW. 
    // DRIVER DOES NOT SHOW A DIFFERENCE BETWEEN MODES FOR NOW BUT MAY CHANGE IN THE FUTURE
    CameraMode mode = camera_modes[0];

    struct v4l2_format current_fmt;
    memset(&current_fmt, 0, sizeof(current_fmt));
    current_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_FMT, &current_fmt) < 0) {
        printf("Failed to get current format\n");
        return;
    }

    if(mode.resolutions.size() < 1){
        printf("Camera modes not initialized\n");
        return;
    }

    // Determine the active resolution
    for (size_t i = 0; i < mode.resolutions.size(); ++i) {
        if (mode.resolutions[i].first == current_fmt.fmt.pix.width && mode.resolutions[i].second == current_fmt.fmt.pix.height) {
            mode.current_resolution = static_cast<int>(i);
            break;
        }
    }

    printf("Curent resolution: %d x %d\n", mode.resolutions[mode.current_resolution].first, mode.resolutions[mode.current_resolution].second);
    printf("Current interval: %f\n", mode.intervals[mode.current_resolution]);
}

// get bayer pattern, bit depth, and viable resolutions w/ corresponding framerates)
void getCameraModes() {

    camera_modes.clear();  //remove existing modes... 
    // TODO probably only need to pull the mode config once we connect to the camera
    //      and then only pull the 'current' mode when we do a height-width switch

    struct v4l2_format current_fmt;
    memset(&current_fmt, 0, sizeof(current_fmt));
    current_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_FMT, &current_fmt) < 0) {
        printf("Failed to get current format\n");
        return;
    }

    __u32 current_width = current_fmt.fmt.pix.width;
    __u32 current_height = current_fmt.fmt.pix.height;

    // Enumerate only first mode.  No way for us to tell which mode we are doing or how to switch between them anyway. [0] and [1] are identical
    struct v4l2_fmtdesc fmt_desc;
    memset(&fmt_desc, 0, sizeof(fmt_desc));
    fmt_desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt_desc.index = 0;

    if (ioctl(fd, VIDIOC_ENUM_FMT, &fmt_desc) != 0) {
        printf("Failed to enumerate formats\n");
        return;
    }

    // Create the camera mode
    CameraMode mode;
    mode.description = reinterpret_cast<char*>(fmt_desc.description);
    mode.pixelFormat = pixelFormatToString(fmt_desc.pixelformat);  //pixelFormat has some spaces, capitals, etc and is not the 'V4L2_PIX_FMT_SRGGB12' for ex
    mode.bitDepth = getBitDepthFromPixelFormat(fmt_desc.pixelformat);

    // Enumerate resolutions for this mode
    struct v4l2_frmsizeenum frmsize;
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.pixel_format = fmt_desc.pixelformat;

    while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            // Add discrete resolution
            mode.resolutions.emplace_back(frmsize.discrete.width, frmsize.discrete.height);

            // Enumerate frame intervals for this resolution
            struct v4l2_frmivalenum frmival;
            memset(&frmival, 0, sizeof(frmival));
            frmival.pixel_format = fmt_desc.pixelformat;
            frmival.width = frmsize.discrete.width;
            frmival.height = frmsize.discrete.height;

            while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
                if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                    double interval = static_cast<double>(frmival.discrete.numerator) /
                                      static_cast<double>(frmival.discrete.denominator);
                    mode.intervals.push_back(interval);
                }
                frmival.index++;
            }
        }
        frmsize.index++;
    }

    // Determine the active resolution
    for (size_t i = 0; i < mode.resolutions.size(); ++i) {
        if (mode.resolutions[i].first == current_width && mode.resolutions[i].second == current_height) {
            mode.current_resolution = static_cast<int>(i);
            break;
        }
    }

    camera_modes.push_back(mode);  // add mode to global object

    // Print the mode and its resolutions
    std::cout << "Format: " << mode.description << " (" << mode.pixelFormat << ")\n";
    std::cout << "  Bit Depth: " << mode.bitDepth << " bits\n";
    std::cout << "  Resolutions:\n";

    for (size_t i = 0; i < mode.resolutions.size(); ++i) {
        const auto& res = mode.resolutions[i];
        std::cout << "    " << res.first << "x" << res.second;
        if (mode.current_resolution == static_cast<int>(i)) {
            std::cout << " (current)";
        }
        std::cout << "\n";
        std::cout << "    " << mode.intervals[i];
        if (mode.current_resolution == static_cast<int>(i)) {
            std::cout << " (current)";
        }
        std::cout << "\n";
    }
}

// update current camera control values. Always query after changing modes
// On initial connection to camera, call getAllCameraControls(), otherwise call this 
void updateCameraControls() {
    if(camera_controls.empty()) {
        getAllCameraControls();
        return;
    }

    struct v4l2_query_ext_ctrl query_ext_ctrl;

    memset(&query_ext_ctrl, 0, sizeof(query_ext_ctrl));
    query_ext_ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    while (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query_ext_ctrl) == 0) {
        if (query_ext_ctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
            query_ext_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            continue;
        }

        // find the corresponding element in camera_controls & update values
        auto it = camera_controls.find(sanitizeName(query_ext_ctrl.name));
        if(it != camera_controls.end()){
            // assuming dimensions, names, id, flag, size don't change 
            it->second.minimum = query_ext_ctrl.minimum;
            it->second.maximum = query_ext_ctrl.maximum;
            it->second.default_value = query_ext_ctrl.default_value;
            it->second.step = query_ext_ctrl.step;

            // Retrieve current value of control (VIDIOC_G_CTRL) with id from query_ext_ctrl.id
            struct v4l2_ext_control control;
            memset(&control, 0, sizeof(control));
            control.id = query_ext_ctrl.id;
            if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &control) == 0) {  //VIDIOC_G_EXT_CTRLS VIDIOC_G_CTRL VIDIOC_QUERYCTRL
                it->second.current_value = control.value;
            } else {
                printf("Couldn't get current value of %s\n", sanitizeName(query_ext_ctrl.name).c_str());
            }
        
        } else {
            printf("Couldn't find %s\n", sanitizeName(query_ext_ctrl.name).c_str());
        }

        // Move to the next control
        query_ext_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    if (errno != EINVAL) {
        printf("Error enumerating controls\n");
        return;
    }

}

// update current value based on string input
/*
    Searches camera parameter list for value, 
    then retreives the latest value from the camera, updating the data structure

    @Return value of the parameter or -1 for failure
*/
int getAndUpdateSingleControlVal(std::string search_str){
    auto it = camera_controls.find(search_str);
    if(it != camera_controls.end()){
        struct v4l2_ext_control control;
        memset(&control, 0, sizeof(control));
        control.id = it->second.id;
        if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &control) == 0) {
            it->second.current_value = control.value;
            return control.value;
        } else {
            printf("Error getting current value of %s\n", search_str.c_str());
            return -1;
        }
    }
    printf("Couldn't find camera parameter %s\n", search_str.c_str());
    return PARAM_NOT_FOUND; // inform when parameter doesn't exist
}

/* VIDIOC_S_CTRL returns 0 or -1 and writes to errno variable
   errno could be EINVAL, ERANGE, EBUSY, EACCESS 
   
   when writing values, sometimes driver will set a different val than input. 
   For accuracy, recommend doing 'getAndUpdateSingleControlVal after writing 
   
    interestingly enough, v4l2 only deals in ints for input values
*/
int writeSingleControlVal(std::string search_str, int value) {
    auto it = camera_controls.find(search_str);
    if (it != camera_controls.end()) {
        struct v4l2_ext_control control;
        memset(&control, 0, sizeof(control));
        control.id = it->second.id;
        control.value = value;
        it->second.current_value = value;  //best guess, but won't be correct much of the time

        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &control) == 0) {
            printf("Control '%s' updated to value %d\n", search_str.c_str(), control.value);
            return control.value;
        } else {
            printf("Failed to set control value %s\n", search_str.c_str());
        }
    }
        
    printf("Control '%s' not found\n", search_str.c_str());
    return PARAM_NOT_FOUND; // inform when parameter doesn't exist

    // below min return, above max return? 
}

// removes and rebuilds list of all camera controls and values 
void getAllCameraControls() {

    camera_controls.clear();

    struct v4l2_query_ext_ctrl query_ext_ctrl;

    memset(&query_ext_ctrl, 0, sizeof(query_ext_ctrl));
    query_ext_ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    while (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query_ext_ctrl) == 0) {
        if (query_ext_ctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
            query_ext_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            continue;
        }

        // Create new CameraControl object per loop & add to unordered map
        CameraControl cam_control;
        cam_control.id = query_ext_ctrl.id;
        cam_control.type = query_ext_ctrl.type;
        cam_control.name = sanitizeName(query_ext_ctrl.name); //names have spaces and .
        cam_control.minimum = query_ext_ctrl.minimum;
        cam_control.maximum = query_ext_ctrl.maximum;
        cam_control.step = query_ext_ctrl.step;
        cam_control.default_value = query_ext_ctrl.default_value;
        cam_control.flags = query_ext_ctrl.flags;
        cam_control.elem_size = query_ext_ctrl.elem_size;
        cam_control.elems = query_ext_ctrl.elems;

        // Add dimensions
        for (size_t i = 0; i < query_ext_ctrl.nr_of_dims; ++i) {
            cam_control.dims.push_back(query_ext_ctrl.dims[i]);
        }
        // Retrieve current value of control (VIDIOC_G_CTRL) with id from query_ext_ctrl.id
        struct v4l2_control control;
        memset(&control, 0, sizeof(control));
        control.id = query_ext_ctrl.id;
        if (ioctl(fd, VIDIOC_G_CTRL, &control) == 0) {
            cam_control.current_value = control.value;
        } else {
            cam_control.current_value = -1; // Indicate error or unsupported control
        }

        // Add to map
        camera_controls[cam_control.name] = cam_control;

        // Move to the next control
        query_ext_ctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    if (errno != EINVAL) {
        printf("Error enumerating controls\n");
        return;
    }

    if(camera_controls.empty()) {
        printf("Failed to get camera controls\n");
        return;
    }

    for (const auto& [name, control] : camera_controls) {
        std::cout << "" << name << "\n";
        //std::cout << "  ID: " << control.id << "\n";
        //std::cout << "  Type: " << control.type << "\n";
        //std::cout << "  Min: " << control.minimum << "\n";
        //std::cout << "  Max: " << control.maximum << "\n";
        //std::cout << "  Step: " << control.step << "\n";
        //std::cout << "  Default: " << control.default_value << "\n";
        //std::cout << "  Current: " << control.current_value << "\n";
        //std::cout << "  Flags: " << parseFlags(control.flags) << "\n";
        //std::cout << "  Elem Size: " << control.elem_size << "\n";
        //std::cout << "  Elems: " << control.elems << "\n";
        //if (!control.dims.empty()) {
        //    std::cout << "  Dims: ";
        //    for (const auto& dim : control.dims) {
        //        std::cout << "[" << dim << "]";
        //    }
        //    std::cout << "\n";
        //}
        //std::cout << "--------------------------------------\n";
    }

}


#endif


