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
#include "HidRawDevice.h"
#include "HidLog.h"
#include "Utils.h"

#include <fcntl.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <linux/hiddev.h>  // HID_STRING_SIZE
#include <sys/ioctl.h>
#include <unistd.h>

#include <set>

namespace android {
namespace SensorHalExt {

using HidUtil::HidItem;

HidRawDevice::HidRawDevice(
        const std::string &devName, const std::unordered_set<unsigned int> &usageSet)
        : mDevFd(-1), mMultiIdDevice(false), mValid(false) {
    // open device
    mDevFd = ::open(devName.c_str(), O_RDWR); // read write?
    if (mDevFd < 0) {
        LOG_E << "Error in open device node: " << errno << " (" << ::strerror(errno) << ")"
              << LOG_ENDL;
        return;
    }

    // get device information, including hid descriptor
    if (!populateDeviceInfo()) {
        LOG_E << "Error obtaining HidRaw device information" << LOG_ENDL;
        return;
    }

    if (!generateDigest(usageSet)) {
        LOG_E << "Cannot parse hid descriptor" << LOG_ENDL;
        return;
    }

    // digest error checking
    std::unordered_set<unsigned int> reportIdSet;
    for (auto const &digest : mDigestVector) {
        for (auto const &packet : digest.packets) {
            if (mReportTypeIdMap.emplace(
                        std::make_pair(packet.type, packet.id), &packet).second == false) {
                LOG_E << "Same type - report id pair (" << packet.type << ", " << packet.id << ")"
                      << "is used by more than one usage collection" << LOG_ENDL;
                return;
            }
            reportIdSet.insert(packet.id);
        }
    }
    if (mReportTypeIdMap.empty()) {
        return;
    }

    if (reportIdSet.size() > 1) {
        if (reportIdSet.find(0) != reportIdSet.end()) {
            LOG_E << "Default report id 0 is not expected when more than one report id is found."
                  << LOG_ENDL;
            return;
        }
        mMultiIdDevice = true;
    } else { // reportIdSet.size() == 1
        mMultiIdDevice = !(reportIdSet.find(0) != reportIdSet.end());
    }
    mValid = true;
}

HidRawDevice::~HidRawDevice() {
    if (mDevFd > 0) {
        ::close(mDevFd);
        mDevFd = -1;
    }
}

bool HidRawDevice::populateDeviceInfo() {
    HidDeviceInfo info;
    char buffer[HID_STRING_SIZE + 1];

    if (mDevFd < 0) {
        return false;
    }

    // name
    if (ioctl(mDevFd, HIDIOCGRAWNAME(sizeof(buffer) - 1), buffer) < 0) {
        return false;
    }
    buffer[sizeof(buffer) - 1] = '\0';
    info.name = buffer;

    // physical path
    if (ioctl(mDevFd, HIDIOCGRAWPHYS(sizeof(buffer) - 1), buffer) < 0) {
        return false;
    }
    buffer[sizeof(buffer) - 1] = '\0';
    info.physicalPath = buffer;

    // raw device info
    hidraw_devinfo devInfo;
    if (ioctl(mDevFd, HIDIOCGRAWINFO, &devInfo) < 0) {
        return false;
    }

    switch (devInfo.bustype) {
    case BUS_USB:
        info.busType = "USB";
        break;
    case BUS_HIL:
        info.busType = "HIL";
        break;
    case BUS_BLUETOOTH:
        info.busType = "Bluetooth";
        break;
    case BUS_VIRTUAL:
        info.busType = "Virtual";
        break;
    default:
        info.busType = "Other";
        break;
    }

    info.vendorId = devInfo.vendor;
    info.productId = devInfo.vendor;

    uint32_t descriptorSize;
    /* Get Report Descriptor Size */
    if (ioctl(mDevFd, HIDIOCGRDESCSIZE, &descriptorSize) < 0) {
        return false;
    }

    struct hidraw_report_descriptor reportDescriptor;
    memset(&reportDescriptor, 0, sizeof(reportDescriptor));
    info.descriptor.resize(descriptorSize);
    reportDescriptor.size = descriptorSize;
    if (ioctl(mDevFd, HIDIOCGRDESC, &reportDescriptor) < 0) {
        return false;
    }
    std::copy(reportDescriptor.value, reportDescriptor.value + descriptorSize,
              info.descriptor.begin());
    mDeviceInfo = info;
    return true;
}

bool HidRawDevice::generateDigest(const std::unordered_set<unsigned int> &usage) {
    if (mDeviceInfo.descriptor.empty()) {
        return false;
    }

    std::vector<HidItem> tokens = HidItem::tokenize(mDeviceInfo.descriptor);
    HidParser parser;
    if (!parser.parse(tokens)) {
        return false;
    }

    parser.filterTree();
    mDigestVector = parser.generateDigest(usage);

    return mDigestVector.size() > 0;
}

bool HidRawDevice::isValid() {
    return mValid;
}

bool HidRawDevice::getFeature(uint8_t id, std::vector<uint8_t> *out) {
    if (mDevFd < 0) {
        return false;
    }

    if (out == nullptr) {
        return false;
    }

    const HidParser::ReportPacket *packet = getReportPacket(HidParser::REPORT_TYPE_FEATURE, id);
    if (packet == nullptr) {
        LOG_E << "HidRawDevice::getFeature: unknown feature " << id << LOG_ENDL;
        return false;
    }

    size_t size = packet->getByteSize() + 1; // report id size

    std::lock_guard<std::mutex> l(mIoBufferLock);
    if (mIoBuffer.size() < size) {
        mIoBuffer.resize(size);
    }
    mIoBuffer[0] = id;
    int res = ::ioctl(mDevFd, HIDIOCGFEATURE(size), mIoBuffer.data());
    if (res < 0) {
        LOG_E << "HidRawDevice::getFeature: feature " << static_cast<int>(id)
              << " ioctl returns " << res << " (" << ::strerror(res) << ")" << LOG_ENDL;
        return false;
    }

    if (static_cast<size_t>(res) != size) {
        LOG_E << "HidRawDevice::getFeature: get feature " << static_cast<int>(id)
              << " returned " << res << " bytes, does not match expected " << size << LOG_ENDL;
        return false;
    }
    if (mIoBuffer.front() != id) {
        LOG_E << "HidRawDevice::getFeature: get feature " << static_cast<int>(id)
              << " result has header " << mIoBuffer.front() << LOG_ENDL;
    }
    out->resize(size - 1);
    std::copy(mIoBuffer.begin() + 1, mIoBuffer.begin() + size, out->begin());
    return true;
}

bool HidRawDevice::setFeature(uint8_t id, const std::vector<uint8_t> &in) {
    if (mDevFd < 0) {
        return false;
    }

    const HidParser::ReportPacket *packet = getReportPacket(HidParser::REPORT_TYPE_FEATURE, id);
    if (packet == nullptr) {
        LOG_E << "HidRawDevice::setFeature: Unknown feature " << id << LOG_ENDL;
        return false;
    }

    size_t size = packet->getByteSize();
    if (size != in.size()) {
        LOG_E << "HidRawDevice::setFeature: set feature " << id << " size mismatch, need "
              << size << " bytes, have " << in.size() << " bytes" << LOG_ENDL;
        return false;
    }

    ++size; // report id byte
    std::lock_guard<std::mutex> l(mIoBufferLock);
    if (mIoBuffer.size() < size) {
        mIoBuffer.resize(size);
    }
    mIoBuffer[0] = id;
    std::copy(in.begin(), in.end(), &mIoBuffer[1]);
    int res = ::ioctl(mDevFd, HIDIOCSFEATURE(size), mIoBuffer.data());
    if (res < 0) {
        LOG_E << "HidRawDevice::setFeature: feature " << id << " ioctl returns " << res
              << " (" << ::strerror(res) << ")" << LOG_ENDL;
        return false;
    }
    return true;
}

bool HidRawDevice::sendReport(uint8_t id, std::vector<uint8_t> &data) {
    if (mDevFd < 0) {
        return false;
    }

    const HidParser::ReportPacket *packet = getReportPacket(HidParser::REPORT_TYPE_OUTPUT, id);
    if (packet == nullptr) {
        LOG_E << "HidRawDevice::sendReport: unknown output " << id << LOG_ENDL;
        return false;
    }

    size_t size = packet->getByteSize();
    if (size != data.size()) {
        LOG_E << "HidRawDevice::sendReport: send report " << id << " size mismatch, need "
              << size << " bytes, have " << data.size() << " bytes" << LOG_ENDL;
        return false;
    }
    int res;
    if (mMultiIdDevice) {
        std::lock_guard<std::mutex> l(mIoBufferLock);
        ++size;
        if (mIoBuffer.size() < size) {
            mIoBuffer.resize(size);
        }
        mIoBuffer[0] = id;
        std::copy(mIoBuffer.begin() + 1, mIoBuffer.end(), data.begin());
        res = ::write(mDevFd, mIoBuffer.data(), size);
    } else {
        res = ::write(mDevFd, data.data(), size);
    }
    if (res < 0) {
        LOG_E << "HidRawDevice::sendReport: output " << id << " write returns " << res
              << " (" << ::strerror(res) << ")" << LOG_ENDL;
        return false;
    }
    return true;
}

bool HidRawDevice::receiveReport(uint8_t *id, std::vector<uint8_t> *data) {
    if (mDevFd < 0) {
        return false;
    }

    uint8_t buffer[256];
    int res = ::read(mDevFd, buffer, 256);
    if (res < 0) {
        LOG_E << "HidRawDevice::receiveReport: read returns " << res
              << " (" << ::strerror(res) << ")" << LOG_ENDL;
        return false;
    }

    if (mMultiIdDevice) {
        if (!(res > 1)) {
            LOG_E << "read hidraw returns data too short, len: " << res << LOG_ENDL;
            return false;
        }
        data->resize(static_cast<size_t>(res - 1));
        std::copy(buffer + 1, buffer + res, data->begin());
        *id = buffer[0];
    } else {
        data->resize(static_cast<size_t>(res));
        std::copy(buffer, buffer + res, data->begin());
        *id = 0;
    }
    return true;
}

const HidParser::ReportPacket *HidRawDevice::getReportPacket(unsigned int type, unsigned int id) {
    auto i = mReportTypeIdMap.find(std::make_pair(type, id));
    return (i == mReportTypeIdMap.end()) ? nullptr : i->second;
}

} // namespace SensorHalExt
} // namespace android
