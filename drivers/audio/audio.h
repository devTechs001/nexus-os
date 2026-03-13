/*
 * NEXUS OS - Audio Drivers
 * drivers/audio/audio.h
 *
 * Comprehensive audio driver support for AC'97, HDA, Intel HD Audio,
 * USB Audio, and SoundBlaster compatible devices
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _AUDIO_H
#define _AUDIO_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         AUDIO CONFIGURATION                               */
/*===========================================================================*/

#define AUDIO_MAX_DEVICES           32
#define AUDIO_MAX_CHANNELS          8
#define AUDIO_MAX_STREAMS           64
#define AUDIO_MAX_MIXER_CONTROLS    256
#define AUDIO_BUFFER_SIZE           (64 * 1024)
#define AUDIO_MAX_SAMPLE_RATE       192000
#define AUDIO_MIN_SAMPLE_RATE       8000

/*===========================================================================*/
/*                         AUDIO DEVICE TYPES                                */
/*===========================================================================*/

#define AUDIO_TYPE_UNKNOWN          0
#define AUDIO_TYPE_AC97             1
#define AUDIO_TYPE_HDA              2
#define AUDIO_TYPE_INTEL_HDA        3
#define AUDIO_TYPE_USB_AUDIO        4
#define AUDIO_TYPE_SOUNDBLASTER     5
#define AUDIO_TYPE_MIXER            6
#define AUDIO_TYPE_SPEAKER          7
#define AUDIO_TYPE_MICROPHONE       8
#define AUDIO_TYPE_LINE_IN          9
#define AUDIO_TYPE_LINE_OUT         10
#define AUDIO_TYPE_HEADPHONE        11
#define AUDIO_TYPE_SPDIF            12
#define AUDIO_TYPE_INPUT            13
#define AUDIO_TYPE_OUTPUT           14

/*===========================================================================*/
/*                         AUDIO FORMATS                                     */
/*===========================================================================*/

#define AUDIO_FORMAT_UNKNOWN        0
#define AUDIO_FORMAT_S8             1   /* Signed 8-bit */
#define AUDIO_FORMAT_U8             2   /* Unsigned 8-bit */
#define AUDIO_FORMAT_S16_LE         3   /* Signed 16-bit Little Endian */
#define AUDIO_FORMAT_S16_BE         4   /* Signed 16-bit Big Endian */
#define AUDIO_FORMAT_S24_LE         5   /* Signed 24-bit Little Endian */
#define AUDIO_FORMAT_S24_BE         6   /* Signed 24-bit Big Endian */
#define AUDIO_FORMAT_S32_LE         7   /* Signed 32-bit Little Endian */
#define AUDIO_FORMAT_S32_BE         8   /* Signed 32-bit Big Endian */
#define AUDIO_FORMAT_FLOAT32        9   /* 32-bit Float */
#define AUDIO_FORMAT_FLOAT64        10  /* 64-bit Float */
#define AUDIO_FORMAT_AC3            11  /* AC-3 */
#define AUDIO_FORMAT_DTS            12  /* DTS */
#define AUDIO_FORMAT_AAC            13  /* AAC */
#define AUDIO_FORMAT_MP3            14  /* MP3 */

/*===========================================================================*/
/*                         AUDIO CHANNELS                                    */
/*===========================================================================*/

#define AUDIO_CHANNEL_UNKNOWN       0
#define AUDIO_CHANNEL_MONO          1
#define AUDIO_CHANNEL_STEREO        2
#define AUDIO_CHANNEL_2_1           3   /* Stereo + LFE */
#define AUDIO_CHANNEL_4_0           4   /* Quad */
#define AUDIO_CHANNEL_5_0           5   /* Surround */
#define AUDIO_CHANNEL_5_1           6   /* 5.1 Surround */
#define AUDIO_CHANNEL_7_1           7   /* 7.1 Surround */

/*===========================================================================*/
/*                         AUDIO STREAM TYPES                                */
/*===========================================================================*/

#define AUDIO_STREAM_PLAYBACK       1
#define AUDIO_STREAM_CAPTURE        2
#define AUDIO_STREAM_BIDIRECTIONAL  3

/*===========================================================================*/
/*                         AUDIO STATES                                      */
/*===========================================================================*/

#define AUDIO_STATE_OPEN            0
#define AUDIO_STATE_SETUP           1
#define AUDIO_STATE_PREPARED        2
#define AUDIO_STATE_RUNNING         3
#define AUDIO_STATE_PAUSED          4
#define AUDIO_STATE_SUSPENDED       5
#define AUDIO_STATE_XRUN            6
#define AUDIO_STATE_DRAINING        7
#define AUDIO_STATE_STOPPED         8
#define AUDIO_STATE_CLOSED          9

/*===========================================================================*/
/*                         HDA WIDGET TYPES                                  */
/*===========================================================================*/

#define HDA_WIDGET_AUDIO_OUTPUT     0x00
#define HDA_WIDGET_AUDIO_INPUT      0x01
#define HDA_WIDGET_AUDIO_MIXER      0x02
#define HDA_WIDGET_AUDIO_SELECTOR   0x03
#define HDA_WIDGET_PIN_COMPLEX      0x04
#define HDA_WIDGET_POWER            0x05
#define HDA_WIDGET_VOLUME_KNOB      0x06
#define HDA_WIDGET_BEEP             0x07
#define HDA_WIDGET_VENDOR           0x0F

/*===========================================================================*/
/*                         PIN CONFIGURATION                                 */
/*===========================================================================*/

#define HDA_PIN_CONFIG_UNKNOWN      0x00
#define HDA_PIN_CONFIG_LINE_OUT     0x01
#define HDA_PIN_CONFIG_SPEAKER      0x02
#define HDA_PIN_CONFIG_HEADPHONE    0x03
#define HDA_PIN_CONFIG_CD           0x04
#define HDA_PIN_CONFIG_SPDIF_OUT    0x05
#define HDA_PIN_CONFIG_DIGITAL_OUT  0x06
#define HDA_PIN_CONFIG_MODEM_LINE   0x07
#define HDA_PIN_CONFIG_MODEM_HANDSET 0x08
#define HDA_PIN_CONFIG_LINE_IN      0x09
#define HDA_PIN_CONFIG_AUX          0x0A
#define HDA_PIN_CONFIG_MIC          0x0B
#define HDA_PIN_CONFIG_TELEPHONY    0x0C
#define HDA_PIN_CONFIG_SPDIF_IN     0x0D
#define HDA_PIN_CONFIG_DIGITAL_IN   0x0E

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Audio Format Info
 */
typedef struct {
    u32 format;                     /* Audio Format */
    u32 sample_rate;                /* Sample Rate (Hz) */
    u32 channels;                   /* Channel Count */
    u32 bits_per_sample;            /* Bits Per Sample */
    u32 bytes_per_frame;            /* Bytes Per Frame */
    u32 bytes_per_second;           /* Bytes Per Second */
    u32 period_size;                /* Period Size (frames) */
    u32 period_count;               /* Period Count */
    u32 buffer_size;                /* Buffer Size (bytes) */
} audio_format_t;

/**
 * Audio Device Info
 */
typedef struct {
    u32 device_id;                  /* Device ID */
    char name[128];                 /* Device Name */
    char card_name[64];             /* Card Name */
    char codec_name[64];            /* Codec Name */
    u32 type;                       /* Device Type */
    u32 vendor_id;                  /* Vendor ID */
    u32 device_id_pci;              /* PCI Device ID */
    u32 subsystem_id;               /* Subsystem ID */
    u32 revision;                   /* Revision */
    bool is_connected;              /* Is Connected */
    bool is_default;                /* Is Default */
    bool is_active;                 /* Is Active */
    u32 capabilities;               /* Capabilities */
    audio_format_t format;          /* Current Format */
    void *private_data;             /* Driver Private Data */
} audio_device_t;

/**
 * HDA Codec Info
 */
typedef struct {
    u32 codec_id;                   /* Codec ID */
    u32 vendor_id;                  /* Vendor ID */
    u32 revision_id;                /* Revision ID */
    u32 stepping_id;                /* Stepping ID */
    char name[64];                  /* Codec Name */
    u32 widget_count;               /* Widget Count */
    u32 audio_groups;               /* Audio Groups */
    u32 beep_generator;             /* Beep Generator */
    u32 power_state;                /* Power State */
} hda_codec_t;

/**
 * HDA Widget Info
 */
typedef struct {
    u32 widget_id;                  /* Widget ID */
    u32 type;                       /* Widget Type */
    u32 capabilities;               /* Capabilities */
    u32 connections;                /* Connection Count */
    u32 *connection_list;           /* Connection List */
    u32 pin_config;                 /* Pin Configuration */
    u32 pin_caps;                   /* Pin Capabilities */
    u32 amp_in_caps;                /* Amp In Capabilities */
    u32 amp_out_caps;               /* Amp Out Capabilities */
    u32 volume_steps;               /* Volume Steps */
    u32 mute;                       /* Mute State */
    u32 gain;                       /* Gain */
} hda_widget_t;

/**
 * Audio Stream
 */
typedef struct {
    u32 stream_id;                  /* Stream ID */
    u32 device_id;                  /* Device ID */
    u32 type;                       /* Stream Type */
    u32 state;                      /* Stream State */
    audio_format_t format;          /* Stream Format */
    void *buffer;                   /* Buffer */
    phys_addr_t phys_addr;          /* Physical Address */
    u32 buffer_size;                /* Buffer Size */
    u32 period_size;                /* Period Size */
    u32 period_pos;                 /* Current Period Position */
    u32 hw_ptr;                     /* Hardware Pointer */
    u32 app_ptr;                    /* Application Pointer */
    u32 available;                  /* Available Frames */
    u32 silence_start;              /* Silence Start */
    u32 silence_count;              /* Silence Count */
    void (*on_complete)(void *);
    void (*on_period)(void *);
    void *user_data;                /* User Data */
} audio_stream_t;

/**
 * Mixer Control Info
 */
typedef struct {
    u32 control_id;                 /* Control ID */
    char name[64];                  /* Control Name */
    u32 type;                       /* Control Type */
    u32 access;                     /* Access Flags */
    s32 min;                        /* Minimum Value */
    s32 max;                        /* Maximum Value */
    s32 step;                       /* Step Size */
    s32 value;                      /* Current Value */
    s32 value_min;                  /* Current Value Min (stereo) */
    s32 value_max;                  /* Current Value Max (stereo) */
    bool muted;                     /* Muted */
    u32 channel;                    /* Channel */
} audio_mixer_control_t;

/**
 * Audio Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    audio_device_t devices[AUDIO_MAX_DEVICES]; /* Devices */
    u32 device_count;               /* Device Count */
    u32 default_output;             /* Default Output Device */
    u32 default_input;              /* Default Input Device */
    audio_stream_t streams[AUDIO_MAX_STREAMS]; /* Streams */
    u32 stream_count;               /* Stream Count */
    audio_mixer_control_t mixer_controls[AUDIO_MAX_MIXER_CONTROLS]; /* Mixer Controls */
    u32 mixer_control_count;        /* Mixer Control Count */
    u32 master_volume;              /* Master Volume (0-100) */
    u32 pcm_volume;                 /* PCM Volume (0-100) */
    u32 mic_volume;                 /* Mic Volume (0-100) */
    u32 line_volume;                /* Line Volume (0-100) */
    bool master_muted;              /* Master Muted */
    bool pcm_muted;                 /* PCM Muted */
    bool mic_muted;                 /* Mic Muted */
    bool line_muted;                /* Line Muted */
    hda_codec_t hda_codecs[8];     /* HDA Codecs */
    u32 hda_codec_count;            /* HDA Codec Count */
    hda_widget_t hda_widgets[256]; /* HDA Widgets */
    u32 hda_widget_count;           /* HDA Widget Count */
    u32 sample_rate;                /* Current Sample Rate */
    u32 channels;                   /* Current Channels */
    u32 format;                     /* Current Format */
} audio_driver_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Driver initialization */
int audio_init(void);
int audio_shutdown(void);
bool audio_is_initialized(void);

/* Device management */
int audio_enumerate_devices(void);
audio_device_t *audio_get_device(u32 device_id);
audio_device_t *audio_get_default_output(void);
audio_device_t *audio_get_default_input(void);
int audio_set_default_output(u32 device_id);
int audio_set_default_input(u32 device_id);
int audio_list_devices(audio_device_t *devices, u32 *count);
int audio_get_device_count(void);

/* Device control */
int audio_device_set_volume(u32 device_id, u32 volume);
int audio_device_get_volume(u32 device_id);
int audio_device_set_mute(u32 device_id, bool mute);
int audio_device_is_muted(u32 device_id);
int audio_device_get_info(u32 device_id, audio_device_t *info);

/* Stream management */
audio_stream_t *audio_create_stream(u32 device_id, u32 type, audio_format_t *format);
int audio_destroy_stream(u32 stream_id);
int audio_prepare_stream(u32 stream_id);
int audio_start_stream(u32 stream_id);
int audio_stop_stream(u32 stream_id);
int audio_pause_stream(u32 stream_id);
int audio_resume_stream(u32 stream_id);
int audio_drain_stream(u32 stream_id);
int audio_drop_stream(u32 stream_id);

/* Stream I/O */
int audio_write_stream(u32 stream_id, const void *data, u32 size);
int audio_read_stream(u32 stream_id, void *data, u32 size);
int audio_get_stream_position(u32 stream_id, u32 *pos);
int audio_set_stream_position(u32 stream_id, u32 pos);
int audio_get_stream_delay(u32 stream_id, u32 *delay);

/* HDA Codec functions */
int audio_hda_init_codec(u32 codec_id);
int audio_hda_get_codec_info(u32 codec_id, hda_codec_t *codec);
int audio_hda_get_widget_info(u32 widget_id, hda_widget_t *widget);
int audio_hda_set_pin_control(u32 widget_id, u32 control);
int audio_hda_get_pin_control(u32 widget_id, u32 *control);
int audio_hda_set_amp_gain(u32 widget_id, u32 gain, bool mute);
int audio_hda_get_amp_gain(u32 widget_id, u32 *gain, bool *mute);

/* Mixer functions */
int audio_get_mixer_controls(audio_mixer_control_t *controls, u32 *count);
int audio_get_mixer_control(const char *name, audio_mixer_control_t *control);
int audio_set_mixer_control(u32 control_id, s32 value);
int audio_get_mixer_control_value(u32 control_id, s32 *value);
int audio_set_mixer_mute(u32 control_id, bool mute);
int audio_get_mixer_mute(u32 control_id, bool *mute);

/* Master control */
int audio_set_master_volume(u32 volume);
int audio_get_master_volume(void);
int audio_set_master_mute(bool mute);
int audio_is_master_muted(void);
int audio_set_pcm_volume(u32 volume);
int audio_get_pcm_volume(void);
int audio_set_mic_volume(u32 volume);
int audio_get_mic_volume(void);

/* Format conversion */
int audio_convert_format(void *dst, const void *src, u32 samples, 
                         u32 src_format, u32 dst_format);
int audio_resample(void *dst, const void *src, u32 src_rate, u32 dst_rate,
                   u32 channels, u32 frames);
int audio_mix_buffers(void *dst, const void *src, u32 samples, u32 volume);

/* Utility functions */
const char *audio_get_type_name(u32 type);
const char *audio_get_format_name(u32 format);
const char *audio_get_state_name(u32 state);
u32 audio_get_format_bpp(u32 format);
u32 audio_get_channels_count(u32 channels);

/* Global instance */
audio_driver_t *audio_get_driver(void);

#endif /* _AUDIO_H */
