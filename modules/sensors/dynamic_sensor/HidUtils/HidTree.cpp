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
#include "HidLog.h"
#include "HidTree.h"
#include <memory>

namespace HidUtil {

// HidTreeNode
HidTreeNode::HidTreeNode() : mNodeType(TYPE_UNINITIALIZED), mData(0), mFullUsage(0) {
}

HidTreeNode::HidTreeNode(std::shared_ptr<HidTreeNode> parent,
                         uint32_t data, uint32_t fullUsage, int nodeType)
        : mNodeType(nodeType), mData(data),
        mFullUsage(fullUsage), mParent(parent) {
}

HidTreeNode::HidTreeNode(std::shared_ptr<HidTreeNode> parent,
                         uint32_t data, uint32_t fullUsage)
        : mNodeType(TYPE_NORMAL), mData(data),
        mFullUsage(fullUsage), mParent(parent) {
}

void HidTreeNode::outputRecursive(std::ostream &os, int level) const {
    insertIndentation(os, level);
    os << "Node data: " << mData
       << ", usage " << std::hex << mFullUsage << std::dec << LOG_ENDL;

    for (auto &child : mChildren) {
        child->outputRecursive(os, level + 1);
    }
}

std::shared_ptr<HidTreeNode> HidTreeNode::deepCopy(
        std::shared_ptr<HidTreeNode> parent) const {
    std::shared_ptr<HidTreeNode> copy(new HidTreeNode(parent, mData, mFullUsage, mNodeType));
    for (auto &i : mChildren) {
        copy->mChildren.push_back(i->deepCopy(copy));
    }
    return copy;
}

void HidTreeNode::insertIndentation(std::ostream &os, int level) const {
    constexpr char indentCharacter = '\t';
    std::fill_n(std::ostreambuf_iterator<char>(os), level, indentCharacter);
}

std::shared_ptr<HidTreeNode> HidTreeNode::addChild(std::shared_ptr<HidTreeNode> child) {
    mChildren.push_back(child);
    return child;
}

std::shared_ptr<HidTreeNode> HidTreeNode::getParent() const {
    return mParent.lock();
}

bool HidTreeNode::isReportCollection() const {
    return mNodeType == TYPE_NORMAL && mChildren.size() == 1
            && mChildren.front()->mNodeType == TYPE_REPORT;
}

unsigned int HidTreeNode::getFullUsage() const {
    return mFullUsage;
}

std::vector<std::shared_ptr<HidTreeNode>>& HidTreeNode::getChildren() {
    return mChildren;
}

const std::vector<std::shared_ptr<HidTreeNode>>& HidTreeNode::getChildren() const {
    return mChildren;
}

bool HidTreeNode::isUsageCollection() const {
    using namespace HidDef::CollectionType;
    return mNodeType == TYPE_NORMAL && (mData == PHYSICAL || mData == APPLICATION);
}

int HidTreeNode::getNodeType() const {
    return mNodeType;
}

std::ostream& operator<<(std::ostream& os, const HidTreeNode& n) {
    n.outputRecursive(os, 0);
    return os;
}

// HidReportNode
HidReportNode::HidReportNode(std::shared_ptr<HidTreeNode> parent, const HidReport &report)
    : HidTreeNode(parent, 0 /*data*/, 0 /*fullUsage*/, TYPE_REPORT), mReport(report) {
}

void HidReportNode::outputRecursive(std::ostream &os, int level) const {
    insertIndentation(os, level);
    os << mReport << LOG_ENDL;
}

std::shared_ptr<HidTreeNode> HidReportNode::deepCopy(
        std::shared_ptr<HidTreeNode> parent) const {
    std::shared_ptr<HidTreeNode> copy(new HidReportNode(parent, mReport));
    return copy;
}

const HidReport& HidReportNode::getReport() const {
    return mReport;
}

void HidReportNode::collapse(unsigned int newUsage) {
    mReport.setCollapsed(newUsage);
}

} //namespace HidUtil
