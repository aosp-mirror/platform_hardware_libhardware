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
#ifndef HIDUTIL_HIDTREE_H_
#define HIDUTIL_HIDTREE_H_

#include "HidReport.h"

#include <cstddef>
#include <vector>
#include <iostream>

namespace HidUtil {

// HID report parser output tree node
class HidTreeNode {
    friend std::ostream& operator<<(std::ostream& os, const HidTreeNode& n);
public:
    enum {
        TYPE_UNINITIALIZED = 0,
        TYPE_NORMAL = 1,
        TYPE_REPORT = 2     // can be cast to HidReportNode
    };
    HidTreeNode();
    HidTreeNode(std::shared_ptr<HidTreeNode> parent, uint32_t data, uint32_t fullUsage);

    virtual ~HidTreeNode() = default;

    // make a deep copy of tree at and below this node and attach it to specified parent node
    virtual std::shared_ptr<HidTreeNode> deepCopy(
          std::shared_ptr<HidTreeNode> parent = nullptr) const;

    // add child to this node
    std::shared_ptr<HidTreeNode> addChild(std::shared_ptr<HidTreeNode> child);

    // get all children of a node
    std::vector<std::shared_ptr<HidTreeNode>>& getChildren();
    const std::vector<std::shared_ptr<HidTreeNode>>& getChildren() const;

    // get parent (nullptr if it is root node or if lock weak_ptr failed)
    std::shared_ptr<HidTreeNode> getParent() const;

    // access usage of this node
    unsigned int getFullUsage() const;

    bool isReportCollection() const;
    bool isUsageCollection() const;
    int getNodeType() const;
protected:
    // for derived class to define different nodeType
    HidTreeNode(std::shared_ptr<HidTreeNode> parent,
              uint32_t data, uint32_t fullUsage, int nodeType);

    // helper for stream output
    void insertIndentation(std::ostream &os, int level) const;
private:
    // helper for stream output
    virtual void outputRecursive(std::ostream& os, int level) const;

    int mNodeType;
    uint32_t mData;
    uint32_t mFullUsage;

    std::vector<std::shared_ptr<HidTreeNode>> mChildren;
    std::weak_ptr<HidTreeNode> mParent;
};

// Tree node that corresponds to an input, output or feature report
class HidReportNode : public HidTreeNode {
public:
    HidReportNode(std::shared_ptr<HidTreeNode> parent, const HidReport &report);

    virtual std::shared_ptr<HidTreeNode> deepCopy(
          std::shared_ptr<HidTreeNode> parent = nullptr) const override;

    // obtain HidReport attached to this node
    const HidReport& getReport() const;

    // reset usage of node and set underlying report to collapsed
    void collapse(unsigned int newUsage);
private:
    virtual void outputRecursive(std::ostream &os, int level) const override;

    HidReport mReport;
};

std::ostream& operator<<(std::ostream& os, const HidTreeNode& n);

} // namespace HidUtil

#endif // HIDUTIL_HIDTREE_H_
