# cam

Camera library, based on Raspicam (which utilises Broadcom's MMAL libraries), for use in Raspberry Pi C/C++ projects.

The code was directly copied from the Raspicam source code, and stripped down and cleaned up to what was necessary at the time I needed it. 
It has also been ported to C++ (17).

# Building dependencies

## Raspberry Pi _userland_

Clone the repository and change to the repository directory:

```bash
git clone https://github.com/raspberrypi/userland.git
```

Add a CMake toolchain file, for example (`armv6-rpi-linux-gnueabi-toolchain.cmake`):

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_SYSROOT <CROSS_TOOL_BASE_PATH>/armv6-rpi-linux-gnueabi/armv6-rpi-linux-gnueabi/sysroot)

set(CMAKE_C_COMPILER "<CROSS_TOOL_BASE_PATH>/armv6-rpi-linux-gnueabi/bin/armv6-rpi-linux-gnueabi-gcc")
set(CMAKE_CXX_COMPILER "<CROSS_TOOL_BASE_PATH>/armv6-rpi-linux-gnueabi/bin/armv6-rpi-linux-gnueabi-g++")
```

The cross tools in this instance were generated with _crosstool-ng_.

Configure CMake and build:
```bash
cmake --clean -DCMAKE_TOOLCHAIN_FILE=armv6-rpi-linux-gnueabi-toolchain.cmake
...
make -j 16
```

# Building

This library can be built in the same way as the _userland_ library.

Configure CMake and build:
```bash
cmake --clean -DCMAKE_TOOLCHAIN_FILE=armv6-rpi-linux-gnueabi-toolchain.cmake
..
make -j 16
```

# Example usage

Include paths for CMake:
```
<SOURCE_CODE_BASE_PATH>/cam/include
<SOURCE_CODE_BASE_PATH>/userland
```

The libraries that are needed for loading in the CMake build definition:
```
cam
mmal
mmal_util
mmal_core
mmal_components
mmal_vc_client
vcos
bcm_host
vchiq_arm
pthread
```

Using the camera as a video camera:
```cpp
CAM_STATE state{};
uint width;
uint height;
uint rotation;
std::map<std::string, VideoCallback> cbs;

default_state(&mState);

state.waitMethod = WAIT_METHOD_FOREVER;
state.timeout = 1000;
state.encoding = MMAL_ENCODING_MJPEG;
state.inlineMotionVectors = 0;
state.callback_data.pstate = &state;
state.callback_data.abort = 0;
state.quality = 50;
state.common_settings.width = 1024;
state.common_settings.height = 768;
state.camera_parameters.rotation = 90;
state.framerate = 3;

auto cb = [&](int64_t timestamp, uint8_t *data, uint32_t length, uint32_t offset) {
    ... // handle video frame data
};
state.callback_data.video_cb = cb;

MMAL_STATUS_T status;
if ((status = init(&state)) != MMAL_SUCCESS) {
    cerr << "could not initialise video camera: " << mmal_status_to_string(status) << endl;
    return;
}

// blocking capture (practically this should spawned in a thread)
if ((status = capture(&state)) != MMAL_SUCCESS) {
    cerr << "error capturing video: " << mmal_status_to_string(status) << endl;
    return;
}
```

