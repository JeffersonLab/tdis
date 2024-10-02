#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <string>
#include <vector>


// Include the SplitDataString function
// Assuming it is declared in a header file or above the tests


TEST_CASE("SplitDataString splits an empty string", "[SplitDataString]") {
    std::string line = "";
    std::vector<std::string> expected = {};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString splits a string with only whitespace", "[SplitDataString]") {
    std::string line = "    \t   \n  ";
    std::vector<std::string> expected = {};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString splits a single word", "[SplitDataString]") {
    std::string line = "word";
    std::vector<std::string> expected = {"word"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString splits multiple words separated by spaces", "[SplitDataString]") {
    std::string line = "This is a test";
    std::vector<std::string> expected = {"This", "is", "a", "test"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles leading and trailing whitespace", "[SplitDataString]") {
    std::string line = "   leading and trailing   ";
    std::vector<std::string> expected = {"leading", "and", "trailing"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles multiple consecutive whitespace characters", "[SplitDataString]") {
    std::string line = "Multiple\t\tspaces\tand\nnewlines\n\n";
    std::vector<std::string> expected = {"Multiple", "spaces", "and", "newlines"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles tabs and newlines", "[SplitDataString]") {
    std::string line = "Line1\nLine2\tLine3";
    std::vector<std::string> expected = {"Line1", "Line2", "Line3"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles special characters", "[SplitDataString]") {
    std::string line = "Symbols !@#$%^&*() are kept";
    std::vector<std::string> expected = {"Symbols", "!@#$%^&*()", "are", "kept"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles non-ASCII characters", "[SplitDataString]") {
    std::string line = "こんにちは 世界";
    std::vector<std::string> expected = {"こんにちは", "世界"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles mixed whitespace characters", "[SplitDataString]") {
    std::string line = "Mix\tof \tspaces\nand\t tabs\n";
    std::vector<std::string> expected = {"Mix", "of", "spaces", "and", "tabs"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles numeric data separated by tabs", "[SplitDataString]") {
    std::string line = "\t312.019\t4.03231e-08\t1\t67\t3\t0.0100074\t";
    std::vector<std::string> expected = {"312.019", "4.03231e-08", "1", "67", "3", "0.0100074"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles a mixture of whitespace and non-whitespace", "[SplitDataString]") {
    std::string line = "  Mixed \t content\nwith  multiple   \twhitespace characters ";
    std::vector<std::string> expected = {"Mixed", "content", "with", "multiple", "whitespace", "characters"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles strings with no whitespace", "[SplitDataString]") {
    std::string line = "NoWhitespaceAtAll";
    std::vector<std::string> expected = {"NoWhitespaceAtAll"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles strings with only whitespace between words", "[SplitDataString]") {
    std::string line = "Word1 Word2 Word3";
    std::vector<std::string> expected = {"Word1", "Word2", "Word3"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles strings with emojis and special Unicode characters", "[SplitDataString]") {
    std::string line = "😀 Emoji test 🚀 with spaces";
    std::vector<std::string> expected = {"😀", "Emoji", "test", "🚀", "with", "spaces"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles strings with carriage returns", "[SplitDataString]") {
    std::string line = "Line1\r\nLine2\r\nLine3";
    std::vector<std::string> expected = {"Line1", "Line2", "Line3"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}


TEST_CASE("SplitDataString handles multiple consecutive spaces", "[SplitDataString]") {
    std::string line = "word1  word2   word3";
    std::vector<std::string> expected = {"word1", "word2", "word3"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles tabs and spaces between words", "[SplitDataString]") {
    std::string line = "word1\t\tword2 \t  word3";
    std::vector<std::string> expected = {"word1", "word2", "word3"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles trailing whitespace", "[SplitDataString]") {
    std::string line = "word1 word2 word3   ";
    std::vector<std::string> expected = {"word1", "word2", "word3"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles leading and trailing whitespace", "[SplitDataString]") {
    std::string line = "   word1 word2 word3   ";
    std::vector<std::string> expected = {"word1", "word2", "word3"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString does not produce empty tokens for multiple consecutive whitespace", "[SplitDataString]") {
    std::string line = "word1    word2\t\t\tword3\n\n\nword4";
    std::vector<std::string> expected = {"word1", "word2", "word3", "word4"};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}

TEST_CASE("SplitDataString handles string with only whitespace", "[SplitDataString]") {
    std::string line = "     \t\t   \n\n  ";
    std::vector<std::string> expected = {};
    auto result = SplitDataString(line);
    REQUIRE(result == expected);
}
