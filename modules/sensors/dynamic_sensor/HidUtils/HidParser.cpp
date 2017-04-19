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
#include "HidDefs.h"
#include "HidParser.h"
#include "HidLog.h"
#include <iostream>
#include <iomanip>

namespace HidUtil {

void HidParser::reset() {
    mGlobalStack = HidGlobalStack();
    mLocal = HidLocal();
    mTree = std::make_shared<HidTreeNode>();
    mCurrent = mTree;
}

bool HidParser::parse(const std::vector<HidItem> &token) {
    // Clean up internal states of the parser for a new stream of descriptor token
    reset();

    bool ret = true;
    using namespace HidDef::TagType;

    for (auto &i : token) {
        switch (i.type) {
            case MAIN:
                ret = processMainTag(i);
                break;
            case GLOBAL:
                ret = mGlobalStack.append(i);
                break;
            case LOCAL:
                ret = mLocal.append(i);
                break;
            default:
                LOG_E << "HidParser found illegal HidItem: " << i << LOG_ENDL;
                ret = false;
        }

        // in case a parse failure, quit prematurely
        if (!ret) {
            break;
        }
    }
    return ret;
}

bool HidParser::processMainTag(const HidItem &i) {
    using namespace HidDef::MainTag;
    using namespace HidDef::ReportFlag;

    bool ret = true;
    switch (i.tag) {
        case COLLECTION: {
            unsigned int collectionType;
            if (!i.dataAsUnsigned(&collectionType)) {
                LOG_E << "Cannot get collection type at offset " << i.offset << LOG_ENDL;
                ret = false;
                break;
            }
            unsigned int fullUsage =
                    mGlobalStack.top().usagePage.get(0) << 16 | mLocal.getUsage(0);
            mCurrent = mCurrent->addChild(
                    std::make_shared<HidTreeNode>(mCurrent, collectionType, fullUsage));
            break;
        }
        case END_COLLECTION:
            mCurrent = mCurrent->getParent();
            if (!mCurrent) {
                // trigger parse failure so that mCurrent will not be accessed
                LOG_E << "unmatched END_COLLECTION at " << i.offset << LOG_ENDL;
                ret = false;
            }
            break;
        case INPUT:
        case OUTPUT:
        case FEATURE: {
            unsigned int reportType = i.tag;
            unsigned int flag;
            if (!i.dataAsUnsigned(&flag)) {
                LOG_E << "Cannot get report flag at offset " << i.offset << LOG_ENDL;
                ret = false;
                break;
            }
            const HidGlobal &top = mGlobalStack.top();

            // usage page, local min/max, report size and count have to be defined at report
            // definition.
            if (!(top.usagePage.isSet() && top.logicalMin.isSet() && top.logicalMax.isSet()
                  && top.reportSize.isSet() && top.reportCount.isSet())) {
                LOG_E << "Report defined at " << i.offset
                      << " does not have all mandatory fields set" << LOG_ENDL;
                ret = false;
                break;
            }
            if (top.reportSize.get(0) > 32) {
                LOG_E << "Report defined at " << i.offset
                      << " has unsupported report size(> 32 bit)" << LOG_ENDL;
                ret = false;
                break;
            }

            HidReport report(reportType, flag, top, mLocal);
            mReport.push_back(report);
            std::shared_ptr<HidTreeNode> node(new HidReportNode(mCurrent, report));
            mCurrent->addChild(node);
            break;
        }
        default:
            LOG_E << "unknown main tag, " << i << LOG_ENDL;
            ret = false;
    }
    // locals is cleared after any main tag according to HID spec
    mLocal.clear();
    return ret;
}

bool HidParser::parse(const unsigned char *begin, size_t size) {
    std::vector<HidItem> hidItemVector = HidItem::tokenize(begin, size);
    return parse(hidItemVector);
}

void HidParser::filterTree() {
    if (mTree != nullptr) {
        filterTree(mTree);
    }
}

void HidParser::filterTree(std::shared_ptr<HidTreeNode> &node) {
    if (node->isReportCollection()) {
        std::shared_ptr<HidReportNode> reportNode =
                std::static_pointer_cast<HidReportNode>(node->getChildren().front());
        if (reportNode != nullptr) {
            reportNode->collapse(node->getFullUsage());
            node = reportNode;
        }
    } else {
        for (auto &i : node->getChildren()) {
            filterTree(i);
        }
    }
}

HidParser::DigestVector HidParser::generateDigest(
        const std::unordered_set<unsigned int> &interestedUsage) {
    DigestVector digestVector;
    digest(&digestVector, mTree, interestedUsage);
    return digestVector;
}

void HidParser::digest(HidParser::DigestVector *digestVector,
                       const std::shared_ptr<HidTreeNode> &node,
                       const std::unordered_set<unsigned int> &interestedUsage) {
    if (digestVector == nullptr) {
        return;
    }

    if (node->isUsageCollection()
            && interestedUsage.find(node->getFullUsage()) != interestedUsage.end()) {
        // this collection contains the usage interested
        ReportSetGroup reportSetGroup;

        // one layer deep search
        for (auto &i : node->getChildren()) {
            // skip all nodes that is not a report node
            if (i->getNodeType() != HidTreeNode::TYPE_REPORT) {
                continue;
            }
            const HidReport &report =
                    std::static_pointer_cast<HidReportNode>(i)->getReport();

            unsigned int id = report.getReportId();;
            if (reportSetGroup.find(id) == reportSetGroup.end()) {
                // create an id group if it is not created
                reportSetGroup.emplace(id, ReportSet());
            }

            ReportSet &reportGroup = reportSetGroup[id];
            switch(report.getType()) {
                using namespace HidDef::MainTag;
                case FEATURE:
                    reportGroup[REPORT_TYPE_FEATURE].push_back(report);
                    break;
                case INPUT:
                    reportGroup[REPORT_TYPE_INPUT].push_back(report);
                    break;
                case OUTPUT:
                    reportGroup[REPORT_TYPE_OUTPUT].push_back(report);
                    break;
            }
        }
        ReportDigest digest = {
            .fullUsage = node->getFullUsage(),
            .packets = convertGroupToPacket(reportSetGroup)
        };
        digestVector->emplace_back(digest);
    } else {
        for (const auto &child : node->getChildren()) {
            if (child->getNodeType() == HidTreeNode::TYPE_NORMAL) {
                // only follow into collection nodes
                digest(digestVector, child, interestedUsage);
            }
        }
    }
}

std::vector<HidParser::ReportPacket> HidParser::convertGroupToPacket(
        const HidParser::ReportSetGroup &group) {
    std::vector<ReportPacket> packets;

    const std::vector<int> types = {REPORT_TYPE_FEATURE, REPORT_TYPE_INPUT, REPORT_TYPE_OUTPUT};

    for (const auto &setPair : group) {
        unsigned int id = setPair.first;
        for (auto type : types) {
            const auto &reports = setPair.second[type]; // feature

            // template
            ReportPacket packet = {
                .type = type,
                .id = id,
                .bitSize = 0
            };

            for (const auto &r : reports) {
                auto logical = r.getLogicalRange();
                auto physical = r.getPhysicalRange();

                int64_t offset = physical.first - logical.first;
                double scale = static_cast<double>((physical.second - physical.first))
                        / (logical.second - logical.first);
                scale *= r.getExponentValue();

                ReportItem digest = {
                    .usage = r.getFullUsage(),
                    .id = id,
                    .minRaw = logical.first,
                    .maxRaw = logical.second,
                    .a = scale,
                    .b = offset,
                    .bitOffset = packet.bitSize,
                    .bitSize = r.getSize(),
                    .count = r.getCount(),
                    .unit = r.getUnit(),
                };
                packet.reports.push_back(digest);
                packet.bitSize += digest.bitSize * digest.count;
            }
            if (!packet.reports.empty()) {
                packets.push_back(std::move(packet));
            }
        }
    }
    return packets;
}

static std::string reportTypeToString(int reportType) {
    switch (reportType) {
        case HidParser::REPORT_TYPE_INPUT:
            return "INPUT";
        case HidParser::REPORT_TYPE_OUTPUT:
            return "OUTPUT";
        case HidParser::REPORT_TYPE_FEATURE:
            return "FEATURE";
        default:
            return "INVALID REPORT";
    }
}

std::ostream& operator<<(std::ostream &os, const HidParser::DigestVector &digests) {
    for (const auto &i : digests) {
        os << "Usage: 0x" << std::hex  << i.fullUsage << std::dec
           << ", " << i.packets.size() << " report packet:" << LOG_ENDL;
        for (const auto &packet : i.packets) {
            os << reportTypeToString(packet.type) << " id: " << packet.id
               << " size: " << packet.bitSize
               << "b(" << packet.getByteSize() << "B), "
               << packet.reports.size() << " entries" << LOG_ENDL;

            for (const auto &report : packet.reports) {
                double min, max;
                report.decode(report.mask(report.minRaw), &min);
                report.decode(report.mask(report.maxRaw), &max);

                os << "  " << report.bitOffset << " size: " << report.bitSize
                   << ", count: " << report.count
                   << ", usage: " << std::hex << std::setfill('0') << std::setw(8)
                   << report.usage << std::dec
                   << ", min: " << report.minRaw << ", max: " << report.maxRaw
                   << ", minDecoded: " << min
                   << ", maxDecoded: " << max
                   << ", a: " << report.a << ", b: " << report.b
                   << std::hex
                   << ", minRawHex: 0x" << report.mask(report.minRaw)
                   << ", maxRawHex: 0x" << report.mask(report.maxRaw)
                   << ", rawMasked: 0x" << report.rawMask()
                   << std::dec << LOG_ENDL;
            }
        }
        os << LOG_ENDL;
    }
    os << LOG_ENDL;
    return os;
}
} // namespace HidUtil
