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

#include "TestHidDescriptor.h"
#include "HidLog.h"
#include "HidParser.h"
#include "StreamIoUtil.h"

using namespace HidUtil;

void printRawValue(const std::vector<unsigned char> &descriptor) {
    LOG_D << "Descriptor [" << descriptor.size() << "]: " << std::hex;
    hexdumpToStream(LOG_D, descriptor.begin(), descriptor.end());
}

void printToken(const std::vector<HidItem> &hidItemVector) {
    LOG_V << "Total " << hidItemVector.size() << " tokens" << LOG_ENDL;
    for (auto &i : hidItemVector) {
        LOG_V << i << LOG_ENDL;
    }
}

int main() {
    const TestHidDescriptor *t = findTestDescriptor("accel3");
    constexpr unsigned int ACCEL_3D_USAGE = 0x200073;

    assert(t != nullptr);
    std::vector<unsigned char> descriptor(t->data, t->data + t->len);

    // parse can be done in one step with HidParser::parse(const unsigned char *begin, size_t size);
    // here it is done in multiple steps for illustration purpose
    HidParser hidParser;
    // print out raw value
    printRawValue(descriptor);

    // tokenize it
    std::vector<HidItem> hidItemVector = HidItem::tokenize(descriptor);

    // print out tokens
    printToken(hidItemVector);

    // parse it
    if (hidParser.parse(hidItemVector)) {
        // making a deepcopy of tree (not necessary, but for illustration)
        std::shared_ptr<HidTreeNode> tree = hidParser.getTree()->deepCopy();

        LOG_V << "Tree: " << LOG_ENDL;
        LOG_V << *tree;
        LOG_V << LOG_ENDL;

        hidParser.filterTree();
        LOG_V << "FilteredTree: " << LOG_ENDL;
        LOG_V << *(hidParser.getTree());

        LOG_V << "DigestVector: " << LOG_ENDL;
        std::unordered_set<unsigned int> interested = {ACCEL_3D_USAGE};
        HidParser::DigestVector digestVector = hidParser.generateDigest(interested);
        LOG_V << digestVector;

    } else {
        LOG_V << "Parsing Error" << LOG_ENDL;
    }

    return 0;
}

