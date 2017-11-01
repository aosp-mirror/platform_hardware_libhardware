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
#ifndef HIDUTIL_TRISTATE_H_
#define HIDUTIL_TRISTATE_H_

#include <cassert>
#include <iostream>

namespace HidUtil {
template<typename T>
class TriState {
public:
    // constructor
    TriState() : mIsSet(false) { }
    TriState(const TriState<T> &other) : mIsSet(other.mIsSet), mValue(other.mValue) { }
    explicit TriState(const T &value) : mIsSet(true), mValue(value) { }

    void clear() {
        mValue = T();
        mIsSet = false;
    }
    bool isSet() const {
        return mIsSet;
    }

    const T get(const T &defaultValue) const {
        return isSet() ? mValue : defaultValue;
    }

    // operator overloading
    explicit operator T () const {
        assert(mIsSet);
        return mValue;
    }

    TriState<T>& operator=(const TriState<T> &other) {
        mIsSet = other.mIsSet;
        mValue = other.mValue;
        return *this;
    }

    TriState<T>& operator=(const T& value) {
        mIsSet = true;
        mValue = value;
        return *this;
    }

    TriState<T>& operator++()  {
        if (mIsSet) {
            mValue++;
        }
        return *this;
    }

    TriState<T> operator++(int) {
        TriState<T> tmp(*this);
        operator++();
        return tmp;
    }

    TriState<T>& operator--()  {
        if (mIsSet) {
            mValue--;
        }
        return *this;
    }

    TriState<T> operator--(int) {
        TriState<T> tmp(*this);
        operator--();
        return tmp;
    }

#define UNARY_OP(op) \
    TriState<T> operator op() { \
        TriState<T> tmp(*this); \
        if (mIsSet) { \
            tmp.mValue = op tmp.mValue; \
        } \
        return tmp; \
    }

    UNARY_OP(!);
    UNARY_OP(-);
    UNARY_OP(~);
#undef UNARY_OP

#define COMPOUND_ASSIGN_OP(op) \
    TriState<T>& operator op (const TriState<T>& rhs) { \
        if (mIsSet && rhs.mIsSet) { \
            mValue op rhs.mValue; \
        } else { \
            mIsSet = false; \
        } \
        return *this; \
    } \
    TriState<T>& operator op(const T& rhs) { \
        if (mIsSet) { \
            mValue op rhs; \
        } \
        return *this; \
    }

    COMPOUND_ASSIGN_OP(+=);
    COMPOUND_ASSIGN_OP(-=);
    COMPOUND_ASSIGN_OP(*=);
    COMPOUND_ASSIGN_OP(/=);
    COMPOUND_ASSIGN_OP(%=);
    COMPOUND_ASSIGN_OP(&=);
    COMPOUND_ASSIGN_OP(|=);
    COMPOUND_ASSIGN_OP(^=);
#undef COMPOUND_ASSIGN_OP

    TriState<T>& operator >>=(int i) {
        if (mIsSet) {
            mValue >>= i;
        }
        return *this; \
    }

    TriState<T>& operator <<=(int i) {
        if (mIsSet) {
            mValue <<= i;
        }
        return *this; \
    }

    TriState<T> operator <<(int i) { \
        TriState<T> tmp(*this);
        operator<<(i);
        return tmp;
    }

    TriState<T> operator >>(int i) { \
        TriState<T> tmp(*this);
        operator>>(i);
        return tmp;
    }

#define BINARY_OP(op, compound_op) \
    friend TriState<T> operator op(TriState<T> lhs, const TriState<T>& rhs) { \
        lhs compound_op rhs; \
    return lhs; \
    }\
        friend TriState<T> operator op(TriState<T> lhs, const T& rhs) { \
    lhs compound_op rhs; \
        return lhs; \
    }\
    friend TriState<T> operator op(const T &lhs, const TriState<T>& rhs) { \
        TriState<T> tmp(lhs); \
        return tmp op rhs; \
    }

    BINARY_OP(+, +=);
    BINARY_OP(-, -=);
    BINARY_OP(*, *=);
    BINARY_OP(/, /=);
    BINARY_OP(%, %=);
    BINARY_OP(&, &=);
    BINARY_OP(|, |=);
    BINARY_OP(^, ^=);
#undef BINARY_OP

#define RELATION_OP(op) \
    friend TriState<bool> operator op(const TriState<T>& lhs, const TriState<T>& rhs) { \
        if (lhs.mIsSet && rhs.mIsSet) { \
            return TriState<bool>(lhs.mValue op rhs.mValue); \
        } else { \
        return TriState<bool>(); \
        } \
    } \
    friend TriState<bool> operator op(const TriState<T>& lhs, const T& rhs) { \
        if (lhs.mIsSet) { \
            return TriState<bool>(lhs.mValue op rhs); \
        } else { \
            return TriState<bool>(); \
        } \
    } \
    friend TriState<bool> operator op(const T& lhs, const TriState<T>& rhs) { \
        if (rhs.mIsSet) { \
            return TriState<bool>(lhs op rhs.mValue); \
        } else { \
            return TriState<bool>(); \
        } \
    }

    RELATION_OP(==);
    RELATION_OP(!=);
    RELATION_OP(>=);
    RELATION_OP(<=);
    RELATION_OP(>);
    RELATION_OP(<);
#undef RELATION_OP

    friend TriState<bool> operator &&(TriState<T>& lhs, const TriState<T>& rhs) {
        if (lhs.mIsSet && rhs.mIsSet) {
            return TriState<bool>(lhs.mValue && rhs.mValue);
        } else {
            return TriState<bool>();
        }
    }

    friend TriState<bool> operator ||(TriState<T>& lhs, const TriState<T>& rhs) {
        if (lhs.mIsSet && rhs.mIsSet) {
            return TriState<bool>(lhs.mValue || rhs.mValue);
        } else {
            return TriState<bool>();
        }
    }

    friend std::ostream& operator <<(std::ostream &os, const TriState<T> &v) {
        if (v.mIsSet) {
            os << v.mValue;
        } else {
            os << "[not set]";
        }
        return os;
    }

    friend std::istream& operator >>(std::istream &is, const TriState<T> &v) {
        T a;
        is >> a;
        v = TriState<T>(a);
        return is;
    }
private:
    bool mIsSet;
    T mValue;
};

// commonly used ones
typedef TriState<unsigned> tri_uint;
typedef TriState<int> tri_int;

typedef TriState<uint32_t> tri_uint32_t;
typedef TriState<int32_t> tri_int32_t;
typedef TriState<uint8_t> tri_uint8_t;
typedef TriState<uint16_t> tri_uint16_t;
}

#endif // HIDUTIL_TRISTATE_H_
