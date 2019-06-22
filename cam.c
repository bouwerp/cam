//
// Created by Pieter Bouwer on 2019-05-04.
// Fork of the raspicam/raspivid application.
//

#include "cam.h"
#include <stdio.h>

/**
 * Convert a MMAL status return value to a simple boolean of success
 * ALso displays a fault if code is not success
 *
 * @param status The error code to convert
 * @return 0 if status is success, 1 otherwise
 */

void bcm_host_init(void) {
    VCHIQ_INSTANCE_T vchiq_instance;
    static int initted;
    int success;

    if (initted)
        return;
    initted = 1;
    vcos_init();

    if (vchiq_initialise(&vchiq_instance) != VCHIQ_SUCCESS) {
       vcos_log_error("* failed to open vchiq instance\n");
        exit(-1);
    }

    vcos_log("vchi_initialise");
    success = vchi_initialise(&global_initialise_instance);
    vcos_assert(success == 0);
    vchiq_instance = (VCHIQ_INSTANCE_T) global_initialise_instance;

    global_connection = vchi_create_connection(single_get_func_table(),
                                               vchi_mphi_message_driver_func_table());

    vcos_log("vchi_connect");
    vchi_connect(&global_connection, 1, global_initialise_instance);

    vc_vchi_gencmd_init(global_initialise_instance, &global_connection, 1);
    vc_vchi_dispmanx_init(global_initialise_instance, &global_connection, 1);
    vc_vchi_tv_init(global_initialise_instance, &global_connection, 1);
    vc_vchi_cec_init(global_initialise_instance, &global_connection, 1);

    if (success == 0) {
        vcos_assert(success == 0);
    }
}

int set_stereo_mode(MMAL_PORT_T *port, MMAL_PARAMETER_STEREOSCOPIC_MODE_T *stereo_mode) {
    MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo = {{MMAL_PARAMETER_STEREOSCOPIC_MODE, sizeof(stereo)},
                                                 MMAL_STEREOSCOPIC_MODE_NONE, MMAL_FALSE, MMAL_FALSE
    };
    if (stereo_mode->mode != MMAL_STEREOSCOPIC_MODE_NONE) {
        stereo.mode = stereo_mode->mode;
        stereo.decimate = stereo_mode->decimate;
        stereo.swap_eyes = stereo_mode->swap_eyes;
    }
    return mmal_port_parameter_set(port, &stereo.hdr);
}

/** Default camera callback function
 * Handles the --settings
 * @param port
 * @param Callback data
 */
void default_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    fprintf(stderr, "Camera control callback  cmd=0x%08x", buffer->cmd);

    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED) {
        MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *) buffer->data;
        switch (param->hdr.id) {
            case MMAL_PARAMETER_CAMERA_SETTINGS: {
                MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T *) param;
               vcos_log_info("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                       settings->exposure,
                       settings->analog_gain.num, settings->analog_gain.den,
                       settings->digital_gain.num, settings->digital_gain.den);
               vcos_log_info("AWB R=%u/%u, B=%u/%u",
                       settings->awb_red_gain.num, settings->awb_red_gain.den,
                       settings->awb_blue_gain.num, settings->awb_blue_gain.den);
            }
                break;
        }
    } else if (buffer->cmd == MMAL_EVENT_ERROR) {
       vcos_log_error("No data received from sensor. Check all connections, including the Sunny one on the camera board");
    } else {
       vcos_log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
    }

    mmal_buffer_header_release(buffer);
}

/**
 * Adjust the saturation level for images
 * @param camera Pointer to camera component
 * @param saturation Value to adjust, -100 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_saturation(MMAL_COMPONENT_T *camera, int saturation) {
    int ret = 0;

    if (!camera)
        return 1;

    if (saturation >= -100 && saturation <= 100) {
        MMAL_RATIONAL_T value = {saturation, 100};
        ret = mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SATURATION, value);
    } else {
       vcos_log_error("Invalid saturation value");
        ret = 1;
    }

    return ret;
}

/**
 * Set the sharpness of the image
 * @param camera Pointer to camera component
 * @param sharpness Sharpness adjustment -100 to 100
 */
int set_sharpness(MMAL_COMPONENT_T *camera, int sharpness) {
    int ret = 0;

    if (!camera)
        return 1;

    if (sharpness >= -100 && sharpness <= 100) {
        MMAL_RATIONAL_T value = {sharpness, 100};
        ret = mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SHARPNESS, value);
    } else {
       vcos_log_error("Invalid sharpness value");
        ret = 1;
    }

    return ret;
}

/**
 * Set the contrast adjustment for the image
 * @param camera Pointer to camera component
 * @param contrast Contrast adjustment -100 to  100
 * @return
 */
int set_contrast(MMAL_COMPONENT_T *camera, int contrast) {
    int ret = 0;

    if (!camera)
        return 1;

    if (contrast >= -100 && contrast <= 100) {
        MMAL_RATIONAL_T value = {contrast, 100};
        ret = (mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_CONTRAST, value));
    } else {
       vcos_log_error("Invalid contrast value");
        ret = 1;
    }

    return ret;
}

/**
 * Adjust the brightness level for images
 * @param camera Pointer to camera component
 * @param brightness Value to adjust, 0 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_brightness(MMAL_COMPONENT_T *camera, int brightness) {
    int ret = 0;

    if (!camera)
        return 1;

    if (brightness >= 0 && brightness <= 100) {
        MMAL_RATIONAL_T value = {brightness, 100};
        ret = (mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_BRIGHTNESS, value));
    } else {
       vcos_log_error("Invalid brightness value");
        ret = 1;
    }

    return ret;
}

/**
 * Adjust the ISO used for images
 * @param camera Pointer to camera component
 * @param ISO Value to set TODO :
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_ISO(MMAL_COMPONENT_T *camera, int ISO) {
    if (!camera)
        return 1;

    return (mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_ISO, ISO));
}

/**
 * Adjust the metering mode for images
 * @param camera Pointer to camera component
 * @param saturation Value from following
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_metering_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T m_mode) {
    MMAL_PARAMETER_EXPOSUREMETERINGMODE_T meter_mode = {{MMAL_PARAMETER_EXP_METERING_MODE, sizeof(meter_mode)},
                                                        m_mode
    };
    if (!camera)
        return 1;

    return (mmal_port_parameter_set(camera->control, &meter_mode.hdr));
}

/**
 * Set the video stabilisation flag. Only used in video mode
 * @param camera Pointer to camera component
 * @param saturation Flag 0 off 1 on
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_video_stabilisation(MMAL_COMPONENT_T *camera, int vstabilisation) {
    if (!camera)
        return 1;

    return (
            mmal_port_parameter_set_boolean(camera->control, MMAL_PARAMETER_VIDEO_STABILISATION, vstabilisation));
}

/**
 * Adjust the exposure compensation for images (EV)
 * @param camera Pointer to camera component
 * @param exp_comp Value to adjust, -10 to +10
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_exposure_compensation(MMAL_COMPONENT_T *camera, int exp_comp) {
    if (!camera)
        return 1;

    return (mmal_port_parameter_set_int32(camera->control, MMAL_PARAMETER_EXPOSURE_COMP, exp_comp));
}

/**
 * Set exposure mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_EXPOSUREMODE_OFF,
 *   - MMAL_PARAM_EXPOSUREMODE_AUTO,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,
 *   - MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPORTS,
 *   - MMAL_PARAM_EXPOSUREMODE_SNOW,
 *   - MMAL_PARAM_EXPOSUREMODE_BEACH,
 *   - MMAL_PARAM_EXPOSUREMODE_VERYLONG,
 *   - MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,
 *   - MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,
 *   - MMAL_PARAM_EXPOSUREMODE_FIREWORKS,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_exposure_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMODE_T mode) {
    MMAL_PARAMETER_EXPOSUREMODE_T exp_mode = {{MMAL_PARAMETER_EXPOSURE_MODE, sizeof(exp_mode)}, mode};

    if (!camera)
        return 1;

    return (mmal_port_parameter_set(camera->control, &exp_mode.hdr));
}

/**
 * Set flicker avoid mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_FLICKERAVOID_OFF,
 *   - MMAL_PARAM_FLICKERAVOID_AUTO,
 *   - MMAL_PARAM_FLICKERAVOID_50HZ,
 *   - MMAL_PARAM_FLICKERAVOID_60HZ,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_flicker_avoid_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_FLICKERAVOID_T mode) {
    MMAL_PARAMETER_FLICKERAVOID_T fl_mode = {{MMAL_PARAMETER_FLICKER_AVOID, sizeof(fl_mode)}, mode};

    if (!camera)
        return 1;

    return (mmal_port_parameter_set(camera->control, &fl_mode.hdr));
}

/**
 * Set the aWB (auto white balance) mode for images
 * @param camera Pointer to camera component
 * @param awb_mode Value to set from
 *   - MMAL_PARAM_AWBMODE_OFF,
 *   - MMAL_PARAM_AWBMODE_AUTO,
 *   - MMAL_PARAM_AWBMODE_SUNLIGHT,
 *   - MMAL_PARAM_AWBMODE_CLOUDY,
 *   - MMAL_PARAM_AWBMODE_SHADE,
 *   - MMAL_PARAM_AWBMODE_TUNGSTEN,
 *   - MMAL_PARAM_AWBMODE_FLUORESCENT,
 *   - MMAL_PARAM_AWBMODE_INCANDESCENT,
 *   - MMAL_PARAM_AWBMODE_FLASH,
 *   - MMAL_PARAM_AWBMODE_HORIZON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_awb_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_AWBMODE_T awb_mode) {
    MMAL_PARAMETER_AWBMODE_T param = {{MMAL_PARAMETER_AWB_MODE, sizeof(param)}, awb_mode};

    if (!camera)
        return 1;

    return (mmal_port_parameter_set(camera->control, &param.hdr));
}

int set_awb_gains(MMAL_COMPONENT_T *camera, float r_gain, float b_gain) {
    MMAL_PARAMETER_AWB_GAINS_T param = {{MMAL_PARAMETER_CUSTOM_AWB_GAINS, sizeof(param)},
                                        {0,                               0},
                                        {0,                               0}};

    if (!camera)
        return 1;

    if (!r_gain || !b_gain)
        return 0;

    param.r_gain.num = (unsigned int) (r_gain * 65536);
    param.b_gain.num = (unsigned int) (b_gain * 65536);
    param.r_gain.den = param.b_gain.den = 65536;
    return (mmal_port_parameter_set(camera->control, &param.hdr));
}

/**
 * Set the image effect for the images
 * @param camera Pointer to camera component
 * @param imageFX Value from
 *   - MMAL_PARAM_IMAGEFX_NONE,
 *   - MMAL_PARAM_IMAGEFX_NEGATIVE,
 *   - MMAL_PARAM_IMAGEFX_SOLARIZE,
 *   - MMAL_PARAM_IMAGEFX_POSTERIZE,
 *   - MMAL_PARAM_IMAGEFX_WHITEBOARD,
 *   - MMAL_PARAM_IMAGEFX_BLACKBOARD,
 *   - MMAL_PARAM_IMAGEFX_SKETCH,
 *   - MMAL_PARAM_IMAGEFX_DENOISE,
 *   - MMAL_PARAM_IMAGEFX_EMBOSS,
 *   - MMAL_PARAM_IMAGEFX_OILPAINT,
 *   - MMAL_PARAM_IMAGEFX_HATCH,
 *   - MMAL_PARAM_IMAGEFX_GPEN,
 *   - MMAL_PARAM_IMAGEFX_PASTEL,
 *   - MMAL_PARAM_IMAGEFX_WATERCOLOUR,
 *   - MMAL_PARAM_IMAGEFX_FILM,
 *   - MMAL_PARAM_IMAGEFX_BLUR,
 *   - MMAL_PARAM_IMAGEFX_SATURATION,
 *   - MMAL_PARAM_IMAGEFX_COLOURSWAP,
 *   - MMAL_PARAM_IMAGEFX_WASHEDOUT,
 *   - MMAL_PARAM_IMAGEFX_POSTERISE,
 *   - MMAL_PARAM_IMAGEFX_COLOURPOINT,
 *   - MMAL_PARAM_IMAGEFX_COLOURBALANCE,
 *   - MMAL_PARAM_IMAGEFX_CARTOON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_imageFX(MMAL_COMPONENT_T *camera, MMAL_PARAM_IMAGEFX_T imageFX) {
    MMAL_PARAMETER_IMAGEFX_T imgFX = {{MMAL_PARAMETER_IMAGE_EFFECT, sizeof(imgFX)}, imageFX};

    if (!camera)
        return 1;

    return (mmal_port_parameter_set(camera->control, &imgFX.hdr));
}

/**
 * Set the colour effect  for images (Set UV component)
 * @param camera Pointer to camera component
 * @param colourFX  Contains enable state and U and V numbers to set (e.g. 128,128 = Black and white)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_colourFX(MMAL_COMPONENT_T *camera, const MMAL_PARAM_COLOURFX_T *colourFX) {
    MMAL_PARAMETER_COLOURFX_T colfx = {{MMAL_PARAMETER_COLOUR_EFFECT, sizeof(colfx)}, 0, 0, 0};

    if (!camera)
        return 1;

    colfx.enable = colourFX->enable;
    colfx.u = colourFX->u;
    colfx.v = colourFX->v;

    return (mmal_port_parameter_set(camera->control, &colfx.hdr));

}

/**
 * Set the rotation of the image
 * @param camera Pointer to camera component
 * @param rotation Degree of rotation (any number, but will be converted to 0,90,180 or 270 only)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_rotation(MMAL_COMPONENT_T *camera, int rotation) {
    int ret;
    int my_rotation = ((rotation % 360) / 90) * 90;

    ret = mmal_port_parameter_set_int32(camera->output[0], MMAL_PARAMETER_ROTATION, my_rotation);
    mmal_port_parameter_set_int32(camera->output[1], MMAL_PARAMETER_ROTATION, my_rotation);
    mmal_port_parameter_set_int32(camera->output[2], MMAL_PARAMETER_ROTATION, my_rotation);

    return (ret);
}

/**
 * Set the flips state of the image
 * @param camera Pointer to camera component
 * @param hflip If true, horizontally flip the image
 * @param vflip If true, vertically flip the image
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip) {
    MMAL_PARAMETER_MIRROR_T mirror = {{MMAL_PARAMETER_MIRROR, sizeof(MMAL_PARAMETER_MIRROR_T)}, MMAL_PARAM_MIRROR_NONE};

    if (hflip && vflip)
        mirror.value = MMAL_PARAM_MIRROR_BOTH;
    else if (hflip)
        mirror.value = MMAL_PARAM_MIRROR_HORIZONTAL;
    else if (vflip)
        mirror.value = MMAL_PARAM_MIRROR_VERTICAL;

    mmal_port_parameter_set(camera->output[0], &mirror.hdr);
    mmal_port_parameter_set(camera->output[1], &mirror.hdr);
    return (mmal_port_parameter_set(camera->output[2], &mirror.hdr));
}

/**
 * Set the ROI of the sensor to use for captures/preview
 * @param camera Pointer to camera component
 * @param rect   Normalised coordinates of ROI rectangle
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_ROI(MMAL_COMPONENT_T *camera, PARAM_FLOAT_RECT_T rect) {
    MMAL_PARAMETER_INPUT_CROP_T crop = {{MMAL_PARAMETER_INPUT_CROP, sizeof(MMAL_PARAMETER_INPUT_CROP_T)}};

    crop.rect.x = (65536 * rect.x);
    crop.rect.y = (65536 * rect.y);
    crop.rect.width = (65536 * rect.w);
    crop.rect.height = (65536 * rect.h);

    return (mmal_port_parameter_set(camera->control, &crop.hdr));
}

/**
 * Adjust the exposure time used for images
 * @param camera Pointer to camera component
 * @param shutter speed in microseconds
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_shutter_speed(MMAL_COMPONENT_T *camera, int speed) {
    if (!camera)
        return 1;

    return (mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_SHUTTER_SPEED, speed));
}

/**
 * Adjust the Dynamic range compression level
 * @param camera Pointer to camera component
 * @param strength Strength of DRC to apply
 *        MMAL_PARAMETER_DRC_STRENGTH_OFF
 *        MMAL_PARAMETER_DRC_STRENGTH_LOW
 *        MMAL_PARAMETER_DRC_STRENGTH_MEDIUM
 *        MMAL_PARAMETER_DRC_STRENGTH_HIGH
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_DRC(MMAL_COMPONENT_T *camera, MMAL_PARAMETER_DRC_STRENGTH_T strength) {
    MMAL_PARAMETER_DRC_T drc = {{MMAL_PARAMETER_DYNAMIC_RANGE_COMPRESSION, sizeof(MMAL_PARAMETER_DRC_T)}, strength};

    if (!camera)
        return 1;

    return (mmal_port_parameter_set(camera->control, &drc.hdr));
}

int set_stats_pass(MMAL_COMPONENT_T *camera, int stats_pass) {
    if (!camera)
        return 1;

    return (
            mmal_port_parameter_set_boolean(camera->control, MMAL_PARAMETER_CAPTURE_STATS_PASS, stats_pass));
}

/**
 * Set the annotate data
 * @param camera Pointer to camera component
 * @param Bitmask of required annotation data. 0 for off.
 * @param If set, a pointer to text string to use instead of bitmask, max length 32 characters
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int set_annotate(MMAL_COMPONENT_T *camera, const int settings, const char *string,
                 const int text_size, const int text_colour, const int bg_colour,
                 const unsigned int justify, const unsigned int x, const unsigned int y) {
    MMAL_PARAMETER_CAMERA_ANNOTATE_V4_T annotate =
            {{MMAL_PARAMETER_ANNOTATE, sizeof(MMAL_PARAMETER_CAMERA_ANNOTATE_V4_T)}};

    if (settings) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char tmp[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V4];
        int process_datetime = 1;

        annotate.enable = 1;

        if (settings & (ANNOTATE_APP_TEXT | ANNOTATE_USER_TEXT)) {
            if ((settings & (ANNOTATE_TIME_TEXT | ANNOTATE_DATE_TEXT)) && strchr(string, '%') != NULL) {
                //string contains strftime parameter?
                strftime(annotate.text, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3, string, &tm);
                process_datetime = 0;
            } else {
                strncpy(annotate.text, string, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3);
            }
            annotate.text[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 - 1] = '\0';
        }

        if (process_datetime && (settings & ANNOTATE_TIME_TEXT)) {
            if (strlen(annotate.text)) {
                strftime(tmp, 32, " %X", &tm);
            } else {
                strftime(tmp, 32, "%X", &tm);
            }
            strncat(annotate.text, tmp, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 - strlen(annotate.text) - 1);
        }

        if (process_datetime && (settings & ANNOTATE_DATE_TEXT)) {
            if (strlen(annotate.text)) {
                strftime(tmp, 32, " %x", &tm);
            } else {
                strftime(tmp, 32, "%x", &tm);
            }
            strncat(annotate.text, tmp, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 - strlen(annotate.text) - 1);
        }

        if (settings & ANNOTATE_SHUTTER_SETTINGS)
            annotate.show_shutter = MMAL_TRUE;

        if (settings & ANNOTATE_GAIN_SETTINGS)
            annotate.show_analog_gain = MMAL_TRUE;

        if (settings & ANNOTATE_LENS_SETTINGS)
            annotate.show_lens = MMAL_TRUE;

        if (settings & ANNOTATE_CAF_SETTINGS)
            annotate.show_caf = MMAL_TRUE;

        if (settings & ANNOTATE_MOTION_SETTINGS)
            annotate.show_motion = MMAL_TRUE;

        if (settings & ANNOTATE_FRAME_NUMBER)
            annotate.show_frame_num = MMAL_TRUE;

        if (settings & ANNOTATE_BLACK_BACKGROUND)
            annotate.enable_text_background = MMAL_TRUE;

        annotate.text_size = text_size;

        if (text_colour != -1) {
            annotate.custom_text_colour = MMAL_TRUE;
            annotate.custom_text_Y = text_colour & 0xff;
            annotate.custom_text_U = (text_colour >> 8) & 0xff;
            annotate.custom_text_V = (text_colour >> 16) & 0xff;
        } else
            annotate.custom_text_colour = MMAL_FALSE;

        if (bg_colour != -1) {
            annotate.custom_background_colour = MMAL_TRUE;
            annotate.custom_background_Y = bg_colour & 0xff;
            annotate.custom_background_U = (bg_colour >> 8) & 0xff;
            annotate.custom_background_V = (bg_colour >> 16) & 0xff;
        } else
            annotate.custom_background_colour = MMAL_FALSE;

        annotate.justify = justify;
        annotate.x_offset = x;
        annotate.y_offset = y;
    } else
        annotate.enable = 0;

    return (mmal_port_parameter_set(camera->control, &annotate.hdr));
}
/**
 * 
 * @param camera 
 * @param analog 
 * @param digital 
 * @return 
 */
int set_gains(MMAL_COMPONENT_T *camera, float analog, float digital) {
    MMAL_RATIONAL_T rational = {0, 65536};
    MMAL_STATUS_T status;

    if (!camera)
        return 1;

    rational.num = (unsigned int) (analog * 65536);
    status = mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_ANALOG_GAIN, rational);
    if (status != MMAL_SUCCESS)
        return (status);

    rational.num = (unsigned int) (digital * 65536);
    status = mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_DIGITAL_GAIN, rational);
    return (status);
}

/**
 * Set the specified camera to all the specified settings
 * @param camera Pointer to camera component
 * @param params Pointer to parameter block containing parameters
 * @return 0 if successful, none-zero if unsuccessful.
 */
int set_all_parameters(MMAL_COMPONENT_T *camera, const CAM_PARAMETERS *params) {
    int result;

    result  = set_saturation(camera, params->saturation);
    result += set_sharpness(camera, params->sharpness);
    result += set_contrast(camera, params->contrast);
    result += set_brightness(camera, params->brightness);
    result += set_ISO(camera, params->ISO);
    result += set_video_stabilisation(camera, params->videoStabilisation);
    result += set_exposure_compensation(camera, params->exposureCompensation);
    result += set_exposure_mode(camera, params->exposureMode);
    result += set_flicker_avoid_mode(camera, params->flickerAvoidMode);
    result += set_metering_mode(camera, params->exposureMeterMode);
    result += set_awb_mode(camera, params->awbMode);
    result += set_awb_gains(camera, params->awb_gains_r, params->awb_gains_b);
    result += set_imageFX(camera, params->imageEffect);
    result += set_colourFX(camera, &params->colourEffects);
    result += set_rotation(camera, params->rotation);
    result += set_flips(camera, params->hflip, params->vflip);
    result += set_ROI(camera, params->roi);
    result += set_shutter_speed(camera, params->shutter_speed);
    result += set_DRC(camera, params->drc_level);
    result += set_stats_pass(camera, params->stats_pass);
    result += set_annotate(camera, params->enable_annotate, params->annotate_string,
                                           params->annotate_text_size,
                                           params->annotate_text_colour,
                                           params->annotate_bg_colour,
                                           params->annotate_justify,
                                           params->annotate_x,
                                           params->annotate_y);
    result += set_gains(camera, params->analog_gain, params->digital_gain);

    if (params->settings)
    {
        MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request =
                {
                        {MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
                        MMAL_PARAMETER_CAMERA_SETTINGS, 1
                };

        MMAL_STATUS_T status = mmal_port_parameter_set(camera->control, &change_event_request.hdr);
        if ( status != MMAL_SUCCESS )
        {
            vcos_log_error("No camera settings events");
        }

        result += status;
    }

    return result;
}
/**
 * Assign set of default parameters to the passed in parameter block
 *
 * @param state Pointer to parameter block
 *
 */
void preview_set_defaults(CAM_PREVIEW_PARAMETERS *state)
{
    state->wantPreview = 1;
    state->wantFullScreenPreview = 1;
    state->opacity = 255;
    state->previewWindow.x = 0;
    state->previewWindow.y = 0;
    state->previewWindow.width = 1024;
    state->previewWindow.height = 768;
    state->preview_component = NULL;
}

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T create_camera_component(CAM_STATE *state) {
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *video_port = NULL, *still_port = NULL;
    MMAL_STATUS_T status;

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Failed to create camera component");
        goto error;
    }

    status = set_stereo_mode(camera->output[0], &state->camera_parameters.stereo_mode);
    status += set_stereo_mode(camera->output[1], &state->camera_parameters.stereo_mode);
    status += set_stereo_mode(camera->output[2], &state->camera_parameters.stereo_mode);

    if (status != MMAL_SUCCESS) {
       vcos_log_error("Could not set stereo mode : error %d", status);
        goto error;
    }

    MMAL_PARAMETER_INT32_T camera_num =
            {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->common_settings.cameraNum};

    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

    if (status != MMAL_SUCCESS) {
       vcos_log_error("Could not select camera : error %d", status);
        goto error;
    }

    if (!camera->output_num) {
        status = MMAL_ENOSYS;
       vcos_log_error("Camera doesn't have output ports");
        goto error;
    }

    status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG,
                                            state->common_settings.sensor_mode);

    if (status != MMAL_SUCCESS) {
       vcos_log_error("Could not set sensor mode : error %d", status);
        goto error;
    }

    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, default_camera_control_callback);

    if (status != MMAL_SUCCESS) {
       vcos_log_error("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
                {
                        {MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config)},
                        .max_stills_w = state->common_settings.width,
                        .max_stills_h = state->common_settings.height,
                        .stills_yuv422 = 0,
                        .one_shot_stills = 0,
                        .stills_capture_circular_buffer_height = 0,
                        .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC,
                };
        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    // Now set up the port formats

    //enable dynamic framerate if necessary
    if (state->camera_parameters.shutter_speed) {
        if (state->framerate > 1000000. / state->camera_parameters.shutter_speed) {
            state->framerate = 0;
            if (state->common_settings.verbose)
                fprintf(stderr, "Enable dynamic frame rate to fulfil shutter speed requirement\n");
        }
    }
    // Set the encode format on the video  port
    // Set the encode format on the still  port

    format = still_port->format;

    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->common_settings.width;
    format->es->video.crop.height = state->common_settings.height;
    format->es->video.frame_rate.num = 0;
    format->es->video.frame_rate.den = 1;

    status = mmal_port_format_commit(still_port);

    if (status != MMAL_SUCCESS) {
       vcos_log_error("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);

    if (status != MMAL_SUCCESS) {
       vcos_log_error("camera component couldn't be enabled");
        goto error;
    }

    // Note: this sets lots of parameters that were not individually addressed before.
    set_all_parameters(camera, &state->camera_parameters);

    state->camera_component = camera;

    if (state->common_settings.verbose)
        vcos_log_info("Camera component done\n");

    return status;

    error:

    if (camera)
        mmal_component_destroy(camera);

    return status;
}

/**
 * Create the encoder component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T create_encoder_component(CAM_STATE *state) {
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
    MMAL_STATUS_T status;
    MMAL_POOL_T *pool;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);

    if (status != MMAL_SUCCESS) {
       vcos_log_error("Unable to create video encoder component");
        goto error;
    }

    if (!encoder->input_num || !encoder->output_num) {
        status = MMAL_ENOSYS;
       vcos_log_error("Video encoder doesn't have input/output ports");
        goto error;
    }

    encoder_input = encoder->input[0];
    encoder_output = encoder->output[0];

    // We want same format on input and output
    mmal_format_copy(encoder_output->format, encoder_input->format);

    // Only supporting H264 at the moment
    encoder_output->format->encoding = state->encoding;

    if (state->encoding == MMAL_ENCODING_H264) {
        if (state->level == MMAL_VIDEO_LEVEL_H264_4) {
            if (state->bitrate > MAX_BITRATE_LEVEL4) {
                vcos_log_error( "Bitrate too high: Reducing to 25MBit/s\n");
                state->bitrate = MAX_BITRATE_LEVEL4;
            }
        } else {
            if (state->bitrate > MAX_BITRATE_LEVEL42) {
                vcos_log_error( "Bitrate too high: Reducing to 62.5MBit/s\n");
                state->bitrate = MAX_BITRATE_LEVEL42;
            }
        }
    } else if (state->encoding == MMAL_ENCODING_MJPEG) {
        if (state->bitrate > MAX_BITRATE_MJPEG) {
            vcos_log_error( "Bitrate too high: Reducing to 25MBit/s\n");
            state->bitrate = MAX_BITRATE_MJPEG;
        }
    }

    encoder_output->format->bitrate = state->bitrate;

    if (state->encoding == MMAL_ENCODING_H264)
        encoder_output->buffer_size = encoder_output->buffer_size_recommended;
    else
        encoder_output->buffer_size = 256 << 10;


    if (encoder_output->buffer_size < encoder_output->buffer_size_min)
        encoder_output->buffer_size = encoder_output->buffer_size_min;

    encoder_output->buffer_num = encoder_output->buffer_num_recommended;

    if (encoder_output->buffer_num < encoder_output->buffer_num_min)
        encoder_output->buffer_num = encoder_output->buffer_num_min;

    // We need to set the frame rate on output to 0, to ensure it gets
    // updated correctly from the input framerate when port connected
    encoder_output->format->es->video.frame_rate.num = 0;
    encoder_output->format->es->video.frame_rate.den = 1;

    // Commit the port changes to the output port
    status = mmal_port_format_commit(encoder_output);

    if (status != MMAL_SUCCESS) {
       vcos_log_error("Unable to set format on video encoder output port");
        goto error;
    }

    // Set the rate control parameter
    if (0) {
        MMAL_PARAMETER_VIDEO_RATECONTROL_T param = {{MMAL_PARAMETER_RATECONTROL, sizeof(param)},
                                                    MMAL_VIDEO_RATECONTROL_DEFAULT};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
           vcos_log_error("Unable to set ratecontrol");
            goto error;
        }

    }

    if (state->encoding == MMAL_ENCODING_H264 &&
        state->intraperiod != -1) {
        MMAL_PARAMETER_UINT32_T param = {{MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, state->intraperiod};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
           vcos_log_error("Unable to set intraperiod");
            goto error;
        }
    }

    if (state->encoding == MMAL_ENCODING_H264 && state->slices > 1 && state->common_settings.width <= 1280) {
        int frame_mb_rows = VCOS_ALIGN_UP(state->common_settings.height, 16) >> 4;

        if (state->slices > frame_mb_rows) //warn user if too many slices selected
        {
            vcos_log_error( "H264 Slice count (%d) exceeds number of macroblock rows (%d). Setting slices to %d.\n",
                    state->slices, frame_mb_rows, frame_mb_rows);
            // Continue rather than abort..
        }
        int slice_row_mb = frame_mb_rows / state->slices;
        if (frame_mb_rows - state->slices * slice_row_mb)
            slice_row_mb++; //must round up to avoid extra slice if not evenly divided

        status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_MB_ROWS_PER_SLICE, slice_row_mb);
        if (status != MMAL_SUCCESS) {
           vcos_log_error("Unable to set number of slices");
            goto error;
        }
    }

    if (state->encoding == MMAL_ENCODING_H264 &&
        state->quantisationParameter) {
        MMAL_PARAMETER_UINT32_T param = {{MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)},
                                         state->quantisationParameter};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
           vcos_log_error("Unable to set initial QP");
            goto error;
        }

        MMAL_PARAMETER_UINT32_T param2 = {{MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param)},
                                          state->quantisationParameter};
        status = mmal_port_parameter_set(encoder_output, &param2.hdr);
        if (status != MMAL_SUCCESS) {
           vcos_log_error("Unable to set min QP");
            goto error;
        }

        MMAL_PARAMETER_UINT32_T param3 = {{MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param)},
                                          state->quantisationParameter};
        status = mmal_port_parameter_set(encoder_output, &param3.hdr);
        if (status != MMAL_SUCCESS) {
           vcos_log_error("Unable to set max QP");
            goto error;
        }
    }

    if (state->encoding == MMAL_ENCODING_H264) {
        MMAL_PARAMETER_VIDEO_PROFILE_T param;
        param.hdr.id = MMAL_PARAMETER_PROFILE;
        param.hdr.size = sizeof(param);

        param.profile[0].profile = state->profile;

        if ((VCOS_ALIGN_UP(state->common_settings.width, 16) >> 4) *
            (VCOS_ALIGN_UP(state->common_settings.height, 16) >> 4) * state->framerate > 245760) {
            if ((VCOS_ALIGN_UP(state->common_settings.width, 16) >> 4) *
                (VCOS_ALIGN_UP(state->common_settings.height, 16) >> 4) * state->framerate <= 522240) {
                vcos_log_error( "Too many macroblocks/s: Increasing H264 Level to 4.2\n");
                state->level = MMAL_VIDEO_LEVEL_H264_42;
            } else {
               vcos_log_error("Too many macroblocks/s requested");
                status = MMAL_EINVAL;
                goto error;
            }
        }

        param.profile[0].level = state->level;

        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
           vcos_log_error("Unable to set H264 profile");
            goto error;
        }
    }

    if (mmal_port_parameter_set_boolean(encoder_input, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, state->immutableInput) !=
        MMAL_SUCCESS) {
       vcos_log_error("Unable to set immutable input flag");
        // Continue rather than abort..
    }

    if (state->encoding == MMAL_ENCODING_H264) {
        //set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
        if (mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER,
                                            state->bInlineHeaders) != MMAL_SUCCESS) {
           vcos_log_error("failed to set INLINE HEADER FLAG parameters");
            // Continue rather than abort..
        }

        //set flag for add SPS TIMING
        if (mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_SPS_TIMING,
                                            state->addSPSTiming) != MMAL_SUCCESS) {
           vcos_log_error("failed to set SPS TIMINGS FLAG parameters");
            // Continue rather than abort..
        }

        //set INLINE VECTORS flag to request motion vector estimates
        if (mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_VECTORS,
                                            state->inlineMotionVectors) != MMAL_SUCCESS) {
           vcos_log_error("failed to set INLINE VECTORS parameters");
            // Continue rather than abort..
        }

        // Adaptive intra refresh settings
        if (state->intra_refresh_type != -1) {
            MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T param;
            param.hdr.id = MMAL_PARAMETER_VIDEO_INTRA_REFRESH;
            param.hdr.size = sizeof(param);

            // Get first so we don't overwrite anything unexpectedly
            status = mmal_port_parameter_get(encoder_output, &param.hdr);
            if (status != MMAL_SUCCESS) {
               vcos_log_error("Unable to get existing H264 intra-refresh values. Please update your firmware");
                // Set some defaults, don't just pass random stack data
                param.air_mbs = param.air_ref = param.cir_mbs = param.pir_mbs = 0;
            }

            param.refresh_mode = state->intra_refresh_type;

            //if (state->intra_refresh_type == MMAL_VIDEO_INTRA_REFRESH_CYCLIC_MROWS)
            //   param.cir_mbs = 10;

            status = mmal_port_parameter_set(encoder_output, &param.hdr);
            if (status != MMAL_SUCCESS) {
               vcos_log_error("Unable to set H264 intra-refresh values");
                goto error;
            }
        }
    }

    //  Enable component
   vcos_log_info("enabling the encoder component...\n");
    status = mmal_component_enable(encoder);
    if (status != MMAL_SUCCESS) {
       vcos_log_error("Unable to enable video encoder component\n");
        goto error;
    }

    /* Create pool of buffer headers for the output port to consume */
   vcos_log_info("creating the buffer header pool...\n");
    pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);

    if (!pool) {
       vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);
    }

    state->video_encoder_pool = pool;
    state->video_encoder_component = encoder;

   vcos_log_info("encoder component done\n");
    if (state->common_settings.verbose)
        vcos_log_error( "Encoder component done\n");

    return status;

    error:
    if (encoder)
        mmal_component_destroy(encoder);

    state->video_encoder_component = NULL;

    return status;
}

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct. camera_component member set to the created camera_component if successful.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T create_still_camera_component(CAM_STATE *state) {
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
    MMAL_STATUS_T status;

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Failed to create camera component");
        goto error;
    }

    status = set_stereo_mode(camera->output[0], &state->camera_parameters.stereo_mode);
    status += set_stereo_mode(camera->output[1], &state->camera_parameters.stereo_mode);
    status += set_stereo_mode(camera->output[2], &state->camera_parameters.stereo_mode);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not set stereo mode : error %d", status);
        goto error;
    }

    MMAL_PARAMETER_INT32_T camera_num =
            {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->common_settings.cameraNum};

    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not select camera : error %d", status);
        goto error;
    }

    if (!camera->output_num)
    {
        status = MMAL_ENOSYS;
        vcos_log_error("Camera doesn't have output ports");
        goto error;
    }

    status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->common_settings.sensor_mode);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not set sensor mode : error %d", status);
        goto error;
    }

    preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, default_camera_control_callback);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
                {
                        { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
                        .max_stills_w = state->common_settings.width,
                        .max_stills_h = state->common_settings.height,
                        .stills_yuv422 = 0,
                        .one_shot_stills = 1,
                        .max_preview_video_w = state->preview_parameters.previewWindow.width,
                        .max_preview_video_h = state->preview_parameters.previewWindow.height,
                        .num_preview_video_frames = 3,
                        .stills_capture_circular_buffer_height = 0,
                        .fast_preview_resume = 0,
                        .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
                };

        if (state->fullResPreview)
        {
            cam_config.max_preview_video_w = state->common_settings.width;
            cam_config.max_preview_video_h = state->common_settings.height;
        }

        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    set_all_parameters(camera, &state->camera_parameters);

    // Now set up the port formats

    format = preview_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    if(state->camera_parameters.shutter_speed > 6000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 50, 1000 }, {166, 1000}
        };
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    }
    else if(state->camera_parameters.shutter_speed > 1000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 166, 1000 }, {999, 1000}
        };
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    }
    if (state->fullResPreview)
    {
        // In this mode we are forcing the preview to be generated from the full capture resolution.
        // This runs at a max of 15fps with the OV5647 sensor.
        format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
        format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = state->common_settings.width;
        format->es->video.crop.height = state->common_settings.height;
        format->es->video.frame_rate.num = FULL_RES_PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = FULL_RES_PREVIEW_FRAME_RATE_DEN;
    }
    else
    {
        // Use a full FOV 4:3 mode
        format->es->video.width = VCOS_ALIGN_UP(state->preview_parameters.previewWindow.width, 32);
        format->es->video.height = VCOS_ALIGN_UP(state->preview_parameters.previewWindow.height, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = state->preview_parameters.previewWindow.width;
        format->es->video.crop.height = state->preview_parameters.previewWindow.height;
        format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;
    }

    status = mmal_port_format_commit(preview_port);
    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera viewfinder format couldn't be set");
        goto error;
    }

    // Set the same format on the video  port (which we don't use here)
    mmal_format_full_copy(video_port->format, format);
    status = mmal_port_format_commit(video_port);

    if (status  != MMAL_SUCCESS)
    {
        vcos_log_error("camera video format couldn't be set");
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    format = still_port->format;

    if(state->camera_parameters.shutter_speed > 6000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 50, 1000 }, {166, 1000}
        };
        mmal_port_parameter_set(still_port, &fps_range.hdr);
    }
    else if(state->camera_parameters.shutter_speed > 1000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 167, 1000 }, {999, 1000}
        };
        mmal_port_parameter_set(still_port, &fps_range.hdr);
    }
    // Set our stills format on the stills (for encoder) port
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->common_settings.width;
    format->es->video.crop.height = state->common_settings.height;
    format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;

    status = mmal_port_format_commit(still_port);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera component couldn't be enabled");
        goto error;
    }

    state->camera_component = camera;
    state->still_encoder_input_port = still_port;
    state->camera_video_port = video_port;
    state->preview_parameters.camera_preview_port = preview_port;

    if (state->common_settings.verbose)
        vcos_log_info( "Camera component done\n");

    return status;

    error:

    if (camera)
        mmal_component_destroy(camera);

    return status;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_still_camera_component(CAM_STATE *state) {
    if (state->camera_component)
    {
        mmal_component_destroy(state->camera_component);
        state->camera_component = NULL;
    }
}

/**
 * Create the encoder component, set up its ports
 *
 * @param state Pointer to state control struct. encoder_component member set to the created camera_component if successful.
 *
 * @return a MMAL_STATUS, MMAL_SUCCESS if all OK, something else otherwise
 */
MMAL_STATUS_T create_still_encoder_component(CAM_STATE *state) {
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
    MMAL_STATUS_T status;
    MMAL_POOL_T *pool;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &encoder);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to create JPEG encoder component");
        goto error;
    }

    if (!encoder->input_num || !encoder->output_num)
    {
        status = MMAL_ENOSYS;
        vcos_log_error("JPEG encoder doesn't have input/output ports");
        goto error;
    }

    encoder_input = encoder->input[0];
    encoder_output = encoder->output[0];

    // We want same format on input and output
    mmal_format_copy(encoder_output->format, encoder_input->format);

    // Specify out output format
    encoder_output->format->encoding = state->encoding;

    encoder_output->buffer_size = encoder_output->buffer_size_recommended;

    if (encoder_output->buffer_size < encoder_output->buffer_size_min)
        encoder_output->buffer_size = encoder_output->buffer_size_min;

    encoder_output->buffer_num = encoder_output->buffer_num_recommended;

    if (encoder_output->buffer_num < encoder_output->buffer_num_min)
        encoder_output->buffer_num = encoder_output->buffer_num_min;

    // Commit the port changes to the output port
    status = mmal_port_format_commit(encoder_output);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to set format on video encoder output port");
        goto error;
    }

    // Set the JPEG quality level
    status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_JPEG_Q_FACTOR, state->quality);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to set JPEG quality");
        goto error;
    }

    // Set the JPEG restart interval
    status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_JPEG_RESTART_INTERVAL, state->restart_interval);

    if (state->restart_interval && status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to set JPEG restart interval");
        goto error;
    }

    // Set up any required thumbnail
    {
        MMAL_PARAMETER_THUMBNAIL_CONFIG_T param_thumb = {{MMAL_PARAMETER_THUMBNAIL_CONFIGURATION, sizeof(MMAL_PARAMETER_THUMBNAIL_CONFIG_T)}, 0, 0, 0, 0};

        if ( state->thumbnailConfig.enable &&
             state->thumbnailConfig.width > 0 && state->thumbnailConfig.height > 0 )
        {
            // Have a valid thumbnail defined
            param_thumb.enable = 1;
            param_thumb.width = state->thumbnailConfig.width;
            param_thumb.height = state->thumbnailConfig.height;
            param_thumb.quality = state->thumbnailConfig.quality;
        }
        status = mmal_port_parameter_set(encoder->control, &param_thumb.hdr);
    }

    //  Enable component
    status = mmal_component_enable(encoder);

    if (status  != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to enable video encoder component");
        goto error;
    }

    /* Create pool of buffer headers for the output port to consume */
    pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);

    if (!pool)
    {
        vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);
    }

    state->encoder_pool = pool;
    state->still_encoder_component = encoder;
    state->still_encoder_output_port = encoder_output;
    state->still_encoder_input_port = encoder_input;

    if (state->common_settings.verbose)
        vcos_log_info( "Encoder component done\n");

    return status;

    error:

    if (encoder)
        mmal_component_destroy(encoder);

    return status;
}

/**
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_still_encoder_component(CAM_STATE *state) {
    // Get rid of any port buffers first
    if (state->encoder_pool)
    {
        mmal_port_pool_destroy(state->encoder_component->output[0], state->encoder_pool);
    }

    if (state->encoder_component)
    {
        mmal_component_destroy(state->encoder_component);
        state->encoder_component = NULL;
    }
}

/** 
 * Set default
 * @param state 
 */
void commonsettings_set_defaults(CAM_COMMONSETTINGS_PARAMETERS *state) {
    strncpy(state->camera_name, "(Unknown)", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
    // We dont set width and height since these will be specific to the app being built.
    state->width = 0;
    state->height = 0;
    state->filename = NULL;
    state->verbose = 0;
    state->cameraNum = 0;
    state->sensor_mode = 0;
    state->gps = 0;
};

/**
 * Give the supplied parameter block a set of default values
 * @params Pointer to parameter block
 */
void camcontrol_set_defaults(CAM_PARAMETERS *params) {
    vcos_assert(params);

    params->sharpness = 0;
    params->contrast = 0;
    params->brightness = 50;
    params->saturation = 0;
    params->ISO = 0;                    // 0 = auto
    params->videoStabilisation = 0;
    params->exposureCompensation = 0;
    params->exposureMode = MMAL_PARAM_EXPOSUREMODE_AUTO;
    params->flickerAvoidMode = MMAL_PARAM_FLICKERAVOID_OFF;
    params->exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE;
    params->awbMode = MMAL_PARAM_AWBMODE_AUTO;
    params->imageEffect = MMAL_PARAM_IMAGEFX_NONE;
    params->colourEffects.enable = 0;
    params->colourEffects.u = 128;
    params->colourEffects.v = 128;
    params->rotation = 0;
    params->hflip = params->vflip = 0;
    params->roi.x = params->roi.y = 0.0;
    params->roi.w = params->roi.h = 1.0;
    params->shutter_speed = 0;          // 0 = auto
    params->awb_gains_r = 0;      // Only have any function if AWB OFF is used.
    params->awb_gains_b = 0;
    params->drc_level = MMAL_PARAMETER_DRC_STRENGTH_OFF;
    params->stats_pass = MMAL_FALSE;
    params->enable_annotate = 0;
    params->annotate_string[0] = '\0';
    params->annotate_text_size = 0;    //Use firmware default
    params->annotate_text_colour = -1;   //Use firmware default
    params->annotate_bg_colour = -1;     //Use firmware default
    params->stereo_mode.mode = MMAL_STEREOSCOPIC_MODE_NONE;
    params->stereo_mode.decimate = MMAL_FALSE;
    params->stereo_mode.swap_eyes = MMAL_FALSE;
}

/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
int default_still_state(CAM_STATE *state) {
    if (!state)
    {
        vcos_assert(0);
        return -1;
    }

    memset(state, 0, sizeof(*state));

    commonsettings_set_defaults(&state->common_settings);

    state->timeout = -1; // replaced with 5000ms later if unset
    state->quality = 85;
    state->wantRAW = 0;
    state->linkname = NULL;
    state->frameStart = 0;
    state->thumbnailConfig.enable = 1;
    state->thumbnailConfig.width = 64;
    state->thumbnailConfig.height = 48;
    state->thumbnailConfig.quality = 35;
    state->camera_component = NULL;
    state->still_encoder_component = NULL;
    state->encoder_connection = NULL;
    state->encoder_pool = NULL;
    state->encoding = MMAL_ENCODING_JPEG;
    state->timelapse = 0;
    state->fullResPreview = 0;
    state->frameNextMethod = FRAME_NEXT_SINGLE;
    state->burstCaptureMode=0;
    state->timestamp = 0;
    state->restart_interval = 0;

    // Set up the camera_parameters to default
    camcontrol_set_defaults(&state->camera_parameters);

    // Setup preview window defaults
    preview_set_defaults(&state->preview_parameters);
}

/**
 * Create the preview component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T preview_create(CAM_PREVIEW_PARAMETERS *state) {
    MMAL_COMPONENT_T *preview = 0;
    MMAL_PORT_T *preview_port = NULL;
    MMAL_STATUS_T status;

    if (!state->wantPreview)
    {
        // No preview required, so create a null sink component to take its place
        status = mmal_component_create("vc.null_sink", &preview);

        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to create null sink component");
            goto error;
        }
    }
    else
    {
        status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER,
                                       &preview);

        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to create preview component");
            goto error;
        }

        if (!preview->input_num)
        {
            status = MMAL_ENOSYS;
            vcos_log_error("No input ports found on component");
            goto error;
        }

        preview_port = preview->input[0];

        MMAL_DISPLAYREGION_T param;
        param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
        param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);

        param.set = MMAL_DISPLAY_SET_LAYER;
        param.layer = PREVIEW_LAYER;

        param.set |= MMAL_DISPLAY_SET_ALPHA;
        param.alpha = state->opacity;

        if (state->wantFullScreenPreview)
        {
            param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
            param.fullscreen = 1;
        }
        else
        {
            param.set |= (MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_FULLSCREEN);
            param.fullscreen = 0;
            param.dest_rect = state->previewWindow;
        }

        status = mmal_port_parameter_set(preview_port, &param.hdr);

        if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
        {
            vcos_log_error("unable to set preview port parameters (%u)", status);
            goto error;
        }
    }

    /* Enable component */
    status = mmal_component_enable(preview);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to enable preview/null sink component (%u)", status);
        goto error;
    }

    state->preview_component = preview;
    state->camera_preview_port = preview_port;

    return status;

    error:

    if (preview)
        mmal_component_destroy(preview);

    return status;
}

/**
 * Destroy the preview component
 *
 * @param state Pointer to state control struct
 *
 */
void preview_destroy(CAM_PREVIEW_PARAMETERS *state)
{
    if (state->preview_component)
    {
        mmal_component_destroy(state->preview_component);
        state->preview_component = NULL;
    }
}

/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
void default_state(CAM_STATE *state) {
    if (!state) {
        vcos_assert(0);
        return;
    }

    // Default everything to zero
    memset(state, 0, sizeof(CAM_STATE));

    commonsettings_set_defaults(&state->common_settings);

    // Now set anything non-zero
    state->timeout = -1; // replaced with 5000ms later if unset
    state->common_settings.width = 1920;       // Default to 1080p
    state->common_settings.height = 1080;
    state->encoding = MMAL_ENCODING_H264;
    state->bitrate = 17000000; // This is a decent default bitrate for 1080p
    state->framerate = VIDEO_FRAME_RATE_NUM;
    state->intraperiod = -1;    // Not set
    state->quantisationParameter = 0;
    state->immutableInput = 1;
    state->profile = MMAL_VIDEO_PROFILE_H264_BASELINE;
    state->level = MMAL_VIDEO_LEVEL_H264_4;
    state->waitMethod = WAIT_METHOD_NONE;
    state->onTime = 5000;
    state->offTime = 5000;
    state->bCapturing = 0;
    state->bInlineHeaders = 0;
    state->segmentSize = 0;  // 0 = not segmenting the file.
    state->segmentNumber = 1;
    state->segmentWrap = 0; // Point at which to wrap segment number back to 1. 0 = no wrap
    state->splitNow = 0;
    state->splitWait = 0;
    state->inlineMotionVectors = 0;
    state->intra_refresh_type = -1;
    state->frame = 0;
    state->addSPSTiming = MMAL_FALSE;
    state->slices = 1;

    // Set up the camera_parameters to default
    camcontrol_set_defaults(&state->camera_parameters);
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_camera_component(CAM_STATE *state) {
    if (state->camera_component) {
        mmal_component_destroy(state->camera_component);
        state->camera_component = NULL;
    }
}

/**
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_encoder_component(CAM_STATE *state) {
    // Get rid of any port buffers first
    if (state->video_encoder_pool) {
        mmal_port_pool_destroy(state->video_encoder_component->output[0], state->video_encoder_pool);
    }

    if (state->video_encoder_component) {
        mmal_component_destroy(state->video_encoder_component);
        state->video_encoder_component = NULL;
    }
}

/**
 * todo
 * @param camera_num
 * @param camera_name
 * @param width
 * @param height
 */
void get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height) {
    MMAL_COMPONENT_T *camera_info;
    MMAL_STATUS_T status;

    // Default to the OV5647 setup
    strncpy(camera_name, "OV5647", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);

    // Try to get the camera name and maximum supported resolution
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
    if (status == MMAL_SUCCESS) {
        MMAL_PARAMETER_CAMERA_INFO_T param;
        param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
        param.hdr.size = sizeof(param) - 4;  // Deliberately undersize to check firmware version
        status = mmal_port_parameter_get(camera_info->control, &param.hdr);

        if (status != MMAL_SUCCESS) {
            // Running on newer firmware
            param.hdr.size = sizeof(param);
            status = mmal_port_parameter_get(camera_info->control, &param.hdr);
            if (status == MMAL_SUCCESS && param.num_cameras > camera_num) {
                // Take the parameters from the first camera listed.
                if (*width == 0)
                    *width = param.cameras[camera_num].max_width;
                if (*height == 0)
                    *height = param.cameras[camera_num].max_height;
                strncpy(camera_name, param.cameras[camera_num].camera_name, MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
                camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN - 1] = 0;
            } else
                vcos_log_error("Cannot read camera info, keeping the defaults for OV5647");
        } else {
            // Older firmware
            // Nothing to do here, keep the defaults for OV5647
        }

        mmal_component_destroy(camera_info);
    } else {
       vcos_log_error("Failed to create camera_info component");
    }

    // default to OV5647 if nothing detected..
    if (*width == 0)
        *width = 2592;
    if (*height == 0)
        *height = 1944;
}

/**
 * todo
 * @param cam_num
 */
void check_camera_model(int cam_num) {
    MMAL_COMPONENT_T *camera_info;
    MMAL_STATUS_T status;

    // Try to get the camera name
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
    if (status == MMAL_SUCCESS) {
        MMAL_PARAMETER_CAMERA_INFO_T param;
        param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
        param.hdr.size = sizeof(param) - 4;  // Deliberately undersize to check firmware version
        status = mmal_port_parameter_get(camera_info->control, &param.hdr);

        if (status != MMAL_SUCCESS) {
            // Running on newer firmware
            param.hdr.size = sizeof(param);
            status = mmal_port_parameter_get(camera_info->control, &param.hdr);
            if (status == MMAL_SUCCESS && param.num_cameras > cam_num) {
                if (!strncmp(param.cameras[cam_num].camera_name, "toshh2c", 7)) {
                    vcos_log_error( "The driver for the TC358743 HDMI to CSI2 chip you are using is NOT supported.\n");
                    vcos_log_error(
                            "They were written for a demo purposes only, and are in the firmware on an as-is\n");
                    vcos_log_error( "basis and therefore requests for support or changes will not be acted on.\n\n");
                }
            }
        }

        mmal_component_destroy(camera_info);
    }
}

/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection) {
    MMAL_STATUS_T status;

    status = mmal_connection_create(connection, output_port, input_port,
                                    MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

    if (status == MMAL_SUCCESS) {
        status = mmal_connection_enable(*connection);
        if (status != MMAL_SUCCESS)
            mmal_connection_destroy(*connection);
    }

    return status;
}

/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
void check_disable_port(MMAL_PORT_T *port) {
    if (port && port->is_enabled)
        mmal_port_disable(port);
}

/**
 * todo
 * @return
 */
uint64_t get_microseconds64() {
    struct timespec spec;
    uint64_t us;

    clock_gettime(CLOCK_MONOTONIC_RAW, &spec);

    us = spec.tv_sec * 1000000ULL;
    us += spec.tv_nsec / 1000;

    return us;
}

/**
 * Pause for specified time, but return early if detect an abort request
 *
 * @param state Pointer to state control struct
 * @param pause Time in ms to pause
 * @param callback Struct contain an abort flag tested for early termination
 *
 */
static int pause_and_test_abort(CAM_STATE *state, int pause) {
    int wait;

    if (!pause)
        return 0;

    // Going to check every ABORT_INTERVAL milliseconds
    for (wait = 0; wait < pause; wait += ABORT_INTERVAL) {
        vcos_sleep(ABORT_INTERVAL);
        if (state->callback_data.abort)
            return 1;
    }

    return 0;
}

/**
 * Zoom in and Zoom out by changing ROI
 * @param camera Pointer to camera component
 * @param zoom_command zoom command enum
 * @return 0 if successful, non-zero otherwise
 */
int zoom_in_zoom_out(MMAL_COMPONENT_T *camera, ZOOM_COMMAND_T zoom_command, PARAM_FLOAT_RECT_T *roi) {
    MMAL_PARAMETER_INPUT_CROP_T crop;
    crop.hdr.id = MMAL_PARAMETER_INPUT_CROP;
    crop.hdr.size = sizeof(crop);

    if (mmal_port_parameter_get(camera->control, &crop.hdr) != MMAL_SUCCESS) {
        vcos_log_error("mmal_port_parameter_get(camera->control, &crop.hdr) failed, skip it\n");
        return 0;
    }

    if (zoom_command == ZOOM_IN) {
        if (crop.rect.width <= (zoom_full_16P16 + zoom_increment_16P16)) {
            crop.rect.width = zoom_full_16P16;
            crop.rect.height = zoom_full_16P16;
        } else {
            crop.rect.width -= zoom_increment_16P16;
            crop.rect.height -= zoom_increment_16P16;
        }
    } else if (zoom_command == ZOOM_OUT) {
        unsigned int increased_size = crop.rect.width + zoom_increment_16P16;
        if (increased_size < crop.rect.width) //overflow
        {
            crop.rect.width = 65536;
            crop.rect.height = 65536;
        } else {
            crop.rect.width = increased_size;
            crop.rect.height = increased_size;
        }
    }

    if (zoom_command == ZOOM_RESET) {
        crop.rect.x = 0;
        crop.rect.y = 0;
        crop.rect.width = 65536;
        crop.rect.height = 65536;
    } else {
        unsigned int centered_top_coordinate = (65536 - crop.rect.width) / 2;
        crop.rect.x = centered_top_coordinate;
        crop.rect.y = centered_top_coordinate;
    }

    int ret = (mmal_port_parameter_set(camera->control, &crop.hdr));

    if (ret == 0) {
        roi->x = roi->y = (double) crop.rect.x / 65536;
        roi->w = roi->h = (double) crop.rect.width / 65536;
    } else {
        vcos_log_error("Failed to set crop values, x/y: %u, w/h: %u", crop.rect.x, crop.rect.width);
        ret = 1;
    }

    return ret;
}

/**
 * Function to wait in various ways (depending on settings)
 *
 * @param state Pointer to the state data
 *
 * @return !0 if to continue, 0 if reached end of run
 */
int wait_for_next_change(CAM_STATE *state) {
    int keep_running = 1;
    static int64_t complete_time = -1;

    // Have we actually exceeded our timeout?
    int64_t current_time = get_microseconds64() / 1000;

    if (complete_time == -1)
        complete_time = current_time + state->timeout;

    // if we have run out of time, flag we need to exit
    if (current_time >= complete_time && state->timeout != 0)
        keep_running = 0;

    switch (state->waitMethod) {
        case WAIT_METHOD_NONE:
            (void) pause_and_test_abort(state, state->timeout);
            return 0;

        case WAIT_METHOD_FOREVER: {
            // We never return from this. Expect a ctrl-c to exit or abort.
            while (!state->callback_data.abort)
                // Have a sleep so we don't hog the CPU.
                vcos_sleep(ABORT_INTERVAL);

            return 0;
        }

        case WAIT_METHOD_TIMED: {
            int abort;

            if (state->bCapturing)
                abort = pause_and_test_abort(state, state->onTime);
            else
                abort = pause_and_test_abort(state, state->offTime);

            if (abort)
                return 0;
            else
                return keep_running;
        }

        case WAIT_METHOD_KEYPRESS: {
            char ch;

            if (state->common_settings.verbose)
                vcos_log_info( "Press Enter to %s, X then ENTER to exit, [i,o,r] then ENTER to change zoom\n",
                        state->bCapturing ? "pause" : "capture");

            ch = getchar();
            if (ch == 'x' || ch == 'X')
                return 0;
            else if (ch == 'i' || ch == 'I') {
                vcos_log_error("Starting zoom in\n");

                zoom_in_zoom_out(state->camera_component, ZOOM_IN, &(state->camera_parameters).roi);
            } else if (ch == 'o' || ch == 'O') {
                vcos_log_error("Starting zoom out\n");

                zoom_in_zoom_out(state->camera_component, ZOOM_OUT, &(state->camera_parameters).roi);
            } else if (ch == 'r' || ch == 'R') {
                if (state->common_settings.verbose)
                    vcos_log_info( "starting reset zoom\n");

                zoom_in_zoom_out(state->camera_component, ZOOM_RESET, &(state->camera_parameters).roi);
            }

            return keep_running;
        }


        case WAIT_METHOD_SIGNAL: {
            // Need to wait for a SIGUSR1 signal
            sigset_t waitset;
            int sig;
            int result = 0;

            sigemptyset(&waitset);
            sigaddset(&waitset, SIGUSR1);

            // We are multi threaded because we use mmal, so need to use the pthread
            // variant of procmask to block SIGUSR1 so we can wait on it.
            pthread_sigmask(SIG_BLOCK, &waitset, NULL);

            if (state->common_settings.verbose) {
                vcos_log_info( "Waiting for SIGUSR1 to %s\n", state->bCapturing ? "pause" : "capture");
            }

            result = sigwait(&waitset, &sig);

            if (state->common_settings.verbose && result != 0)
                vcos_log_error( "Bad signal received - error %d\n", errno);

            return keep_running;
        }

    } // switch

    return keep_running;
}

/**
 * Start capturing video.
 * @param state
 */
MMAL_STATUS_T capture(CAM_STATE *state) {
    int running = 1;
    MMAL_STATUS_T status;

    int num = mmal_queue_length(state->video_encoder_pool->queue);
    int q;
    for (q=0; q<num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state->video_encoder_pool->queue);

        if (!buffer) {
            vcos_log_error("Unable to get a required buffer %d from pool queue", q);
            return MMAL_ENOSPC;
        }

        if (mmal_port_send_buffer(state->video_encoder_output_port, buffer)!= MMAL_SUCCESS) {
            vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
            return MMAL_ENOSPC;
        }
    }

    int initialCapturing = state->bCapturing;
    while (running) {
        // Change state
        state->bCapturing = !state->bCapturing;

        if ((status = mmal_port_parameter_set_boolean(state->camera_video_port,
                                                      MMAL_PARAMETER_CAPTURE, state->bCapturing) != MMAL_SUCCESS)) {
            vcos_log_error("failed to start capturing: %s\n", mmal_status_to_string(status));
            return status;
        }

        if(state->splitWait) {
            if(state->bCapturing) {
                if ((status = mmal_port_parameter_set_boolean(state->video_encoder_output_port,
                                                    MMAL_PARAMETER_VIDEO_REQUEST_I_FRAME, 1) != MMAL_SUCCESS)) {
                    vcos_log_error("failed to request I-FRAME");
                    return status;
                }
            }
            else {
                if(!initialCapturing)
                    state->splitNow=1;
            }
            initialCapturing=0;
        }
        // by default, will wait for timeout to have expired
        running = wait_for_next_change(state);
    }
}

/**
 * Function to wait in various ways (depending on settings) for the next frame
 *
 * @param state Pointer to the state data
 * @param [in][out] frame The last frame number, adjusted to next frame number on output
 * @return !0 if to continue, 0 if reached end of run
 */
int wait_for_next_frame(CAM_STATE *state, int *frame) {
    static int64_t complete_time = -1;
    int keep_running = 1;

    int64_t current_time =  get_microseconds64()/1000;

    if (complete_time == -1)
        complete_time =  current_time + state->timeout;

    // if we have run out of time, flag we need to exit
    // If timeout = 0 then always continue
    if (current_time >= complete_time && state->timeout != 0)
        keep_running = 0;

    switch (state->frameNextMethod)
    {
        case FRAME_NEXT_SINGLE :
            // simple timeout for a single capture
            vcos_sleep(state->timeout);
            return 0;

        case FRAME_NEXT_FOREVER :
        {
            *frame+=1;

            // Have a sleep so we don't hog the CPU.
            vcos_sleep(1000);

            // Run forever so never indicate end of loop
            return 1;
        }

        case FRAME_NEXT_TIMELAPSE :
        {
            static int64_t next_frame_ms = -1;

            // Always need to increment by at least one, may add a skip later
            *frame += 1;

            if (next_frame_ms == -1)
            {
                vcos_sleep(CAMERA_SETTLE_TIME);

                // Update our current time after the sleep
                current_time =  get_microseconds64()/1000;

                // Set our initial 'next frame time'
                next_frame_ms = current_time + state->timelapse;
            }
            else
            {
                int64_t this_delay_ms = next_frame_ms - current_time;

                if (this_delay_ms < 0)
                {
                    // We are already past the next exposure time
                    if (-this_delay_ms < state->timelapse/2)
                    {
                        // Less than a half frame late, take a frame and hope to catch up next time
                        next_frame_ms += state->timelapse;
                        vcos_log_info("Frame %d is %d ms late", *frame, (int)(-this_delay_ms));
                    }
                    else
                    {
                        int nskip = 1 + (-this_delay_ms)/state->timelapse;
                        vcos_log_info("Skipping frame %d to restart at frame %d", *frame, *frame+nskip);
                        *frame += nskip;
                        this_delay_ms += nskip * state->timelapse;
                        vcos_sleep(this_delay_ms);
                        next_frame_ms += (nskip + 1) * state->timelapse;
                    }
                }
                else
                {
                    vcos_sleep(this_delay_ms);
                    next_frame_ms += state->timelapse;
                }
            }

            return keep_running;
        }

        case FRAME_NEXT_KEYPRESS :
        {
            int ch;

            if (state->common_settings.verbose)
                vcos_log_info( "Press Enter to capture, X then ENTER to exit\n");

            ch = getchar();
            *frame+=1;
            if (ch == 'x' || ch == 'X')
                return 0;
            else
            {
                return keep_running;
            }
        }

        case FRAME_NEXT_IMMEDIATELY :
        {
            // Not waiting, just go to next frame.
            // Actually, we do need a slight delay here otherwise exposure goes
            // badly wrong since we never allow it frames to work it out
            // This could probably be tuned down.
            // First frame has a much longer delay to ensure we get exposure to a steady state
            if (*frame == 0)
                vcos_sleep(CAMERA_SETTLE_TIME);
            else
                vcos_sleep(30);

            *frame+=1;

            return keep_running;
        }

        case FRAME_NEXT_GPIO :
        {
            // Intended for GPIO firing of a capture
            return 0;
        }

        case FRAME_NEXT_SIGNAL :
        {
            // Need to wait for a SIGUSR1 or SIGUSR2 signal
            sigset_t waitset;
            int sig;
            int result = 0;

            sigemptyset( &waitset );
            sigaddset( &waitset, SIGUSR1 );
            sigaddset( &waitset, SIGUSR2 );

            // We are multi threaded because we use mmal, so need to use the pthread
            // variant of procmask to block until a SIGUSR1 or SIGUSR2 signal appears
            pthread_sigmask( SIG_BLOCK, &waitset, NULL );

            if (state->common_settings.verbose)
            {
                vcos_log_info( "Waiting for SIGUSR1 to initiate capture and continue or SIGUSR2 to capture and exit\n");
            }

            result = sigwait( &waitset, &sig );

            if (result == 0)
            {
                if (sig == SIGUSR1)
                {
                    if (state->common_settings.verbose)
                        vcos_log_info( "Received SIGUSR1\n");
                }
                else if (sig == SIGUSR2)
                {
                    if (state->common_settings.verbose)
                        vcos_log_info( "Received SIGUSR2\n");
                    keep_running = 0;
                }
            }
            else
            {
                if (state->common_settings.verbose)
                    vcos_log_error( "Bad signal received - error %d\n", errno);
            }

            *frame+=1;

            return keep_running;
        }
    } // end of switch

    // Should have returned by now, but default to timeout
    return keep_running;
}

/**
 * TODO
 * @param state
 * @return
 */
MMAL_STATUS_T capture_still(CAM_STATE *state, CAM_STILL_CB still_cb) {
    state->callback_data.mutex = 1;

    int frame, keep_looping = 1;
    MMAL_STATUS_T status;

    // Set up our userdata - this is passed though to the callback where we need the information.
    // Null until we open our filename
    state->callback_data.pstate = state;
    state->callback_data.still_cb = still_cb;

    // create the semaphore to indicate successful frame handling (the semaphore is
    // completed in the encoder buffer callback)
    if ((vcos_semaphore_create(&state->callback_data.complete_semaphore, "picam-sem", 0)) != VCOS_SUCCESS) {
        vcos_log_error("%s: failed to create semaphore", __func__);
    }

    while (keep_looping)
    {
        if (state->common_settings.verbose)
            vcos_log_info( "waiting for next frame\n");

        keep_looping = wait_for_next_frame(state, &frame);

        if (state->timestamp)
        {
            frame = (int)time(NULL);
        }

        // We only capture if a filename was specified and it opened
        {
            int num, q;

            if (state->common_settings.verbose)
                vcos_log_info( "disabling exif\n");
            mmal_port_parameter_set_boolean(
                    state->still_encoder_component->output[0], MMAL_PARAMETER_EXIF_DISABLE, 1);

            // Same with raw, apparently need to set it for each capture, whilst port
            // is not enabled
            if (state->wantRAW)
            {
                if (mmal_port_parameter_set_boolean(
                        state->camera_still_port, MMAL_PARAMETER_ENABLE_RAW_CAPTURE, 1) != MMAL_SUCCESS)
                {
                    vcos_log_error("RAW was requested, but failed to enable");
                }
            }

            // There is a possibility that shutter needs to be set each loop.
            if (state->common_settings.verbose)
                vcos_log_info( "setting shutter speed\n");
            if ((mmal_port_parameter_set_uint32(state->camera_component->control,
                                                                  MMAL_PARAMETER_SHUTTER_SPEED, state->camera_parameters.shutter_speed)) != MMAL_SUCCESS)
                vcos_log_error("Unable to set shutter speed");

            // Enable the encoder output port
            state->still_encoder_output_port->userdata = (struct MMAL_PORT_USERDATA_T *)&state->callback_data;

            if (state->common_settings.verbose)
                vcos_log_info( "Enabling encoder output port\n");

            // Enable the encoder output port and tell it its callback function
            status = mmal_port_enable(state->still_encoder_output_port, still_encoder_buffer_callback);

            // Send all the buffers to the encoder output port
            num = mmal_queue_length(state->encoder_pool->queue);

            for (q=0; q<num; q++)
            {
                MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state->encoder_pool->queue);

                if (!buffer)
                    vcos_log_error("Unable to get a required buffer %d from pool queue", q);

                if (mmal_port_send_buffer(state->still_encoder_output_port, buffer)!= MMAL_SUCCESS)
                    vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
            }

            if (state->burstCaptureMode)
            {
                mmal_port_parameter_set_boolean(
                        state->camera_component->control,  MMAL_PARAMETER_CAMERA_BURST_CAPTURE, 1);
            }

            if (state->common_settings.verbose)
                vcos_log_info( "Starting capture %d\n", frame);

            if (mmal_port_parameter_set_boolean(
                    state->camera_still_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS)
            {
                vcos_log_error("%s: Failed to start capture", __func__);
            }
            else
            {
                // Wait for capture to complete
                // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
                // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
                vcos_semaphore_wait(&state->callback_data.complete_semaphore);
                if (state->common_settings.verbose)
                    vcos_log_info( "Finished capture %d\n", frame);
            }

            // Disable encoder output port
            status = mmal_port_disable(state->still_encoder_output_port);
        }
    } // end for (frame)

    vcos_semaphore_delete(&state->callback_data.complete_semaphore);
    state->callback_data.capture_in_progress = 0;
}

/**
 * Initialise the camera.
 * @param state
 * @param camera_video_port
 * @param video_encoder_output_port
 * @param video_encoder_input_port
 * @param callback
 * @return
 */
MMAL_STATUS_T init(CAM_STATE *state) {
    MMAL_STATUS_T status;

    // Setup for sensor specific parameters, only set W/H settings if zero on entry
    get_sensor_defaults(state->common_settings.cameraNum, state->common_settings.camera_name,
                        &state->common_settings.width, &state->common_settings.height);

    check_camera_model(state->common_settings.cameraNum);

    if ((status = create_camera_component(state)) != MMAL_SUCCESS) {
        return status;
    }
    if ((status = create_encoder_component(state)) != MMAL_SUCCESS) {
        return status;
    }

    // connect components
    state->camera_video_port = state->camera_component->output[MMAL_CAMERA_VIDEO_PORT];
    state->video_encoder_input_port = state->video_encoder_component->input[0];
    state->video_encoder_output_port = state->video_encoder_component->output[0];

    // connect the camera's video port to the video_encoder's input port
    status = connect_ports(state->camera_video_port, state->video_encoder_input_port, &state->video_encoder_connection);
    if (status != MMAL_SUCCESS) {
        return status;
    }

    // Set up our userdata - this is passed though to the callback where we need the information.
    (state->video_encoder_output_port)->userdata = (struct MMAL_PORT_USERDATA_T *)&state->callback_data;
    // Enable the encoder output port and tell it its callback function
    status = mmal_port_enable(state->video_encoder_output_port, encoder_buffer_callback);

    if (status != MMAL_SUCCESS) {
        return status;
    }

    return MMAL_SUCCESS;
}

/**
 * Initialise the still camera.
 * @param state
 * @return
 */
MMAL_STATUS_T init_still(CAM_STATE *state) {
    MMAL_STATUS_T status;
    
    bcm_host_init();

    // Setup for sensor specific parameters
    get_sensor_defaults(state->common_settings.cameraNum, state->common_settings.camera_name,
                        &state->common_settings.width, &state->common_settings.height);

    // OK, we have a nice set of parameters. Now set up our components
    // We have three components. Camera, Preview and encoder.
    // Camera and encoder are different in stills/video, but preview
    // is the same so handed off to a separate module

    if ((status = create_still_camera_component(state)) != MMAL_SUCCESS) {
        vcos_log_error("%s: failed to create still camera component: %s", __func__, mmal_status_to_string(status));
        return status;
    }
    if ((status = preview_create(&state->preview_parameters)) != MMAL_SUCCESS) {
        vcos_log_error("%s: failed to create preview component: %s", __func__, mmal_status_to_string(status));
        destroy_camera_component(state);
        return status;
    }
    if ((status = create_still_encoder_component(state)) != MMAL_SUCCESS) {
        vcos_log_error("%s: failed to create still encoder component: %s", __func__, mmal_status_to_string(status));
        preview_destroy(&state->preview_parameters);
        destroy_camera_component(state);
        return status;
    }

    PORT_USERDATA callback_data;
    callback_data.image_data = NULL;
    callback_data.image_data_length = 0;
    callback_data._image_data = NULL;
    callback_data._image_data_length = 0;

    if (state->common_settings.verbose)
        vcos_log_info( "Starting component connection stage\n");

    state->preview_parameters.camera_preview_port = state->camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
    state->camera_video_port   = state->camera_component->output[MMAL_CAMERA_VIDEO_PORT];
    state->camera_still_port   = state->camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
    state->still_encoder_input_port  = state->still_encoder_component->input[0];
    state->still_encoder_output_port = state->still_encoder_component->output[0];

    if (state->common_settings.verbose)
        vcos_log_info( "Connecting camera preview port to video render.\n");

    // Note we are lucky that the preview and null sink components use the same input port
    // so we can simple do this without conditionals
    state->preview_parameters.camera_preview_input_port  = state->preview_parameters.preview_component->input[0];

    // Connect camera to preview (which might be a null_sink if no preview required)
    if ((status = connect_ports(state->preview_parameters.camera_preview_port,
                                state->preview_parameters.camera_preview_input_port, &state->preview_connection)) != MMAL_SUCCESS) {
        vcos_log_error("%s: failed to connect camera to preview: %s", __func__, mmal_status_to_string(status));
        return status;
    }

    if (state->common_settings.verbose)
        vcos_log_info( "connecting camera still port to encoder input port\n");

    // Now connect the camera to the encoder
    if ((status = connect_ports(state->camera_still_port,
                                state->still_encoder_input_port, &state->encoder_connection)) != MMAL_SUCCESS) {
        vcos_log_error("%s: failed to connect camera still port to encoder input: %s",
                       __func__, mmal_status_to_string(status));
        return status;
    }
}

/**
 * Destroys components and ports utilised for the camera.
 * @param state
 * @param video_encoder_output_port
 */
void destroy(CAM_STATE *state) {
    /* disable ports that are not handled by connections */
    check_disable_port(state->video_encoder_output_port);
    /* destroy connections */
    if (state->video_encoder_connection)
        mmal_connection_destroy(state->video_encoder_connection);
    /* disable components */
    if (state->video_encoder_component)
        mmal_component_disable(state->video_encoder_component);
    if (state->camera_component)
        mmal_component_disable(state->camera_component);
    /* destroy components */
    destroy_encoder_component(state);
    destroy_camera_component(state);
}

/**
 *  buffer header callback function for encoder
 *
 *  Callback will call provided state callbacks with each frame of buffer data.
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_BUFFER_HEADER_T *new_buffer;
    static int64_t base_time =  -1;

    // All our segment times based on the receipt of the first encoder callback
    if (base_time == -1)
        base_time = get_microseconds64()/1000;

    // We pass our file handle and other stuff in via the userdata field.
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    if (pData) {
        int bytes_written = buffer->length;
        int64_t current_time = get_microseconds64()/1000;

        if ((buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) &&
            ((pData->pstate->segmentSize && current_time > base_time + pData->pstate->segmentSize) ||
             (pData->pstate->splitWait && pData->pstate->splitNow))) {
            // increase segment??
            base_time = current_time;
            pData->pstate->splitNow = 0;
            pData->pstate->segmentNumber++;
            // Only wrap if we have a wrap point set
            if (pData->pstate->segmentWrap && pData->pstate->segmentNumber > pData->pstate->segmentWrap)
                pData->pstate->segmentNumber = 1;
        }

        if (buffer->length) { // only handle buffers with data
            // thread safety, perhaps?
            mmal_buffer_header_mem_lock(buffer);

            // deal with any 'side information' (only IMV's in our case)
            if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) {
                if(pData->pstate->inlineMotionVectors) {
                    vcos_log_info("*** IMV of length %i\n", buffer->length);
                } else {
                    bytes_written = buffer->length;
                }
            } else {
                /* a frame has ended */
                if((buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END || buffer->flags == 0 ||
                    /* is a keyframe (i.e., standalone) */
                    buffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME) &&
                   /* contains config data (codec data) */
                   !(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)) {
                    // must be specified time? and not the same time?
                    if(buffer->pts != MMAL_TIME_UNKNOWN && buffer->pts != pData->pstate->lasttime) {
                        int64_t pts;
                        if(pData->pstate->frame==0)pData->pstate->starttime=buffer->pts;
                        pData->pstate->lasttime=buffer->pts;
                        pts = buffer->pts - pData->pstate->starttime;

                        // callback to handle frame data
                        pData->video_cb(pts, buffer->data, buffer->length, buffer->offset);

                        // increase frame count
                        pData->pstate->frame++;
                    }
                }
            }

            mmal_buffer_header_mem_unlock(buffer);

            if (bytes_written != buffer->length) {
                vcos_log_error("Failed to write buffer data (%d from %d)- aborting", bytes_written, buffer->length);
                pData->abort = 1;
            }
        }
    } else {
        vcos_log_error("Received a encoder buffer callback with no state");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled)
    {
        MMAL_STATUS_T status;

        new_buffer = mmal_queue_get(pData->pstate->video_encoder_pool->queue);

        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);

        if (!new_buffer || status != MMAL_SUCCESS)
            vcos_log_error("Unable to return a buffer to the encoder port\n");
    }
}

/**
 *  buffer header callback function for encoder
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
void still_encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    int complete = 0;

    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    if (pData)
    {
        int bytes_written = buffer->length;

        if (buffer->length)
        {
            mmal_buffer_header_mem_lock(buffer);

            if (pData->_image_data == NULL) {
                // start a new image
                pData->_image_data = malloc(sizeof(uint8_t) * buffer->length);
                memcpy(pData->_image_data, buffer->data, buffer->length);
                pData->_image_data_length = buffer->length;
            } else {
                // continue building the current image
                pData->_image_data = realloc(pData->_image_data,
                        sizeof(uint8_t) * (buffer->length + pData->_image_data_length));
                memcpy(&(pData->_image_data)[pData->_image_data_length], buffer->data, buffer->length);
                pData->_image_data_length = buffer->length + pData->_image_data_length;
            }

            mmal_buffer_header_mem_unlock(buffer);
        }

        // We need to check we wrote what we wanted - it's possible we have run out of storage.
        if (bytes_written != buffer->length)
        {
            vcos_log_error("Unable to write buffer to file - aborting");
            complete = 1;
        }

        // Now flag if we have completed
        if (buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED))
            complete = 1;
    }
    else
    {
        vcos_log_error("Received a encoder buffer callback with no state");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled)
    {
        MMAL_STATUS_T status = MMAL_SUCCESS;
        MMAL_BUFFER_HEADER_T *new_buffer;

        new_buffer = mmal_queue_get(pData->pstate->encoder_pool->queue);

        if (new_buffer)
        {
            status = mmal_port_send_buffer(port, new_buffer);
        }
        if (!new_buffer || status != MMAL_SUCCESS)
            vcos_log_error("Unable to return a buffer to the encoder port");
    }

    if (complete) {
        if (pData->still_cb) {
            // call the callback with the completed image
            pData->still_cb(pData->_image_data, pData->_image_data_length);
            pData->image_data = malloc(sizeof(uint8_t) * pData->_image_data_length);
            memcpy(pData->image_data, pData->_image_data, pData->_image_data_length);
            pData->image_data_length = pData->_image_data_length;
            free(pData->_image_data);
            pData->_image_data = NULL;
            pData->_image_data_length = 0;
        } else {
            vcos_log_error("no still callback specified");
        }
        vcos_semaphore_post(&(pData->complete_semaphore));
    }
}

/**
 * Destroys all components of the still camera.
 * @param state
 */
void destroy_still(CAM_STATE* state) {
    if (state->common_settings.verbose)
        vcos_log_info( "Closing down\n");

    // Disable all our ports that are not handled by connections
    check_disable_port(state->camera_video_port);
    check_disable_port(state->still_encoder_output_port);

    if (state->preview_connection)
        mmal_connection_destroy(state->preview_connection);

    if (state->encoder_connection)
        mmal_connection_destroy(state->encoder_connection);

    /* Disable components */
    if (state->encoder_component)
        mmal_component_disable(state->encoder_component);

    if (state->preview_parameters.preview_component)
        mmal_component_disable(state->preview_parameters.preview_component);

    if (state->camera_component)
        mmal_component_disable(state->camera_component);

    destroy_encoder_component(state);
    preview_destroy(&state->preview_parameters);
    destroy_camera_component(state);
}
