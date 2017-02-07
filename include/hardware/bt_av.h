/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_INCLUDE_BT_AV_H
#define ANDROID_INCLUDE_BT_AV_H

#include <vector>

#include <hardware/bluetooth.h>

__BEGIN_DECLS

/* Bluetooth AV connection states */
typedef enum {
    BTAV_CONNECTION_STATE_DISCONNECTED = 0,
    BTAV_CONNECTION_STATE_CONNECTING,
    BTAV_CONNECTION_STATE_CONNECTED,
    BTAV_CONNECTION_STATE_DISCONNECTING
} btav_connection_state_t;

/* Bluetooth AV datapath states */
typedef enum {
    BTAV_AUDIO_STATE_REMOTE_SUSPEND = 0,
    BTAV_AUDIO_STATE_STOPPED,
    BTAV_AUDIO_STATE_STARTED,
} btav_audio_state_t;

/*
 * Enum values for each A2DP supported codec.
 * There should be a separate entry for each A2DP codec that is supported
 * for encoding (SRC), and for decoding purpose (SINK).
 */
typedef enum {
  BTAV_A2DP_CODEC_INDEX_SOURCE_MIN = 0,

  // Add an entry for each source codec here.
  // NOTE: The values should be same as those listed in the following file:
  //   BluetoothCodecConfig.java
  BTAV_A2DP_CODEC_INDEX_SOURCE_SBC = 0,
  BTAV_A2DP_CODEC_INDEX_SOURCE_AAC,
  BTAV_A2DP_CODEC_INDEX_SOURCE_APTX,
  BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_HD,
  BTAV_A2DP_CODEC_INDEX_SOURCE_LDAC,

  BTAV_A2DP_CODEC_INDEX_SOURCE_MAX,

  BTAV_A2DP_CODEC_INDEX_SINK_MIN = BTAV_A2DP_CODEC_INDEX_SOURCE_MAX,

  // Add an entry for each sink codec here
  BTAV_A2DP_CODEC_INDEX_SINK_SBC = BTAV_A2DP_CODEC_INDEX_SINK_MIN,

  BTAV_A2DP_CODEC_INDEX_SINK_MAX,

  BTAV_A2DP_CODEC_INDEX_MIN = BTAV_A2DP_CODEC_INDEX_SOURCE_MIN,
  BTAV_A2DP_CODEC_INDEX_MAX = BTAV_A2DP_CODEC_INDEX_SINK_MAX
} btav_a2dp_codec_index_t;

typedef enum {
  // Disable the codec.
  // NOTE: This value can be used only during initialization when
  // function btav_source_interface_t::init() is called.
  BTAV_A2DP_CODEC_PRIORITY_DISABLED = -1,

  // Reset the codec priority to its default value.
  BTAV_A2DP_CODEC_PRIORITY_DEFAULT = 0,

  // Highest codec priority.
  BTAV_A2DP_CODEC_PRIORITY_HIGHEST = 1000 * 1000
} btav_a2dp_codec_priority_t;

typedef enum {
  BTAV_A2DP_CODEC_SAMPLE_RATE_NONE   = 0x0,
  BTAV_A2DP_CODEC_SAMPLE_RATE_44100  = 0x1 << 0,
  BTAV_A2DP_CODEC_SAMPLE_RATE_48000  = 0x1 << 1,
  BTAV_A2DP_CODEC_SAMPLE_RATE_88200  = 0x1 << 2,
  BTAV_A2DP_CODEC_SAMPLE_RATE_96000  = 0x1 << 3,
  BTAV_A2DP_CODEC_SAMPLE_RATE_176400 = 0x1 << 4,
  BTAV_A2DP_CODEC_SAMPLE_RATE_192000 = 0x1 << 5
} btav_a2dp_codec_sample_rate_t;

typedef enum {
  BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE = 0x0,
  BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16   = 0x1 << 0,
  BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24   = 0x1 << 1,
  BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32   = 0x1 << 2
} btav_a2dp_codec_bits_per_sample_t;

typedef enum {
  BTAV_A2DP_CODEC_CHANNEL_MODE_NONE   = 0x0,
  BTAV_A2DP_CODEC_CHANNEL_MODE_MONO   = 0x1 << 0,
  BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO = 0x1 << 1
} btav_a2dp_codec_channel_mode_t;

/*
 * Structure for representing codec capability or configuration.
 * It is used for configuring A2DP codec preference, and for reporting back
 * current configuration or codec capability.
 * For codec capability, fields "sample_rate", "bits_per_sample" and
 * "channel_mode" can contain bit-masks with all supported features.
 */
typedef struct {
  btav_a2dp_codec_index_t codec_type;
  btav_a2dp_codec_priority_t codec_priority; // Codec selection priority
                                // relative to other codecs: larger value
                                // means higher priority. If 0, reset to
                                // default.
  btav_a2dp_codec_sample_rate_t sample_rate;
  btav_a2dp_codec_bits_per_sample_t bits_per_sample;
  btav_a2dp_codec_channel_mode_t channel_mode;
  int64_t codec_specific_1;     // Codec-specific value 1
  int64_t codec_specific_2;     // Codec-specific value 2
  int64_t codec_specific_3;     // Codec-specific value 3
  int64_t codec_specific_4;     // Codec-specific value 4
} btav_a2dp_codec_config_t;

/** Callback for connection state change.
 *  state will have one of the values from btav_connection_state_t
 */
typedef void (* btav_connection_state_callback)(btav_connection_state_t state,
                                                    bt_bdaddr_t *bd_addr);

/** Callback for audiopath state change.
 *  state will have one of the values from btav_audio_state_t
 */
typedef void (* btav_audio_state_callback)(btav_audio_state_t state,
                                               bt_bdaddr_t *bd_addr);

/** Callback for audio configuration change.
 *  Used only for the A2DP Source interface.
 */
typedef void (* btav_audio_source_config_callback)(
    btav_a2dp_codec_config_t codec_config,
    std::vector<btav_a2dp_codec_config_t> codecs_local_capabilities,
    std::vector<btav_a2dp_codec_config_t> codecs_selectable_capabilities);

/** Callback for audio configuration change.
 *  Used only for the A2DP Sink interface.
 *  sample_rate: sample rate in Hz
 *  channel_count: number of channels (1 for mono, 2 for stereo)
 */
typedef void (* btav_audio_sink_config_callback)(bt_bdaddr_t *bd_addr,
                                                 uint32_t sample_rate,
                                                 uint8_t channel_count);

/** BT-AV A2DP Source callback structure. */
typedef struct {
    /** set to sizeof(btav_source_callbacks_t) */
    size_t      size;
    btav_connection_state_callback  connection_state_cb;
    btav_audio_state_callback audio_state_cb;
    btav_audio_source_config_callback audio_config_cb;
} btav_source_callbacks_t;

/** BT-AV A2DP Sink callback structure. */
typedef struct {
    /** set to sizeof(btav_sink_callbacks_t) */
    size_t      size;
    btav_connection_state_callback  connection_state_cb;
    btav_audio_state_callback audio_state_cb;
    btav_audio_sink_config_callback audio_config_cb;
} btav_sink_callbacks_t;

/**
 * NOTE:
 *
 * 1. AVRCP 1.0 shall be supported initially. AVRCP passthrough commands
 *    shall be handled internally via uinput
 *
 * 2. A2DP data path shall be handled via a socket pipe between the AudioFlinger
 *    android_audio_hw library and the Bluetooth stack.
 *
 */

/** Represents the standard BT-AV A2DP Source interface.
 */
typedef struct {

    /** set to sizeof(btav_source_interface_t) */
    size_t          size;
    /**
     * Register the BtAv callbacks.
     */
    bt_status_t (*init)(btav_source_callbacks_t* callbacks,
                std::vector<btav_a2dp_codec_config_t> codec_priorities);

    /** connect to headset */
    bt_status_t (*connect)( bt_bdaddr_t *bd_addr );

    /** dis-connect from headset */
    bt_status_t (*disconnect)( bt_bdaddr_t *bd_addr );

    /** configure the codecs settings preferences */
    bt_status_t (*config_codec)(std::vector<btav_a2dp_codec_config_t> codec_preferences);

    /** Closes the interface. */
    void  (*cleanup)( void );

} btav_source_interface_t;

/** Represents the standard BT-AV A2DP Sink interface.
 */
typedef struct {

    /** set to sizeof(btav_sink_interface_t) */
    size_t          size;
    /**
     * Register the BtAv callbacks
     */
    bt_status_t (*init)( btav_sink_callbacks_t* callbacks );

    /** connect to headset */
    bt_status_t (*connect)( bt_bdaddr_t *bd_addr );

    /** dis-connect from headset */
    bt_status_t (*disconnect)( bt_bdaddr_t *bd_addr );

    /** Closes the interface. */
    void  (*cleanup)( void );

    /** Sends Audio Focus State. */
    void  (*set_audio_focus_state)( int focus_state );

    /** Sets the audio track gain. */
    void  (*set_audio_track_gain)( float gain );
} btav_sink_interface_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_BT_AV_H */
