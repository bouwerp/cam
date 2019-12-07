//
// Created by Pieter Bouwer on 2019-04-30.
//

#include <functional>
#include <cstdio>
#include <cstdbool>
#include <cstdlib>
#include <cctype>
#include <endian.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sysexits.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CAM_H
#define CAM_H

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_component.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/mmal_parameters_camera.h"
#include "interface/vmcs_host/vc_dispmanx.h"
#include "interface/vmcs_host/vc_vchi_gencmd.h"
#include "interface/vmcs_host/vc_vchi_bufman.h"
#include "interface/vmcs_host/vc_tvservice.h"
#include "interface/vmcs_host/vc_cecservice.h"
#include "interface/vchiq_arm/vchiq_if.h"

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Video format information
// 0 implies variable
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

// Max bitrate we allow for recording
#define MAX_BITRATE_MJPEG 25000000 // 25Mbits/s
#define MAX_BITRATE_LEVEL4 25000000 // 25Mbits/s
#define MAX_BITRATE_LEVEL42 62500000 // 62.5Mbits/s

/// Interval at which we check for an failure abort during capture
#define ABORT_INTERVAL 100 // ms

#define zoom_full_16P16 ((unsigned int)(65536 * 0.15))
#define zoom_increment_16P16 (65536UL / 10)

// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1

/// Amount of time before first image taken to allow settling of
/// exposure etc. in milliseconds.
#define CAMERA_SETTLE_TIME       1000

/// Layer that preview window should be displayed on
#define PREVIEW_LAYER      2

// Frames rates of 0 implies variable, but denominator needs to be 1 to prevent div by 0
#define PREVIEW_FRAME_RATE_NUM 0
#define PREVIEW_FRAME_RATE_DEN 1

#define FULL_RES_PREVIEW_FRAME_RATE_NUM 0
#define FULL_RES_PREVIEW_FRAME_RATE_DEN 1

typedef enum {
    ZOOM_IN, ZOOM_OUT, ZOOM_RESET
} ZOOM_COMMAND_T;

typedef struct CAM_STATE_S CAM_STATE;

// There isn't actually a MMAL structure for the following, so make one
typedef struct mmal_param_colourfx_s {
    int enable;       /// Turn colourFX on or off
    int u, v;          /// U and V to use
} MMAL_PARAM_COLOURFX_T;

typedef struct {
    char camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN]; // Name of the camera sensor
    uint32_t width;                          /// Requested width of image
    uint32_t height;                         /// requested height of image
    char *filename;                     /// filename of output file
    int cameraNum;                      /// Camera number
    int sensor_mode;                    /// Sensor mode. 0=auto. Check docs/forum for modes selected by other values.
    int verbose;                        /// !0 if want detailed run information
    int gps;                            /// Add real-time gpsd output to output

} CAM_COMMONSETTINGS_PARAMETERS;

typedef struct param_float_rect_s {
    double x;
    double y;
    double w;
    double h;
} PARAM_FLOAT_RECT_T;

typedef struct mmal_param_thumbnail_config_s {
    int enable;
    int width, height;
    int quality;
} MMAL_PARAM_THUMBNAIL_CONFIG_T;

typedef std::function<void(int64_t timestamp, uint8_t *data, uint32_t length, uint32_t offset)> VideoCallback;
typedef std::function<void(uint8_t *data, uint32_t length)> StillCallback;

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct {
    VideoCallback video_cb;
    StillCallback still_cb;
    FILE *file_handle;                   /// File handle to write buffer data to.
    CAM_STATE *pstate;              /// pointer to our state in case required in callback
    VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
    int abort;                           /// Set to 1 in callback if an error occurs to attempt to abort the capture
#define IFRAME_BUFSIZE (60*1000)
    int iframe_buff[IFRAME_BUFSIZE];          /// buffer of iframe pointers
    int iframe_buff_wpos;
    int iframe_buff_rpos;
    char header_bytes[29];
    int header_wptr;
    int flush_buffers;
    // temporary image data
    uint8_t *image_data;
    long image_data_length;
} PORT_USERDATA;

/// Frame advance method
enum {
    FRAME_NEXT_SINGLE,
    FRAME_NEXT_TIMELAPSE,
    FRAME_NEXT_KEYPRESS,
    FRAME_NEXT_FOREVER,
    FRAME_NEXT_GPIO,
    FRAME_NEXT_SIGNAL,
    FRAME_NEXT_IMMEDIATELY
};

typedef struct cam_parameters_s {
    int sharpness;             /// -100 to 100
    int contrast;              /// -100 to 100
    int brightness;            ///  0 to 100
    int saturation;            ///  -100 to 100
    int ISO;                   ///  TODO : what range?
    int videoStabilisation;    /// 0 or 1 (false or true)
    int exposureCompensation;  /// -10 to +10 ?
    MMAL_PARAM_EXPOSUREMODE_T exposureMode;
    MMAL_PARAM_EXPOSUREMETERINGMODE_T exposureMeterMode;
    MMAL_PARAM_AWBMODE_T awbMode;
    MMAL_PARAM_IMAGEFX_T imageEffect;
    MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imageEffectsParameters;
    MMAL_PARAM_COLOURFX_T colourEffects;
    MMAL_PARAM_FLICKERAVOID_T flickerAvoidMode;
    int rotation;              /// 0-359
    int hflip;                 /// 0 or 1
    int vflip;                 /// 0 or 1
    PARAM_FLOAT_RECT_T roi;   /// region of interest to use on the sensor. Normalised [0,1] values in the rect
    int shutter_speed;         /// 0 = auto, otherwise the shutter speed in ms
    float awb_gains_r;         /// AWB red gain
    float awb_gains_b;         /// AWB blue gain
    MMAL_PARAMETER_DRC_STRENGTH_T drc_level;  // Strength of Dynamic Range compression to apply
    MMAL_BOOL_T stats_pass;    /// Stills capture statistics pass on/off
    char annotate_string[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2]; /// String to use for annotate - overrides certain bitmask settings

    MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo_mode;
    float analog_gain;         // Analog gain
    float digital_gain;        // Digital gain

    int settings;
} CAM_PARAMETERS;

typedef struct {
    int wantPreview;                       /// Display a preview
    int wantFullScreenPreview;             /// 0 is use previewRect, non-zero to use full screen
    int opacity;                           /// Opacity of window - 0 = transparent, 255 = opaque
    MMAL_RECT_T previewWindow;             /// Destination rectangle for the preview window.
    MMAL_COMPONENT_T *preview_component;   /// Pointer to the created preview display component
    MMAL_PORT_T *camera_preview_port;
    MMAL_PORT_T *camera_preview_input_port;
} CAM_PREVIEW_PARAMETERS;

/** Structure containing all state information for the current run
 */
struct CAM_STATE_S {
    CAM_COMMONSETTINGS_PARAMETERS common_settings{};     /// Common settings
    int timeout{};                        /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
    MMAL_FOURCC_T encoding{};             /// Requested codec video encoding (MJPEG or H264)
    int bitrate{};                        /// Requested bitrate
    uint32_t framerate{};                      /// Requested frame rate (fps)
    uint32_t intraperiod{};                    /// Intra-refresh period (key frame rate)
    uint32_t quantisationParameter{};          /// Quantisation parameter - quality. Set bitrate 0 and set this for variable bitrate
    int bInlineHeaders{};                  /// Insert inline headers to stream (SPS, PPS)
    int immutableInput{};                 /// Flag to specify whether encoder works in place or creates a new buffer. Result is preview can display either
    /// the camera output or the encoder output (with compression artifacts)
    uint32_t profile{};                        /// H264 profile to use for encoding
    uint32_t level{};                          /// H264 level to use for encoding
    int waitMethod{};                     /// Method for switching between pause and capture

    int onTime{};                         /// In timed cycle mode, the amount of time the capture is on per cycle
    int offTime{};                        /// In timed cycle mode, the amount of time the capture is off per cycle

    int segmentSize{};                    /// Segment mode In timed cycle mode, the amount of time the capture is off per cycle
    int segmentWrap{};                    /// Point at which to wrap segment counter
    int segmentNumber{};                  /// Current segment counter
    int splitNow{};                       /// Split at next possible i-frame if set to 1.
    int splitWait{};                      /// Switch if user wants splited files

    CAM_PREVIEW_PARAMETERS preview_parameters{}; /// Camera setup parameters
    CAM_PARAMETERS camera_parameters{}; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component{};    /// Pointer to the camera component
    MMAL_COMPONENT_T *video_encoder_component{};   /// Pointer to the encoder component
    MMAL_CONNECTION_T *video_encoder_connection{}; /// Pointer to the connection from camera to encoder
    MMAL_CONNECTION_T *preview_connection{}; /// Pointer to the connection from camera to preview

    MMAL_PORT_T *camera_video_port{};
    MMAL_PORT_T *video_encoder_input_port{};
    MMAL_PORT_T *video_encoder_output_port{};

    MMAL_POOL_T *video_encoder_pool{}; /// Pointer to the pool of buffers used by encoder output port

    PORT_USERDATA callback_data;        /// Used to move data to the encoder callback

    int bCapturing{};                     /// State of capture/pause

    int inlineMotionVectors{};             /// Encoder outputs inline Motion Vectors
    int intra_refresh_type{};              /// What intra refresh type to use. -1 to not set.
    int frame{};
    int64_t starttime{};
    int64_t lasttime{};

    MMAL_BOOL_T addSPSTiming{};
    int slices{};

    //still
    MMAL_COMPONENT_T *still_encoder_component{};   /// Pointer to the encoder component

    MMAL_PORT_T *camera_still_port{};
    MMAL_PORT_T *still_encoder_input_port{};
    MMAL_PORT_T *still_encoder_output_port{};

    int quality{};                        /// JPEG quality setting (1-100)
    int wantRAW{};                        /// Flag for whether the JPEG metadata also contains the RAW bayer image
    MMAL_PARAM_THUMBNAIL_CONFIG_T thumbnailConfig{};
    int timelapse{};                      /// Delay between each picture in timelapse mode. If 0, disable timelapse
    int fullResPreview{};                 /// If set, the camera preview port runs at capture resolution. Reduces fps.
    int frameNextMethod{};                /// Which method to use to advance to next frame
    int burstCaptureMode{};               /// Enable burst mode
    int timestamp{};                      /// Use timestamp instead of frame#
    int restart_interval{};               /// JPEG restart interval. 0 for none.

    MMAL_COMPONENT_T *encoder_component{};   /// Pointer to the encoder component
    MMAL_CONNECTION_T *encoder_connection{}; /// Pointer to the connection from camera to encoder

    MMAL_POOL_T *encoder_pool{}; /// Pointer to the pool of buffers used by encoder output port
};

/// Capture/Pause switch method
/// Simply capture for time specified
enum {
    WAIT_METHOD_NONE,       /// Simply capture for time specified
    WAIT_METHOD_TIMED,      /// Cycle between capture and pause for times specified
    WAIT_METHOD_KEYPRESS,   /// Switch between capture and pause on keypress
    WAIT_METHOD_SIGNAL,     /// Switch between capture and pause on signal
    WAIT_METHOD_FOREVER     /// Run/record forever
};

static VCHI_INSTANCE_T global_initialise_instance;
static VCHI_CONNECTION_T *global_connection;

void check_camera_model(int cam_num);

void get_sensor_defaults(int camera_num, char *camera_name, uint32_t *width, uint32_t *height);

void destroy_encoder_component(CAM_STATE *state);

MMAL_STATUS_T create_encoder_component(CAM_STATE *state);

void commonsettings_set_defaults(CAM_COMMONSETTINGS_PARAMETERS *state);

void camcontrol_set_defaults(CAM_PARAMETERS *params);

void destroy_camera_component(CAM_STATE *state);

int set_exposure_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMODE_T mode);

int set_flicker_avoid_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_FLICKERAVOID_T mode);

int set_awb_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_AWBMODE_T awb_mode);

int set_awb_gains(MMAL_COMPONENT_T *camera, float r_gain, float b_gain);

int set_rotation(MMAL_COMPONENT_T *camera, int rotation);

int set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip);

int set_ROI(MMAL_COMPONENT_T *camera, PARAM_FLOAT_RECT_T rect);

int set_shutter_speed(MMAL_COMPONENT_T *camera, int speed);

int set_DRC(MMAL_COMPONENT_T *camera, MMAL_PARAMETER_DRC_STRENGTH_T strength);

int set_stats_pass(MMAL_COMPONENT_T *camera, int stats_pass);

int set_gains(MMAL_COMPONENT_T *camera, float analog, float digital);

int set_all_parameters(MMAL_COMPONENT_T *camera, const CAM_PARAMETERS *params);

MMAL_STATUS_T create_camera_component(CAM_STATE *state);

int set_stereo_mode(MMAL_PORT_T *port, MMAL_PARAMETER_STEREOSCOPIC_MODE_T *stereo_mode);

void default_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

int set_saturation(MMAL_COMPONENT_T *camera, int saturation);

int set_sharpness(MMAL_COMPONENT_T *camera, int sharpness);

int set_contrast(MMAL_COMPONENT_T *camera, int contrast);

int set_brightness(MMAL_COMPONENT_T *camera, int brightness);

int set_ISO(MMAL_COMPONENT_T *camera, int ISO);

int set_metering_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T m_mode);

int set_video_stabilisation(MMAL_COMPONENT_T *camera, int vstabilisation);

int set_exposure_compensation(MMAL_COMPONENT_T *camera, int exp_comp);

MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection);

void check_disable_port(MMAL_PORT_T *port);

uint64_t get_microseconds64();

int wait_for_next_change(CAM_STATE *state);

void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

void bcm_host_init(void);

void default_state(CAM_STATE *state);

MMAL_STATUS_T init(CAM_STATE *state);

MMAL_STATUS_T capture(CAM_STATE *state);

void destroy(CAM_STATE *state);

//still
MMAL_STATUS_T create_still_camera_component(CAM_STATE *state);

MMAL_STATUS_T init_still(CAM_STATE *state);

int default_still_state(CAM_STATE *state);

MMAL_STATUS_T create_still_encoder_component(CAM_STATE *state);

MMAL_STATUS_T capture_still(CAM_STATE *state, StillCallback);

int wait_for_next_frame(CAM_STATE *state, int *frame);

void still_encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

void preview_set_defaults(CAM_PREVIEW_PARAMETERS *state);

MMAL_STATUS_T preview_create(CAM_PREVIEW_PARAMETERS *state);

void preview_destroy(CAM_PREVIEW_PARAMETERS *state);

void destroy_still(CAM_STATE *);

#endif //CAM_H

#ifdef __cplusplus
}
#endif