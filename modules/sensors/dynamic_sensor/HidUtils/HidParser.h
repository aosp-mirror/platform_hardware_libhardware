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
#ifndef HIDUTIL_HIDPARSER_H_
#define HIDUTIL_HIDPARSER_H_

#include "HidItem.h"
#include "HidTree.h"
#include "HidGlobal.h"
#include "HidLocal.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>
#include <ostream>

namespace HidUtil {
class HidParser {
public:
    enum {
        REPORT_TYPE_FEATURE = 0,
        REPORT_TYPE_INPUT = 1,
        REPORT_TYPE_OUTPUT = 2
    };

    struct ReportItem;
    struct ReportPacket;

    // report (including input output and feature) grouped by full usage
    struct ReportDigest {
        unsigned int fullUsage;
        std::vector<ReportPacket> packets;
    };

    typedef std::vector<ReportDigest> DigestVector;

    // parse HID descriptor
    bool parse(const std::vector<HidItem> &token);
    bool parse(const unsigned char *begin, size_t size);

    // filter the tree to eliminate single child report leaf node causes by usage array type
    // reports
    void filterTree();

    // generate a list of report digest for all interested usage. It will automatically
    // call filterTree().
    DigestVector generateDigest(const std::unordered_set<unsigned int> &interestedUsage);

    // get parsed tree (filtered or not filtered)
    const std::shared_ptr<HidTreeNode> getTree() const { return mTree; }

    // get all parsed report in a parsed form.
    const std::vector<HidReport>& getReport() const { return mReport; }

private:
    typedef std::array<std::vector<HidReport>, 3> ReportSet;
    typedef std::unordered_map<unsigned int /* reportId */, ReportSet> ReportSetGroup;

    // helper subroutines
    void reset();
    bool processMainTag(const HidItem &i);
    static void filterTree(std::shared_ptr<HidTreeNode> &node);
    static void digest(
            DigestVector *digestVector,
            const std::shared_ptr<HidTreeNode> &node,
            const std::unordered_set<unsigned int> &interestedUsage);
    static std::vector<ReportPacket> convertGroupToPacket(const ReportSetGroup &group);

    HidGlobalStack mGlobalStack;
    HidLocal mLocal;
    std::shared_ptr<HidTreeNode> mTree;
    std::shared_ptr<HidTreeNode> mCurrent;
    std::vector<HidReport> mReport;
};

struct HidParser::ReportItem {
    unsigned int usage;
    unsigned int id;
    int type; // feature, input or output

    int64_t minRaw;
    int64_t maxRaw;

    // conversion for float point values
    // real value = (signExtendIfNeeded(raw) + b) * a
    // raw value = mask(real/a - b);
    //
    // conversion for integer values
    // real value = signExtendedIfNeeded(raw) + b;
    // raw value = mask(real - b);
    double a; // scaling
    int64_t b; // offset
    unsigned int unit;

    size_t bitOffset;
    size_t bitSize; // bit length per unit
    size_t count;

    // helper function
    bool isSigned() const {
        return minRaw < 0;
    }

    bool isByteAligned() const {
        return (bitOffset & 7) == 0 && (bitSize & 7) == 0;
    }

    // convert raw values to unsigned format
    uint32_t mask(int64_t input) const {
        return static_cast<uint32_t>(input & rawMask());
    }

    bool decode(uint32_t input, double *output) const {
        if (output == nullptr) {
            return false;
        }
        int64_t s = signExtendIfNeeded(input);
        if (s < minRaw || s > maxRaw) {
            return false;
        }
        *output = (s + b) * a;
        return true;
    }

    bool encode(double input, uint32_t *output) const {
        if (output == nullptr) {
            return false;
        }
        input = input / a - b;
        if (input < minRaw || input > maxRaw) {
            return false;
        }
        *output = static_cast<uint32_t>(static_cast<int64_t>(input) & rawMask());
        return true;
    }

    int64_t rawMask() const {
        constexpr int64_t one = 1;
        return (one << bitSize) - 1;
    }

    int64_t signExtendIfNeeded(int64_t value) const {
        return value | ((isSigned() && isNegative(value)) ? ~rawMask() : 0);
    }

    bool isNegative(int64_t value) const {
        constexpr int64_t one = 1;
        return ((one << (bitSize - 1)) & value) != 0;
    }
};

// a collection of report item that forms a packet
// this is the input output unit with HID hardware
struct HidParser::ReportPacket {
    std::vector<ReportItem> reports;
    size_t bitSize;
    int type; // REPORT_TYPE_FEATURE/INPUT/OUTPUT
    unsigned int id;

    size_t getByteSize() const { return (bitSize + 7) / 8; };
};

std::ostream& operator<<(std::ostream &os, const HidParser::DigestVector &digest2);
} // namespace HidUtil

#endif // HIDUTIL_HIDPARSER_H_
