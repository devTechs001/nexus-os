/*
 * NEXUS OS - Audio Driver
 * drivers/audio/audio.h
 *
 * Audio driver with ALSA/PulseAudio support
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _AUDIO_H
#define _AUDIO_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         AUDIO CONFIGURATION                               */
/*===========================================================================*/

#define AUDIO_MAX_DEVICES           16
#define AUDIO_MAX_CHANNELS          8
#define AUDIO_MAX_SAMPLE_RATE       192000
#define AUDIO_MIN_SAMPLE_RATE       8000
#define AUDIO_MAX_VOLUME            100

/*===========================================================================*/
/*                         AUDIO DEVICE TYPES                                */
/*===========================================================================*/

#define AUDIO_TYPE_OUTPUT           0
#define AUDIO_TYPE_INPUT            1
#define AUDIO_TYPE_MIXER            2

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Audio Device Info
 */
typedef struct {
    u32 device_id;                  /* Device ID */
    char name[128];                 /* Device Name */
    char card_name[64];             /* Card Name */
    u32 type;                       /* Device Type */
    u32 vendor_id;                  /* Vendor ID */
    u32 device_id_pci;              /* Device ID */
    bool is_connected;              /* Is Connected */
    bool is_default;                /* Is Default */
    u32 sample_rate;                /* Sample Rate */
    u32 channels;                   /* Channels */
    u32 format;                     /* Format */
    u32 volume;                     /* Volume (0-100) */
    bool is_muted;                  /* Is Muted */
    void *driver_data;              /* Driver Data */
} audio_device_t;

/**
 * Audio Stream
 */
typedef struct {
    u32 stream_id;                  /* Stream ID */
    u32 device_id;                  /* Device ID */
    bool is_playing;                /* Is Playing */
    bool is_recording;              /* Is Recording */
    u32 sample_rate;                /* Sample Rate */
    u32 channels;                   /* Channels */
    u32 buffer_size;                /* Buffer Size */
    void *buffer;                   /* Buffer */
    void (*on_complete)(void *);
    void *user_data;                /* User Data */
} audio_stream_t;

/**
 * Audio Driver State
 */
typedef struct {
    bool initialized;               /* Is Initialized */
    audio_device_t devices[AUDIO_MAX_DEVICES]; /* Devices */
    u32 device_count;               /* Device Count */
    u32 default_output;             /* Default Output */
    u32 default_input;              /* Default Input */
    audio_stream_t streams[32];     /* Streams */
    u32 stream_count;               /* Stream Count */
    u32 master_volume;              /* Master Volume */
    bool master_muted;              /* Master Muted */
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

/* Device control */
int audio_device_set_volume(u32 device_id, u32 volume);
int audio_device_get_volume(u32 device_id);
int audio_device_set_mute(u32 device_id, bool mute);
int audio_device_is_muted(u32 device_id);

/* Stream management */
audio_stream_t *audio_create_stream(u32 device_id, u32 sample_rate, u32 channels);
int audio_destroy_stream(u32 stream_id);
int audio_play_stream(u32 stream_id, const void *data, u32 size);
int audio_stop_stream(u32 stream_id);
int audio_pause_stream(u32 stream_id);
int audio_resume_stream(u32 stream_id);

/* Master control */
int audio_set_master_volume(u32 volume);
int audio_get_master_volume(void);
int audio_set_master_mute(bool mute);
int audio_is_master_muted(void);

/* Utility functions */
const char *audio_get_type_name(u32 type);
const char *audio_get_format_name(u32 format);

/* Global instance */
audio_driver_t *audio_get_driver(void);

#endif /* _AUDIO_H */
