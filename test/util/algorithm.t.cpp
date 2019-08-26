#include "../testing.t.hpp"

#include <regex>

using namespace otto;
using namespace otto::util::algorithm;

TEST_CASE ("String algorithms", "[algorithm]") {
  SECTION ("string_replace") {
    std::string short_str = "A short string";
    std::string long_str =
      "A very long string containing more characters than the short string, "
      "and the word string again. After that, i mention string strings more "
      "strings the string string and then continue filling in meaningless "
      "words. This string is mainly this long so benchmarks actually have any meaning";
    std::string same_char = "AAAAAAAAAAA"; // 11 * A

    SECTION ("1") {
      string_replace(short_str, "s", "WHAT");
      REQUIRE(short_str == "A WHAThort WHATtring");
    }

    SECTION ("2") {
      string_replace(short_str, "s", "");
      REQUIRE(short_str == "A hort tring");
    }

    SECTION ("3") {
      string_replace(short_str, "A", "B");
      REQUIRE(short_str == "B short string");
    }

    SECTION ("4") {
      string_replace(short_str, "nonexistant", "nothing");
      REQUIRE(short_str == "A short string");
    }

    SECTION ("5") {
      string_replace(same_char, "AA", "B");
      REQUIRE(same_char == "BBBBBA");
    }

  }
}
