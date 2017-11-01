/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include "HidLog.h"
#include "HidParser.h"
#include "TestHidDescriptor.h"
#include <errno.h>

using HidUtil::HidParser;

bool doParse() {
    HidParser hidParser;
    bool ret = true;

    for (const TestHidDescriptor *p = gDescriptorArray; ; ++p) {
        if (p->data == nullptr || p->len == 0) {
            break;
        }
        const char *name = p->name != nullptr ? p->name : "unnamed";
        bool parseResult = hidParser.parse(p->data, p->len);

        if (parseResult) {
            LOG_V << name << "  filtered tree: " << LOG_ENDL;
            LOG_V << *(hidParser.getTree());
        } else {
            ret = false;
            LOG_E << name << " parsing error!" << LOG_ENDL;
        }
    }
    return ret;
}

bool doParseAndFilter() {
    HidParser hidParser;
    bool ret = true;

    for (const TestHidDescriptor *p = gDescriptorArray; ; ++p) {
        if (p->data == nullptr || p->len == 0) {
            break;
        }
        const char *name = p->name != nullptr ? p->name : "unnamed";
        bool parseResult = hidParser.parse(p->data, p->len);

        if (parseResult) {
            hidParser.filterTree();
            LOG_V << name << "  filtered tree: " << LOG_ENDL;
            LOG_V << *(hidParser.getTree());
        } else {
            ret = false;
            LOG_E << name << " parsing error!" << LOG_ENDL;
        }
    }
    return ret;
}

bool doDigest() {
    HidParser hidParser;
    bool ret = true;

    // value from HID sensor usage page spec
    std::unordered_set<unsigned int> interestedUsage = {
        0x200073, // accelerometer 3d
        0x200076, // gyro 3d
        0x200083, // mag 3d
        0x20008a, // device orientation (rotation vector)
    };

    for (const TestHidDescriptor *p = gDescriptorArray; ; ++p) {
        if (p->data == nullptr || p->len == 0) {
            break;
        }
        const char *name = p->name != nullptr ? p->name : "unnamed";
        bool parseResult = hidParser.parse(p->data, p->len);

        if (!parseResult) {
            LOG_E << name << " parsing error!" << LOG_ENDL;
            ret = false;
            continue;
        }

        hidParser.filterTree();
        LOG_V << name << "  digest: " << LOG_ENDL;
        HidParser::DigestVector digestVector = hidParser.generateDigest(interestedUsage);
        LOG_V << digestVector;
    }
    return ret;
}

void printUsage(char *argv0) {
    LOG_V << "Usage: " << argv0 << " test_name" << LOG_ENDL;
    LOG_V << "  test_name can be parse, parse_filter, digest." << LOG_ENDL;
}

int main(int argc, char* argv[]) {
    int ret;

    if (argc != 2) {
        LOG_E << "Error: need param" << LOG_ENDL;
        printUsage(argv[0]);
        return -EINVAL;
    }

    if (strcmp(argv[1], "parse") == 0) {
        ret = doParse() ? 0 : 1;
    } else if (strcmp(argv[1], "parse_filter") == 0) {
        ret = doParseAndFilter() ? 0 : 1;
    } else if (strcmp(argv[1], "digest") == 0) {
        ret = doDigest() ? 0 : 1;
    } else {
        LOG_E << "Error: unknown test name" << LOG_ENDL;
        printUsage(argv[0]);
        ret = -ENOENT;
    }

    return ret;
}
