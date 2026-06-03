#include <gtest/gtest.h>
#include "utils.h"

// ============================================================
//  Utils — full suite
// ============================================================

// ============================================================
//  Trim
// ============================================================

TEST(UtilsTest, TrimLeadingSpaces) {
    EXPECT_EQ(Utils::Trim("  hello"), "hello");
}

TEST(UtilsTest, TrimTrailingSpaces) {
    EXPECT_EQ(Utils::Trim("hello  "), "hello");
}

TEST(UtilsTest, TrimBothEnds) {
    EXPECT_EQ(Utils::Trim("  hello  "), "hello");
}

TEST(UtilsTest, TrimAlreadyTrimmed) {
    EXPECT_EQ(Utils::Trim("world"), "world");
}

TEST(UtilsTest, TrimWhitespaceOnly) {
    EXPECT_EQ(Utils::Trim("   "), "");
}

TEST(UtilsTest, TrimEmpty) {
    EXPECT_EQ(Utils::Trim(""), "");
}

TEST(UtilsTest, TrimTabCharacters) {
    EXPECT_EQ(Utils::Trim("\t\thello\t"), "hello");
}

TEST(UtilsTest, TrimNewlines) {
    EXPECT_EQ(Utils::Trim("\nhello\r\n"), "hello");
}

TEST(UtilsTest, TrimMixedWhitespace) {
    EXPECT_EQ(Utils::Trim(" \t\n  hello world  \r\n "), "hello world");
}

TEST(UtilsTest, TrimPreservesInternalSpaces) {
    // Internal whitespace must not be stripped
    EXPECT_EQ(Utils::Trim("  hello world  "), "hello world");
}

TEST(UtilsTest, TrimSingleChar) {
    EXPECT_EQ(Utils::Trim("x"), "x");
}

TEST(UtilsTest, TrimSingleCharWithPadding) {
    EXPECT_EQ(Utils::Trim("  x  "), "x");
}

// ============================================================
//  Split
// ============================================================

TEST(UtilsTest, SplitBasicComma) {
    auto parts = Utils::Split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(UtilsTest, SplitTwoElements) {
    auto parts = Utils::Split("hello,world", ',');
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "hello");
    EXPECT_EQ(parts[1], "world");
}

TEST(UtilsTest, SplitSingleElement) {
    auto parts = Utils::Split("hello", ',');
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "hello");
}

TEST(UtilsTest, SplitBySlash) {
    auto parts = Utils::Split("dir/sub/file.txt", '/');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "dir");
    EXPECT_EQ(parts[1], "sub");
    EXPECT_EQ(parts[2], "file.txt");
}

TEST(UtilsTest, SplitLeadingDelimiter) {
    // ",a,b" → ["", "a", "b"]
    auto parts = Utils::Split(",a,b", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "");
    EXPECT_EQ(parts[1], "a");
    EXPECT_EQ(parts[2], "b");
}

TEST(UtilsTest, SplitSpaceSeparated) {
    auto parts = Utils::Split("one two three", ' ');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "one");
    EXPECT_EQ(parts[1], "two");
    EXPECT_EQ(parts[2], "three");
}

TEST(UtilsTest, SplitDoesNotModifyOriginalParts) {
    auto parts = Utils::Split("foo,bar", ',');
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "foo");
    EXPECT_EQ(parts[1], "bar");
}

// ============================================================
//  TryParse
// ============================================================

TEST(UtilsTest, TryParseIntValid) {
    auto v = Utils::TryParse<int>("42");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 42);
}

TEST(UtilsTest, TryParseIntNegative) {
    auto v = Utils::TryParse<int>("-7");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, -7);
}

TEST(UtilsTest, TryParseIntZero) {
    auto v = Utils::TryParse<int>("0");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 0);
}

TEST(UtilsTest, TryParseIntInvalidAlpha) {
    auto v = Utils::TryParse<int>("abc");
    EXPECT_FALSE(v.has_value());
}

TEST(UtilsTest, TryParseIntInvalidTrailingChars) {
    // "42abc" — not a pure integer
    auto v = Utils::TryParse<int>("42abc");
    EXPECT_FALSE(v.has_value());
}

TEST(UtilsTest, TryParseIntEmpty) {
    auto v = Utils::TryParse<int>("");
    EXPECT_FALSE(v.has_value());
}

TEST(UtilsTest, TryParseFloatValid) {
    auto v = Utils::TryParse<float>("3.14");
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, 3.14f, 1e-4f);
}

TEST(UtilsTest, TryParseFloatNegative) {
    auto v = Utils::TryParse<float>("-2.5");
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, -2.5f, 1e-6f);
}

TEST(UtilsTest, TryParseFloatInvalid) {
    auto v = Utils::TryParse<float>("xyz");
    EXPECT_FALSE(v.has_value());
}

TEST(UtilsTest, TryParseDoubleValid) {
    auto v = Utils::TryParse<double>("1.23456789");
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, 1.23456789, 1e-9);
}

// ============================================================
//  ToString
// ============================================================

TEST(UtilsTest, ToStringIntPositive) {
    EXPECT_EQ(Utils::ToString(42), "42");
}

TEST(UtilsTest, ToStringIntNegative) {
    EXPECT_EQ(Utils::ToString(-1), "-1");
}

TEST(UtilsTest, ToStringIntZero) {
    EXPECT_EQ(Utils::ToString(0), "0");
}

TEST(UtilsTest, ToStringFloatContainsDecimal) {
    std::string s = Utils::ToString(1.5f);
    EXPECT_FALSE(s.empty());
    // std::to_string(1.5f) → "1.500000" — decimal point must be present
    EXPECT_NE(s.find('.'), std::string::npos);
}

TEST(UtilsTest, ToStringDoubleIsNonEmpty) {
    std::string s = Utils::ToString(3.14);
    EXPECT_FALSE(s.empty());
}

// ============================================================
//  SetLogEnabled smoke test (doesn't crash)
// ============================================================

TEST(UtilsTest, SetLogEnabledDoesNotCrash) {
    EXPECT_NO_FATAL_FAILURE(Utils::SetLogEnabled(false));
    EXPECT_NO_FATAL_FAILURE(Utils::SetLogEnabled(true));
}
