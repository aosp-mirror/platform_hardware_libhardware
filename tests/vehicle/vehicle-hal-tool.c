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

#define LOG_TAG "vehicle-hal-tool"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <hardware/hardware.h>
#include <hardware/vehicle.h>

#include <cutils/log.h>

void usage() {
    printf("Usage: "
            "./vehicle-hal-tool [-l] [-m -p -t [-v]]\n"
            "-l - List properties\n"
            "-m - Mode (cannot be used with -l). Accepted strings: get, set or sub.\n"
            "-p - Property (only used with -m)\n"
            "-t - Type (only used with -m)\n"
            "-w - Wait time in seconds (only used with -m set to sub)\n"
            "-v - Value to which vehicle_prop_value is set\n"
            "Depending on the type pass the value:\n"
            "Int: pass a quoted integer\n"
            "Float: pass a quoted float\n"
            "Int array: pass a quoted space delimited int array, eg: \"1 2 3 4\" for\n:"
            "setting int32_array's all 4 elements (see VEHICLE_VALUE_TYPE_INT32_VEC4\n"
            "String: pass a normal string\n\n"
            "The configurations to use the tool are as follows:\n"
            "List Properties\n"
            "---------------\n"
            "./vehicle-hal-tool -l \n"
            "Lists the various properties defined in HAL implementation. Use this to check if "
            "the HAL implementation is correctly set up and exposing the capabilities correctly.\n"

            "Get Properties\n"
            "---------------\n"
            "./vehicle-hal-tool -m get -p <prop> -t <type> [-v <vehicle_prop_value>]\n"
            "Example: ./vehicle-hal-tool -m get -p 1028 -t 3 # VEHICLE_PROPERTY_DRIVING_STATUS\n"
            "./vehicle-hal-tool -m get -p 257 -t 1 # VEHICLE_PROPERTY_INFO_MAKE\n"
            "./vehicle-hal-tool -m get -p 2049 -t 19 -v \"3 0 0 0\"\n"
            "                                 # VEHICLE_PROPERTY_RADIO_PRESET\n"
            "with preset value set to 3.\n\n"
            "Set properties\n"
            "--------------\n"
            "./vehicle-hal-tool -m set -p 10 -t 1 -v random_property\n"
            "Set properties may not be applicable to most properties\n\n"
            "Subscribe properties\n"
            "--------------------\n"
            "Subscribes to be notified about a property change (depending on whether\n"
            "it is a on change property or a continuous property) for seconds provided\n"
            "as -w paramter.\n"
            "./vehicle-hal-tool -m sub -p 1028 -w 10\n"
    );
}

void list_all_properties(vehicle_hw_device_t *device) {
    int num_configs = -1;
    const vehicle_prop_config_t *configs = device->list_properties(device, &num_configs);
    if (num_configs < 0) {
        printf("List configs error. %d", num_configs);
        exit(1);
    }

    printf("Listing configs\n--------------------\n");
    int i = 0;
    for (i = 0; i < num_configs; i++) {
        const vehicle_prop_config_t *config_temp = configs + i;
        printf("Property ID: %d\n"
               "Property config_flags: %d\n"
               "Property change mode: %d\n"
               "Property min sample rate: %f\n"
               "Property max sample rate: %f\n",
               config_temp->prop, config_temp->config_flags, config_temp->change_mode,
               config_temp->min_sample_rate, config_temp->max_sample_rate);
    }
}

static void print_property(const vehicle_prop_value_t *data) {
    switch (data->value_type) {
        case VEHICLE_VALUE_TYPE_STRING:
            printf("Value type: STRING\n Size: %d\n", data->value.str_value.len);
            // This implementation only supports ASCII.
            char *ascii_out = (char *) malloc((data->value.str_value.len + 1) * sizeof(char));
            memcpy(ascii_out, data->value.str_value.data, data->value.str_value.len);
            ascii_out[data->value.str_value.len] = '\0';
            printf("Value Type: STRING %s\n", ascii_out);
            free(ascii_out);
            break;
        case VEHICLE_VALUE_TYPE_BYTES:
            printf("Value type: BYTES\n Size: %d", data->value.bytes_value.len);
            for (int i = 0; i < data->value.bytes_value.len; i++) {
                if ((i % 16) == 0) {
                    printf("\n %04X: ", i);
                }
                printf("%02X ", data->value.bytes_value.data[i]);
            }
            printf("\n");
            break;
        case VEHICLE_VALUE_TYPE_BOOLEAN:
            printf("Value type: BOOLEAN\nValue: %d\n", data->value.boolean_value);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_BOOLEAN:
            printf("Value type: ZONED_BOOLEAN\nZone: %d\n", data->zone);
            printf("Value: %d\n", data->value.boolean_value);
            break;
        case VEHICLE_VALUE_TYPE_INT64:
            printf("Value type: INT64\nValue: %" PRId64 "\n", data->value.int64_value);
            break;
        case VEHICLE_VALUE_TYPE_FLOAT:
            printf("Value type: FLOAT\nValue: %f\n", data->value.float_value);
            break;
        case VEHICLE_VALUE_TYPE_FLOAT_VEC2:
            printf("Value type: FLOAT_VEC2\nValue[0]: %f ", data->value.float_array[0]);
            printf("Value[1]: %f\n", data->value.float_array[1]);
            break;
        case VEHICLE_VALUE_TYPE_FLOAT_VEC3:
            printf("Value type: FLOAT_VEC3\nValue[0]: %f ", data->value.float_array[0]);
            printf("Value[1]: %f ", data->value.float_array[1]);
            printf("Value[2]: %f\n", data->value.float_array[2]);
            break;
        case VEHICLE_VALUE_TYPE_FLOAT_VEC4:
            printf("Value type: FLOAT_VEC4\nValue[0]: %f ", data->value.float_array[0]);
            printf("Value[1]: %f ", data->value.float_array[1]);
            printf("Value[2]: %f ", data->value.float_array[2]);
            printf("Value[3]: %f\n", data->value.float_array[3]);
            break;
        case VEHICLE_VALUE_TYPE_INT32:
            printf("Value type: INT32\nValue: %d\n", data->value.int32_value);
            break;
        case VEHICLE_VALUE_TYPE_INT32_VEC2:
            printf("Value type: INT32_VEC2\nValue[0]: %d ", data->value.int32_array[0]);
            printf("Value[1]: %d\n", data->value.int32_array[1]);
            break;
        case VEHICLE_VALUE_TYPE_INT32_VEC3:
            printf("Value type: INT32_VEC3\nValue[0]: %d ", data->value.int32_array[0]);
            printf("Value[1]: %d ", data->value.int32_array[1]);
            printf("Value[2]: %d\n", data->value.int32_array[2]);
            break;
        case VEHICLE_VALUE_TYPE_INT32_VEC4:
            printf("Value type: INT32_VEC4\nValue[0]: %d ", data->value.int32_array[0]);
            printf("Value[1]: %d ", data->value.int32_array[1]);
            printf("Value[2]: %d ", data->value.int32_array[2]);
            printf("Value[3]: %d\n", data->value.int32_array[3]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_FLOAT:
            printf("Value type: ZONED_FLOAT\nZone: %d ", data->zone);
            printf("Value: %f\n", data->value.float_value);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC2:
            printf("Value type: ZONED_FLOAT_VEC2\nZone: %d ", data->zone);
            printf("Value[0]: %f", data->value.float_array[0]);
            printf("Value[1]: %f\n", data->value.float_array[1]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC3:
            printf("Value type: ZONED_FLOAT_VEC3\nZone: %d ", data->zone);
            printf("Value[0]: %f ", data->value.float_array[0]);
            printf("Value[1]: %f ", data->value.float_array[1]);
            printf("Value[2]: %f\n", data->value.float_array[2]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC4:
            printf("Value type: ZONED_FLOAT_VEC4\nZone: %d ", data->zone);
            printf("Value[0]: %f ", data->value.float_array[0]);
            printf("Value[1]: %f ", data->value.float_array[1]);
            printf("Value[2]: %f ", data->value.float_array[2]);
            printf("Value[3]: %f\n", data->value.float_array[3]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_INT32:
            printf("Value type: ZONED_INT32\nZone: %d ", data->zone);
            printf("Value: %d\n", data->value.int32_value);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_INT32_VEC2:
            printf("Value type: ZONED_INT32_VEC2\nZone: %d ", data->zone);
            printf("Value[0]: %d ", data->value.int32_array[0]);
            printf("Value[1]: %d\n", data->value.int32_array[1]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_INT32_VEC3:
            printf("Value type: ZONED_INT32_VEC3\nZone: %d ", data->zone);
            printf("Value[0]: %d ", data->value.int32_array[0]);
            printf("Value[1]: %d ", data->value.int32_array[1]);
            printf("Value[2]: %d\n", data->value.int32_array[2]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_INT32_VEC4:
            printf("Value type: ZONED_INT32_VEC4\nZone: %d ", data->zone);
            printf("Value[0]: %d ", data->value.int32_array[0]);
            printf("Value[1]: %d ", data->value.int32_array[1]);
            printf("Value[2]: %d ", data->value.int32_array[2]);
            printf("Value[3]: %d\n", data->value.int32_array[3]);
            break;
        default:
            printf("Value type not yet handled: %d.\n", data->value_type);
    }
}

void get_property(
    vehicle_hw_device_t *device, int32_t property, int32_t type, char *value_string) {
    vehicle_prop_value_t *data = (vehicle_prop_value_t *) malloc (sizeof(vehicle_prop_value_t));

    // Parse the string according to type.
    if (value_string != NULL && strlen(value_string) > 0) {
        switch (type) {
            case VEHICLE_VALUE_TYPE_INT32:
                sscanf(value_string, "%d", &(data->value.int32_value));
                break;
            case VEHICLE_VALUE_TYPE_INT32_VEC4:
            {
                int32_t vec[4];
                sscanf(value_string, "%d %d %d %d", &vec[0], &vec[1], &vec[2], &vec[3]);
                memcpy(data->value.int32_array, vec, sizeof(vec));
                break;
            }
            default:
                printf("%s Setting value type not supported: %d\n", __func__, type);
                exit(1);
        }
    }

    data->prop = property;
    int ret_code = device->get(device, data);
    if (ret_code != 0) {
        printf("Cannot get property: %d\n", ret_code);
        exit(1);
    }

    // We simply convert the data into the type mentioned by the result of the
    // get call.
    printf("Get output\n------------\n");
    print_property(data);
    free(data);
}

void set_property(vehicle_hw_device_t *device,
                  int32_t property,
                  int32_t type,
                  char *data) {
    vehicle_prop_value_t vehicle_data;
    vehicle_data.prop = property;
    vehicle_data.value_type = type;
    int32_t zone = 0;
    float value = 0.0;
    switch (type) {
        case VEHICLE_VALUE_TYPE_STRING:
            // TODO: Make the code generic to UTF8 characters.
            vehicle_data.value.str_value.len = strlen(data);
            vehicle_data.value.str_value.data =
                (uint8_t *) malloc (strlen(data) * sizeof(uint8_t));
            memcpy(vehicle_data.value.str_value.data, data, strlen(data) + 1);
            break;
        case VEHICLE_VALUE_TYPE_BYTES: {
                int len = strlen(data);
                int numBytes = (len + 1) / 3;
                uint8_t *buf = calloc(numBytes, sizeof(uint8_t));
                char *byte = strtok(data, " ");
                for (int i = 0; byte != NULL && i < numBytes; i++) {
                    buf[i] = strtol(data, NULL, 16);
                    byte = strtok(NULL, " ");
                }
                vehicle_data.value.bytes_value.len = numBytes;
                vehicle_data.value.bytes_value.data = buf;
            }
            break;
        case VEHICLE_VALUE_TYPE_BOOLEAN:
            vehicle_data.value.boolean_value = atoi(data);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_BOOLEAN:
            sscanf(data, "%d %d", &vehicle_data.zone,
                &vehicle_data.value.boolean_value);
            break;
        case VEHICLE_VALUE_TYPE_INT64:
            vehicle_data.value.int64_value = atoi(data);
            break;
        case VEHICLE_VALUE_TYPE_FLOAT:
            vehicle_data.value.float_value = atof(data);
            break;
        case VEHICLE_VALUE_TYPE_FLOAT_VEC2:
            sscanf(data, "%f %f", &vehicle_data.value.float_array[0],
                &vehicle_data.value.float_array[1]);
            break;
        case VEHICLE_VALUE_TYPE_FLOAT_VEC3:
            sscanf(data, "%f %f %f", &vehicle_data.value.float_array[0],
                &vehicle_data.value.float_array[1],
                &vehicle_data.value.float_array[2]);
            break;
        case VEHICLE_VALUE_TYPE_FLOAT_VEC4:
            sscanf(data, "%f %f %f %f", &vehicle_data.value.float_array[0],
                &vehicle_data.value.float_array[1],
                &vehicle_data.value.float_array[2],
                &vehicle_data.value.float_array[3]);
            break;
        case VEHICLE_VALUE_TYPE_INT32:
            vehicle_data.value.int32_value = atoi(data);
            break;
        case VEHICLE_VALUE_TYPE_INT32_VEC2:
            sscanf(data, "%d %d", &vehicle_data.value.int32_array[0],
                &vehicle_data.value.int32_array[1]);
            break;
        case VEHICLE_VALUE_TYPE_INT32_VEC3:
            sscanf(data, "%d %d %d", &vehicle_data.value.int32_array[0],
                &vehicle_data.value.int32_array[1],
                &vehicle_data.value.int32_array[2]);
            break;
        case VEHICLE_VALUE_TYPE_INT32_VEC4:
            sscanf(data, "%d %d %d %d", &vehicle_data.value.int32_array[0],
                &vehicle_data.value.int32_array[1],
                &vehicle_data.value.int32_array[2],
                &vehicle_data.value.int32_array[3]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_FLOAT:
            sscanf(data, "%d %f", &zone, &value);
            vehicle_data.zone = zone;
            vehicle_data.value.float_value = value;
            break;
        case VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC2:
            sscanf(data, "%d %f %f", &vehicle_data.zone,
                &vehicle_data.value.float_array[0],
                &vehicle_data.value.float_array[1]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC3:
            sscanf(data, "%d %f %f %f", &vehicle_data.zone,
                &vehicle_data.value.float_array[0],
                &vehicle_data.value.float_array[1],
                &vehicle_data.value.float_array[2]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_FLOAT_VEC4:
            sscanf(data, "%d %f %f %f %f", &vehicle_data.zone,
                &vehicle_data.value.float_array[0],
                &vehicle_data.value.float_array[1],
                &vehicle_data.value.float_array[2],
                &vehicle_data.value.float_array[3]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_INT32:
            sscanf(data, "%d %d", &vehicle_data.zone,
                &vehicle_data.value.int32_value);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_INT32_VEC2:
            sscanf(data, "%d %d %d", &vehicle_data.zone,
                &vehicle_data.value.int32_array[0],
                &vehicle_data.value.int32_array[1]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_INT32_VEC3:
            sscanf(data, "%d %d %d %d", &vehicle_data.zone,
                &vehicle_data.value.int32_array[0],
                &vehicle_data.value.int32_array[1],
                &vehicle_data.value.int32_array[2]);
            break;
        case VEHICLE_VALUE_TYPE_ZONED_INT32_VEC4:
            sscanf(data, "%d %d %d %d %d", &vehicle_data.zone,
                &vehicle_data.value.int32_array[0],
                &vehicle_data.value.int32_array[1],
                &vehicle_data.value.int32_array[2],
                &vehicle_data.value.int32_array[3]);
            break;
        default:
            printf("set_property: Value type not yet handled: %d\n", type);
            exit(1);
    }
    printf("Setting Property id: %d\n", vehicle_data.prop);
    print_property(&vehicle_data);

    int ret_code = device->set(device, &vehicle_data);
    if (ret_code != 0) {
        printf("Cannot set property: %d\n", ret_code);
        exit(1);
    }
}

int vehicle_event_callback(const vehicle_prop_value_t *event_data) {
    // Print what we got.
    printf("Got some value from callback property: %d\n", event_data->prop);
    printf("Timestamp: %" PRId64 "\n", event_data->timestamp);
    print_property(event_data);
    return 0;
}

int vehicle_error_callback(int32_t error_code, int32_t property, int32_t operation) {
    // Print what we got.
    printf("Error code obtained: %d\n", error_code);
    return 0;
}

void subscribe_to_property(
    vehicle_hw_device_t *device,
    int32_t prop,
    float sample_rate,
    uint32_t wait_in_seconds) {
    // Init the device with a callback.
    int ret_code = device->subscribe(device, prop, 0, 0);
    if (ret_code != 0) {
        printf("Could not subscribe: %d\n", ret_code);
        exit(1);
    }

    // Callbacks will happen on one of the threads created by the HAL hence we
    // can simply sleep here and see the output.
    sleep(wait_in_seconds);

    // Unsubscribe and uninit.
    ret_code = device->unsubscribe(device, prop);
    if (ret_code != 0) {
        printf("Error unsubscribing the HAL, still continuining to uninit HAL ...");
    }
}

int main(int argc, char* argv[]) {
    // Open the vehicle module and just ask for the list of properties.
    const hw_module_t *hw_module = NULL;
    int ret_code = hw_get_module(VEHICLE_HARDWARE_MODULE_ID, &hw_module);
    if (ret_code != 0) {
        printf("Cannot open the hw module. Does the HAL exist? %d\n", ret_code);
        return -1;
    }

    vehicle_module_t *vehicle_module = (vehicle_module_t *)(hw_module);
    hw_device_t *device = NULL;
    ret_code = vehicle_module->common.methods->open(hw_module, NULL, &device);
    if (!device) {
        printf("Cannot open the hw device: %d\n", ret_code);
        return -1;
    }
    vehicle_hw_device_t *vehicle_device = (vehicle_hw_device_t *) (device);
    printf("HAL Loaded!\n");

    vehicle_device->init(vehicle_device, vehicle_event_callback, vehicle_error_callback);

    // If this is a list properties command - we check for -l command.
    int list_properties = 0;
    // Type of the property (see #defines in vehicle.h).
    int property = -1;
    // Type of the value of the property (see enum vehicle_value_type).
    int type = -1;
    // Whether the mode is "get" or "set".
    char mode[100] = "";
    // Actual value as a string representation (supports only PODs for now).
    // TODO: Support structures and complex types in the tool.
    char value[100] = "";
    // Wait time for the subscribe type of calls.
    // We keep a default in case the user does not specify one.
    int wait_time_in_sec = 10;
    // Sample rate for subscribe type of calls.
    // Default value is 0 for onchange type of properties.
    int sample_rate = 0;
    // Int array string which represents the vehicle_value_t in array of
    // numbers. See vehicle_prop_value_t.value.int32_array.
    char int_array_string[1000]; int_array_string[0] = '\0';

    int opt;
    while ((opt = getopt(argc, argv, "lm:p:t:v:w:s:")) != -1) {
        switch (opt) {
            case 'l':
                list_properties = 1;
                break;
            case 'm':
                strcpy(mode, optarg);
                break;
            case 'p':
                property = atoi(optarg);
                break;
            case 't':
                type = atoi(optarg);
                break;
            case 'v':
                strcpy(value, optarg);
                break;
            case 'w':
                wait_time_in_sec = atoi(optarg);
                break;
            case 's':
                sample_rate = atoi(optarg);
                break;
        }
    }

    // We should have atleast one of list properties or mode (for get or set).
    if (!list_properties &&
        !(!strcmp(mode, "get") || !strcmp(mode, "set") || !strcmp(mode, "sub"))) {
        usage();
        exit(1);
    }

    if (list_properties) {
        printf("Listing properties...\n");
        list_all_properties(vehicle_device);
    } else if (!strcmp(mode, "get")) {
        printf("Getting property ...\n");
        if (property == -1) {
            printf("Use -p to pass a valid Property.\n");
            usage();
            exit(1);
        }

        int32_t int_array_list[4];
        int count = -1;
        if (strlen(int_array_string) > 0) {
            count = sscanf(int_array_string, "%d%d%d%d",
                   &int_array_list[0], &int_array_list[1], &int_array_list[2], &int_array_list[3]);
        }

        get_property(vehicle_device, property, type, value);
    } else if (!strcmp(mode, "set")) {
        printf("Setting property ...\n");
        if (property == -1 || type == -1) {
            printf("Use -p to pass a valid Property and -t to pass a valid Type.\n");
            usage();
            exit(1);
        }
        set_property(vehicle_device, property, type, value);
    } else if (!strcmp(mode, "sub")) {
        printf("Subscribing property ...\n");
        if (property == -1 || wait_time_in_sec <= 0) {
            printf("Use -p to pass a valid property and -w to pass a valid wait time(s)\n");
            usage();
            exit(1);
        }
        subscribe_to_property(vehicle_device, property, sample_rate, wait_time_in_sec);
    }

    ret_code = vehicle_device->release(vehicle_device);
    if (ret_code != 0) {
        printf("Error uniniting HAL, exiting anyways.");
    }
    return 0;
}
