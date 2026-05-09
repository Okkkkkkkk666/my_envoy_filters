#include <fstream>
#include "envoy/registry/registry.h"

#include "source/common/srhino_plugin_framework/v1_2_x/libs/regex/regex_replacer_impl.h"

#include "gmock/gmock.h"

namespace TestSrhinoPluginFrameWork {
namespace v1_2_x {
namespace Libs {
namespace Regex {
namespace {

using namespace SrhinoPluginFramework::v1_2_x::Libs::Regex;

TEST(RegexReplacerTest, init) {
  {
    RegexReplacerImpl regex_replacer(true, "abcd", "david", EncodingType::UTF8);
    std::string text("i can say abcdefg");
    EXPECT_TRUE(regex_replacer.replace(text, EncodingType::UTF8));
    EXPECT_EQ(text, "i can say davidefg");
  }
  {
    RegexReplacerImpl regex_replacer(true, R"(["'][^"']+["'])", "read", EncodingType::UTF8);
    std::string text(R"(i can "say" abcdefg)");
    EXPECT_TRUE(regex_replacer.replace(text, EncodingType::UTF8));
    EXPECT_EQ(text, "i can read abcdefg");
  }
}

struct ReplaceTestData{
  std::string match_pattern;
  std::string replace_str;
  std::string text;
  std::string expect_text;
};

bool readReplaceTestData(const char *txtFile, ReplaceTestData& test_data) {
  std::ifstream inFile(txtFile);
  if (!inFile.good()) {
    std::cerr << "ERROR: Can't open txtFile file \"" << txtFile << "\"" << std::endl;
    return false;
  }

  uint32_t id = 0;
  for (unsigned i = 1; !inFile.eof(); ++i) {
    std::string line;
    getline(inFile, line);

    if (line.empty() || line[0] == '#') {
      if (line[1] == '#') {
        break;
      }
      continue;
    }
    id++;
    switch(id) {
      case 1:
        test_data.match_pattern = line;
        break;
      case 2:
        test_data.replace_str = line;
        break;
      case 3:
        test_data.text = line;
        break;
      case 4:
        test_data.expect_text = line;
        break;
      default:
        return false;
        break;
    }

    std::cout << "line: " << i << ": id : " << id << " : " << line << std::endl;
  }
  return (id == 4);
}

void test_encoding(const char* test_file, EncodingType type) {
  ReplaceTestData test_data;
  EXPECT_TRUE(readReplaceTestData(test_file, test_data));
  RegexReplacerImpl regex_replacer(true, test_data.match_pattern, test_data.replace_str, type);
  EXPECT_TRUE(regex_replacer.replace(test_data.text, type));
  std::cout << test_data.text << std::endl;
  EXPECT_EQ(test_data.text, test_data.expect_text);
}
// 不同字符集测试
TEST(RegexReplacerTest, encoding) {
  test_encoding( "/tmp/test_data/replace/iso8859_1.txt", EncodingType::ISO_8859_1);
  test_encoding( "/tmp/test_data/replace/utf8.txt", EncodingType::UTF8);
  test_encoding( "/tmp/test_data/replace/gbk_no_gb2312.txt", EncodingType::GBK);
  test_encoding( "/tmp/test_data/replace/gb2312_no_ascii.txt", EncodingType::GB2312);
}

} // namespace
} // namespace Regex
} // namespace Libs
} // namespace v1_2_x
} // namespace TestSrhinoPluginFrameWork
