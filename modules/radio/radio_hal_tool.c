/*
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "radio_hal_tool"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <log/log.h>

#include <hardware/hardware.h>
#include <hardware/radio.h>
#include <system/radio.h>
#include <system/radio_metadata.h>

// Global state variables.
const struct radio_tuner *hal_tuner = NULL;

void usage() {
    printf("Usage: "
            "./radio_hal_tool -l\n"
            "-l: List properties global to the Radio.\n"
    );
}

void list_all_properties(radio_hw_device_t *device) {
    radio_hal_properties_t hal_properties;
    device->get_properties(device, &hal_properties);
    printf("Class: %d\n"
           "Impl: %s\n"
           "Tuners: %d\n"
           "Bands: %d\n\n",
           hal_properties.class_id, hal_properties.implementor, hal_properties.num_tuners,
           hal_properties.num_bands);

    uint32_t i;
    for (i = 0; i < hal_properties.num_bands; i++) {
        printf("Band Information\n"
               "Type: %d\n"
               "Connected: %d\n"
               "Lower limit: %d\n"
               "Upper limit: %d\n"
               "Spacing: %d\n\n",
               hal_properties.bands[i].type,
               hal_properties.bands[i].antenna_connected,
               hal_properties.bands[i].lower_limit,
               hal_properties.bands[i].upper_limit,
               hal_properties.bands[i].num_spacings);
    }
}

void callback(radio_hal_event_t *event, void *cookie) {
    printf("\nEvent detected\n"
           "Type: %d\n", event->type);
}

void tune(radio_hw_device_t *device, int band_number) {
    int ret;
    radio_hal_properties_t hal_properties;
    ret = device->get_properties(device, &hal_properties);
    if (ret != 0) {
        printf("Err: get_properties returned: %d\n", ret);
        return;
    }

    if ((uint32_t) band_number >= hal_properties.num_bands) {
        printf("Tuner number range should be: [0, %d]\n", hal_properties.num_bands);
    }
    printf("Setting band config as:\n"
           "Type: %d\n"
           "Connected: %d\n"
           "Lower limit: %d\n"
           "Upper limit: %d\n"
           "Spacing: %d\n\n",
           hal_properties.bands[band_number].type,
           hal_properties.bands[band_number].antenna_connected,
           hal_properties.bands[band_number].lower_limit,
           hal_properties.bands[band_number].upper_limit,
           hal_properties.bands[band_number].num_spacings);
    int cookie = 0;
    ret = device->open_tuner(
        device, (const radio_hal_band_config_t *) (&(hal_properties.bands[band_number])), false,
        callback, &cookie, &hal_tuner);
    if (ret != 0) {
        printf("Err: open_tuner returned: %d\n", ret);
        return;
    }
    // It takes some time to apply the config which is currently set as 500ms in
    // the stub implementation.
    sleep(1);

    // Stub tuner implementation will regard this magic channel as a valid channel to tune.
    ret = hal_tuner->tune(hal_tuner, 87916, 0);
    if (ret != 0) {
        printf("Err: tune returned: %d\n", ret);
        return;
    }
    // In the stub implementation it takes ~100ms to tune to the channel and the
    // data is set rightafter.
    sleep(1);
}

void get_tuner_metadata(radio_hw_device_t *device) {
    // Get the metadata and print it.
    radio_program_info_t info;
    radio_metadata_allocate(&info.metadata, 87916, 0);
    int ret;
    ret = hal_tuner->get_program_information(hal_tuner, &info);
    if (ret != 0) {
        printf("Err: Get program info ret code: %d\n", ret);
        return;
    }

    // Print the info.
    printf("Metadata from the band\n");
    int i;
    for (i = 0; i < radio_metadata_get_count(info.metadata); i++) {
        radio_metadata_key_t key;
        radio_metadata_type_t type;
        void *value;
        size_t size;

        radio_metadata_get_at_index(info.metadata, i, &key, &type, &value, &size);

        printf("\nMetadata key: %d\n"
               "Type: %d\n", key, type);

        switch (type) {
            case RADIO_METADATA_TYPE_INT:
                printf("Int value: %d\n", *((int *) value));
                break;
            case RADIO_METADATA_TYPE_TEXT:
                printf("Text value: %s\n", (char *) value);
                break;
            case RADIO_METADATA_TYPE_RAW:
                printf("Raw value, skipping\n");
                break;
            case RADIO_METADATA_TYPE_CLOCK:
                printf("UTC Epoch: %lld\n"
                       "UTC Offset: %d\n",
                       (long long)((radio_metadata_clock_t *) value)->utc_seconds_since_epoch,
                       ((radio_metadata_clock_t *) value)->timezone_offset_in_minutes);
        }
    }

    // Close the tuner when we are done.
    ret = device->close_tuner(device, hal_tuner);
    if (ret != 0) {
        printf("Err: close_tuner returned: %d\n", ret);
    }
}

int main(int argc, char** argv) {
    // Open the radio module and just ask for the list of properties.
    const hw_module_t *hw_module = NULL;
    int rc;
    rc = hw_get_module_by_class(RADIO_HARDWARE_MODULE_ID, RADIO_HARDWARE_MODULE_ID_FM, &hw_module);
    if (rc != 0) {
        printf("Cannot open the hw module. Does the HAL exist? %d\n", rc);
        return -1;
    }

    radio_hw_device_t *dev;
    rc = radio_hw_device_open(hw_module, &dev);
    if (rc != 0) {
        printf("Cannot open the device. Check that HAL implementation. %d\n", rc);
        return -1;
    }
    printf("HAL Loaded!\n");

    // If this is a list properties command - we check for -l command.
    int list_properties = 0;
    // Get metadata.
    int get_metadata = 0;
    // Tune. Takes a tuner number (see bands obtainaed by list_properties).
    int should_tune = 0;
    int band_number = -1;

    int opt;
    while ((opt = getopt(argc, argv, "lmt:")) != -1) {
        switch (opt) {
            case 'l':
                list_properties = 1;
                break;
            case 't':
                should_tune = 1;
                band_number = atoi(optarg);
                break;
            case 'm':
                get_metadata = 1;
                break;
        }
    }

    if (list_properties) {
        printf("Listing properties...\n");
        list_all_properties(dev);
    } else {
        if (should_tune) {
            if (band_number < 0) {
                printf("Tuner number should be positive");
                return -1;
            }
            printf("Tuning to a station...\n");
            tune(dev, band_number);
        }
        if (get_metadata) {
            if (!hal_tuner) {
                printf("Please pass -t <band_number> to tune to a valid station to get metadata.");
                exit(1);
            }
            get_tuner_metadata(dev);
        }
    }
    return 0;
}
