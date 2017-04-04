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
#include "TriState.h"
#include <gtest/gtest.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>

using HidUtil::TriState;
typedef TriState<uint32_t> tri_uint32_t;
typedef TriState<int32_t> tri_int32_t;
typedef TriState<int16_t> tri_int16_t;

TEST(TriStateTest, Constructor) {
    tri_uint32_t a;
    EXPECT_FALSE(a.isSet());

    a += 1;
    EXPECT_FALSE(a.isSet());

    a -= 1;
    EXPECT_FALSE(a.isSet());

    a *= 1;
    EXPECT_FALSE(a.isSet());

    a /= 1;
    EXPECT_FALSE(a.isSet());

    tri_uint32_t b;
    b = a;
    EXPECT_FALSE(b.isSet());

    a = 1;
    EXPECT_TRUE(a.isSet());

    b = a;
    EXPECT_TRUE(b.isSet());

    a.clear();
    EXPECT_FALSE(a.isSet());
    EXPECT_TRUE(b.isSet());

    tri_uint32_t c(b);
    EXPECT_TRUE(c.isSet());

    c.clear();
    EXPECT_FALSE(c.isSet());

    tri_uint32_t d(a);
    EXPECT_FALSE(c.isSet());
}

TEST(TriStateTest, IncAndDecOperation) {
    tri_int32_t a(1);

    EXPECT_EQ(2, (++a).get(0));
    EXPECT_EQ(2, (a++).get(0));
    EXPECT_EQ(3, a.get(0));

    EXPECT_EQ(2, (--a).get(0));
    EXPECT_EQ(2, (a--).get(0));
    EXPECT_EQ(1, a.get(0));

    tri_uint32_t b;
    EXPECT_EQ(static_cast<uint32_t>(100), (++b).get(100));
    EXPECT_EQ(static_cast<uint32_t>(101), (b++).get(101));
    EXPECT_EQ(static_cast<uint32_t>(102), b.get(102));
    EXPECT_FALSE(b.isSet());

    EXPECT_EQ(static_cast<uint32_t>(103), (--b).get(103));
    EXPECT_EQ(static_cast<uint32_t>(104), (b--).get(104));
    EXPECT_EQ(static_cast<uint32_t>(105), b.get(105));
    EXPECT_FALSE(b.isSet());
}

TEST(TriStateTest, Comparison) {
    tri_int32_t a(1);
    tri_int32_t b(1);
    tri_int32_t c(2);
    tri_int32_t d;

    EXPECT_EQ(a, b);
    EXPECT_FALSE((a != b));
    EXPECT_TRUE(!(a != b));
    EXPECT_NE(-1, a);

    EXPECT_LT(a, c);
    EXPECT_LT(a, 3);

    EXPECT_GT(c, b);
    EXPECT_GT(c, 0);

    EXPECT_LE(a, 1);
    EXPECT_LE(a, c);
    EXPECT_LE(a, 3);
    EXPECT_LE(a, b);

    EXPECT_GE(c, b);
    EXPECT_GE(b, a);
    EXPECT_GE(c, 0);
    EXPECT_GE(c, 2);

    EXPECT_FALSE((a == d).isSet());
    EXPECT_FALSE((a >= d).isSet());
    EXPECT_FALSE((a <= d).isSet());
    EXPECT_FALSE((a != d).isSet());
    EXPECT_FALSE((a > d).isSet());
    EXPECT_FALSE((a < d).isSet());

    //EXPECT_EQ(a, d); // will cause runtime failure
    // due to converting a not-set TriState<bool> to bool
}

TEST(TriStateTest, CompoundAssign) {
    tri_uint32_t x;

    x += 10;
    EXPECT_FALSE(x.isSet());
    x -= 10;
    EXPECT_FALSE(x.isSet());
    x *= 10;
    EXPECT_FALSE(x.isSet());
    x /= 10;
    EXPECT_FALSE(x.isSet());
    x &= 10;
    EXPECT_FALSE(x.isSet());
    x |= 10;
    EXPECT_FALSE(x.isSet());
    x %= 10;
    EXPECT_FALSE(x.isSet());
    x <<= 10;
    EXPECT_FALSE(x.isSet());
    x >>= 10;
    EXPECT_FALSE(x.isSet());

    tri_int32_t y, z, w;
#define TEST_COMPOUND_ASSIGN(a, op, op_c, b) \
    y = z = a; \
    w = b; \
    y op_c b; \
    EXPECT_TRUE(y.isSet()); \
    EXPECT_EQ(y, (a op b)); \
    EXPECT_EQ(y, (z op b)); \
    EXPECT_EQ(y, (a op w));

    TEST_COMPOUND_ASSIGN(123, +, +=, 456);
    TEST_COMPOUND_ASSIGN(123, -, -=, 456);
    TEST_COMPOUND_ASSIGN(123, *, *=, 456);
    TEST_COMPOUND_ASSIGN(123, /, /=, 456);
    TEST_COMPOUND_ASSIGN(123, |, |=, 456);
    TEST_COMPOUND_ASSIGN(123, &, &=, 456);
    TEST_COMPOUND_ASSIGN(123, ^, ^=, 456);
    TEST_COMPOUND_ASSIGN(123, %, %=, 456);
#undef TEST_COMPOUND_ASSIGN
    y = z = 123;
    w = 10;
    y <<= 10;
    EXPECT_TRUE(y.isSet());
    EXPECT_EQ(123 << 10, y);

    y = z = 12345;
    w = 10;
    y >>= 10;
    EXPECT_TRUE(y.isSet());
    EXPECT_EQ(12345 >> 10, y);
}

TEST(TriStateTest, UnaryOperation) {
    tri_int16_t p;
    EXPECT_FALSE((!p).isSet());
    EXPECT_FALSE((-p).isSet());
    EXPECT_FALSE((~p).isSet());

    tri_int16_t q(1234);
    EXPECT_TRUE((!q).isSet());
    EXPECT_EQ(!static_cast<int16_t>(1234), (!q));

    tri_int16_t r(1234);
    EXPECT_TRUE((-r).isSet());
    EXPECT_EQ(-1234, (-r));

    tri_int16_t s(1234);
    EXPECT_TRUE((~s).isSet());
    EXPECT_EQ(~static_cast<int16_t>(1234), ~s);
}

