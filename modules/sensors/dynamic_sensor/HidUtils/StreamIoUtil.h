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
#ifndef HIDUTIL_STREAM_IO_UTIL_H_
#define HIDUTIL_STREAM_IO_UTIL_H_

#include "HidLog.h"
#include <istream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cassert>

namespace HidUtil {

template<typename CharT>
class charvectorbuf : public std::streambuf { // class name is consistent with std lib
    static_assert(std::is_const<CharT>::value == false, "cannot use const type");
public:
    // r/w buffer constructors
    charvectorbuf(std::vector<CharT> &vec) {
        init(vec.data(), vec.size());
    }

    charvectorbuf(CharT *begin, CharT *end) {
        assert(end >= begin);
        init(begin, end - begin);
    }

    charvectorbuf(CharT *begin, size_t size) {
        init(begin, size);
    }

    // r/o buffer constructor
    charvectorbuf(const std::vector<CharT> &vec) {
        init(vec.data(), vec.size());
    }

    charvectorbuf(const CharT *begin, const CharT *end) {
        assert(end >= begin);
        init(begin, end - begin);
    }

    charvectorbuf(const CharT *begin, size_t size) {
        init(begin, size);
    }
 protected:
    virtual std::streampos seekpos(
            std::streampos sp, std::ios_base::openmode which =
            std::ios_base::in | std::ios_base::out) override {
        return seekoff(std::streamoff(sp), std::ios_base::beg, which);
    }

    // this is needed to use ftell() on stream
    virtual std::streampos seekoff(
            std::streamoff off, std::ios_base::seekdir way,
            std::ios_base::openmode which =
            std::ios_base::in | std::ios_base::out) override {

        // pptr() == nullptr: read-only
        assert(pptr() == nullptr || egptr() - eback() == epptr() - pbase());
        bool in = which & std::ios_base::in;
        bool out = which & std::ios_base::out;
        pos_type end = egptr() - eback();

        if (!in && !out) {
            return pos_type(-1);
        }

        if (in && out && way == std::ios_base::cur) {
            return pos_type(-1);
        }

        off_type noff;
        switch (way) {
            case std::ios_base::beg:
                noff = 0;
                break;
            case std::ios_base::cur:
                if (in) {
                    noff = gptr() - eback();
                } else {
                    noff = pptr() - pbase();
                }
                break;
            case std::ios_base::end:
                noff = end;
                break;
            default:
                return pos_type(-1);
        }
        noff += off;
        if (noff < 0 ||  noff > end) {
            return pos_type(-1);
        }

        if (noff != 0 && ((in && gptr() == nullptr) || (out && pptr() == nullptr))) {
            return pos_type(-1);
        }

        if (in) {
            setg(eback(), eback() + noff, egptr());
        }

        if (out) {
            setp(pbase(), epptr());
            pbump(noff);
        }

        return pos_type(noff);
    }
private:
    // read only buffer init
    void init(const CharT *base, size_t size) {
        setg((char*)base, (char*)base, (char*)(base + size));
    }

    // read write buffer init
    void init(CharT *base, size_t size) {
        setg((char*)base, (char*)base, (char*)(base + size));
        setp((char*)base, (char*)(base + size));
    }
};

// dump binary values
template <class ForwardIterator>
void hexdumpToStream(std::ostream &os, const ForwardIterator &first, const ForwardIterator &last) {
    static_assert(
            std::is_convertible<
                typename std::iterator_traits<ForwardIterator>::iterator_category,
                std::forward_iterator_tag>::value
            && std::is_convertible<
                typename std::iterator_traits<ForwardIterator>::value_type,
                unsigned char>::value
            && sizeof(typename std::iterator_traits<ForwardIterator>::value_type)
                == sizeof(unsigned char),
            "Only accepts forward iterator of a type of size 1 "
                "that can be convert to unsigned char.\n");
    size_t c = 0;
    std::ostringstream ss;
    for (ForwardIterator i = first; i != last; ++i, ++c) {
        unsigned char v = *i;
        // formatting
        switch (c & 0xf) {
            case 0:
                // address
                os << ss.str() << LOG_ENDL;
                ss.str("");
                ss << std::hex;
                ss << std::setfill('0') << std::setw(4) << c << ": ";
                break;
            case 8:
                // space
                ss << " ";
                break;
        }
        ss << std::setfill('0') << std::setw(2)
           << static_cast<unsigned>(static_cast<unsigned char>(v)) << " ";
    }
    os << ss.str() << LOG_ENDL;
}
} //namespace HidUtil

#endif // HIDUTIL_STREAM_IO_UTIL_H_

