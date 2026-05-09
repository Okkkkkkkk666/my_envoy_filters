#include <fstream>
#include "envoy/registry/registry.h"

#include "source/common/srhino_plugin_framework/v1_2_x/libs/regex/regex_matcher_impl.h"

#include "gmock/gmock.h"

namespace TestSrhinoPluginFrameWork {
namespace v1_2_x {
namespace Libs {
namespace Regex {
namespace {

using namespace SrhinoPluginFramework::v1_2_x::Libs::Regex;

TEST(RegexMatcherTest, init) {
  // 模式集为空
  {
    RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
    std::vector<ExpressionView> expressions;
    std::vector<ExpressionView> failed_exprs;
    EXPECT_FALSE(regex_matcher.init(expressions, failed_exprs));
  }

  // 正则表达式
  {
    RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
    std::vector<ExpressionView> expressions;
    expressions.push_back(ExpressionView("x.{31}", 1, true));
    std::vector<ExpressionView> failed_exprs;
    EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));
  }

  // 正则表达式含有重复超过31，编译失败
  {
    RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
    std::vector<ExpressionView> expressions;
    expressions.push_back(ExpressionView("x.{32}", 1, true));
    std::vector<ExpressionView> failed_exprs;
    EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));
    EXPECT_FALSE(regex_matcher.isEngineReady());
  }
}

TEST(RegexMatcherTest, match) {
  const char* text = "Hyperscan itself doesn't have any UTF8 check on your input and instead "
                     "process your input as separate byte characters no matter HS_FLAG_UTF8 is "
                     "on or off. Users should guarantee UTF8 validness in UTF8 mode.";
  const size_t text_len = strlen(text);

  RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
  std::vector<ExpressionView> expressions;
  expressions.push_back(ExpressionView(R"(([\w]+_)+[\w]+)", 1, true));
  std::vector<ExpressionView> failed_exprs;
  EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));
  // 匹配成功，match的三个重载
  {
    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, text_len, results));
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id(), 1);
    int start = results[0].start();
    int end = results[0].end();
    EXPECT_EQ(std::string(text + start, end - start), "HS_FLAG_UTF8");
  }
  {
    std::vector<MatchResult> results;
    std::string test_string(text, text_len);
    EXPECT_TRUE(regex_matcher.match(test_string, results));
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id(), 1);
    int start = results[0].start();
    int end = results[0].end();
    EXPECT_EQ(std::string(test_string.data() + start, end - start), "HS_FLAG_UTF8");
  }
  {
    std::vector<MatchResult> results;
    std::string_view test_view(text, text_len);
    EXPECT_TRUE(regex_matcher.match(test_view, results));
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id(), 1);
    int start = results[0].start();
    int end = results[0].end();
    EXPECT_EQ(std::string(test_view.data() + start, end - start), "HS_FLAG_UTF8");
  }

  // 匹配不成功
  {
    // 正则表达式匹配不到
    const std::string new_text("this string is a test");
    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(new_text, results));
    EXPECT_EQ(results.size(), 0);
  }
  {
    // 输入为空
    const std::string new_text;
    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(new_text, results));
    EXPECT_EQ(results.size(), 0);
  }
}

// 匹配起始位置测试
TEST(RegexMatcherTest, startOfMatch) {

  // 记录起始位置
  {
    std::vector<ExpressionView> expressions;
    expressions.push_back(ExpressionView(R"(letter)", 1, true));
    RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
    std::vector<ExpressionView> failed_exprs;
    EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(std::string("A letter in book!"), results));
    EXPECT_EQ(results.size(), 1);
    EXPECT_NE(results[0].start(), 0);
  }
  // 不记录起始位置
  {
    std::vector<ExpressionView> expressions;
    expressions.push_back(ExpressionView(R"(letter)", 1, true));
    RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, false);
    std::vector<ExpressionView> failed_exprs;
    EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(std::string("A letter in book!"), results));
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].start(), 0);
  }
}

// 忽略大小写测试
TEST(RegexMatcherTest, caseLess) {

  // 忽略大小写
  {
    std::vector<ExpressionView> expressions;
    expressions.push_back(ExpressionView(R"(letter)", 1, true));
    RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
    std::vector<ExpressionView> failed_exprs;
    EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

    {
      std::vector<MatchResult> results;
      EXPECT_TRUE(regex_matcher.match(std::string("A letter in book!"), results));
      EXPECT_EQ(results.size(), 1);
    }

    {
      std::vector<MatchResult> results;
      EXPECT_TRUE(regex_matcher.match(std::string("A LeTter in book!"), results));
      EXPECT_EQ(results.size(), 1);
    }
  }

  // 不忽略大小写
  {
    std::vector<ExpressionView> expressions;
    expressions.push_back(ExpressionView(R"(letter)", 1, false));
    RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
    std::vector<ExpressionView> failed_exprs;
    EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

    {
      std::vector<MatchResult> results;
      EXPECT_TRUE(regex_matcher.match(std::string("A letter in book!"), results));
      EXPECT_EQ(results.size(), 1);
    }

    {
      std::vector<MatchResult> results;
      EXPECT_TRUE(regex_matcher.match(std::string("A LeTter in book!"), results));
      EXPECT_EQ(results.size(), 0);
    }
  }
}

TEST(RegexMatcherTest, addDelExpression) {
  const std::string text("Hyperscan itself doesn't have any UTF8 check on your input and instead "
                     "process your input as separate byte characters no matter HS_FLAG_UTF8 is "
                     "on or off. Users should guarantee UTF8 validness in UTF8 mode.");

  RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
  std::vector<ExpressionView> expressions;
  expressions.push_back(ExpressionView(R"(abcdef)", 1, true));
  std::vector<ExpressionView> failed_exprs;
  EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

  // init
  {
    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, results));
    EXPECT_EQ(results.size(), 0);
  }
  // add Expression
  {
    ExpressionView delta_expr(R"(([\w]+_)+[\w]+)", 2, true);
    regex_matcher.addExpression(delta_expr);

    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, results));
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id(), 2);
  }

  // del Expression
  {
    regex_matcher.delExpression(2);

    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, results));
    EXPECT_EQ(results.size(), 0);
  }

}

// 测试流模式，匹配部分分别在不同的两个流中的情况
TEST(RegexMatcherTest, streamMode) {
  const std::string text_stream1("Hyperscan itself doesn't have any UTF8 check on your input and instead "
                     "process your input as separate byte charac");
  const std::string text_stream2("ters no matter HS_FLAG_UTF8 is "
                     "on or off. Users should guarantee UTF8 validness in UTF8 mode.");

  RegexMatcherImpl regex_matcher(RegexMode::RegexModeStream, true);
  std::vector<ExpressionView> expressions;
  expressions.push_back(ExpressionView(R"(characters)", 1, true));
  std::vector<ExpressionView> failed_exprs;
  EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

  StreamContext ctx;
  EXPECT_TRUE(regex_matcher.streamOpen(ctx, 1));
  std::vector<MatchResult> results;
  EXPECT_TRUE(regex_matcher.streamScan(ctx, text_stream1, results));
  EXPECT_EQ(results.size(), 0);
  EXPECT_TRUE(regex_matcher.streamScan(ctx, text_stream2, results));
  EXPECT_TRUE(regex_matcher.streamClose(ctx, results));
  // for (auto it : results) {
  //   std::cout << it.id() << ": " << it.start() << ": " << it.end() << std::endl;
  // }
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].end() - results[0].start(), strlen("characters"));
}

// 测试vector模式，匹配部分分别在不同的两个块中的情况
TEST(RegexMatcherTest, vectorMode) {
  const std::vector<const char*> text = {
    R"(Hyperscan itself doesn't have any UTF8 check on your input and instead 
process your input as separate byte charac)",
    R"(ters no matter HS_FLAG_UTF8 is 
on or off. Users should guarantee UTF8 validness in UTF8 mode.)"};
  const std::vector<uint32_t> length = {
    static_cast<uint32_t>(strlen(text[0])),
    static_cast<uint32_t>(strlen(text[1])),
  };

  RegexMatcherImpl regex_matcher(RegexMode::RegexModeVector, true);
  std::vector<ExpressionView> expressions;
  expressions.push_back(ExpressionView(R"(characters)", 1, true));
  std::vector<ExpressionView> failed_exprs;
  EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

  std::vector<MatchResult> results;
  EXPECT_TRUE(regex_matcher.vectorMatch(text, length, results));
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].end() - results[0].start(), strlen("characters"));
}

bool readPattern(const char *txtFile, std::vector<ExpressionView>& expressions) {
  std::ifstream inFile(txtFile);
  if (!inFile.good()) {
    std::cerr << "ERROR: Can't open txtFile file \"" << txtFile << "\"" << std::endl;
    return false;
  }

  uint32_t id = 1;
  for (unsigned i = 1; !inFile.eof(); ++i) {
    std::string line;
    getline(inFile, line);

    if (line.empty() || line[0] == '#') {
      if (line[1] == '#') {
        break;
      }
      continue;
    }

    std::cout << "line: " << i << ": id : " << id << " : " << line << std::endl;
    expressions.push_back(ExpressionView(line, id++, true));
  }
  std::cout << "patterns size: " << expressions.size() << std::endl;
  return !expressions.empty();
}

bool readText(const char *txtFile, std::string& text) {
  std::ifstream inFile(txtFile);
  if (!inFile.good()) {
    std::cerr << "ERROR: Can't open txtFile file \"" << txtFile << "\"" << std::endl;
    return false;
  }
  for (unsigned i = 1; !inFile.eof(); ++i) {
    std::string line;
    getline(inFile, line);

    if (!text.empty()) {
      text.append("\n");
    }
    text.append(line);
  }
  std::cout << "text size: " << text.size() << std::endl;
  return !text.empty();
}

TEST(RegexMatcherTest, encoding) {
  const char* pattern_file = "/tmp/test_data/pcre/all_encoding.txt";
  std::vector<ExpressionView> expressions;

  EXPECT_TRUE(readPattern(pattern_file, expressions));

  RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true);
  std::vector<ExpressionView> failed_exprs;
  EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

  {
    std::string text;
    EXPECT_TRUE(readText("/tmp/test_data/text/iso8859_1.txt", text));
    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, results, EncodingType::ISO_8859_1));
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id(), 1);
  }
  
  {
    std::string text;
    EXPECT_TRUE(readText("/tmp/test_data/text/utf8.txt", text));
    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, results, EncodingType::UTF8));
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id(), 2);
  }
  
  {
    std::string text;
    EXPECT_TRUE(readText("/tmp/test_data/text/gbk_no_gb2312.txt", text));
    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, results, EncodingType::GBK));
    for(auto it : results) {
      std::cout << "match it: " << it.id() << ", start: " << it.start() << ", end: " << it.end() << std::endl;
    }
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id(), 3);
  }
  
  {
    std::string text;
    EXPECT_TRUE(readText("/tmp/test_data/text/gb2312_no_ascii.txt", text));
    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, results, EncodingType::GB2312));
    for(auto it : results) {
      std::cout << "match it: " << it.id() << ", start: " << it.start() << ", end: " << it.end() << std::endl;
    }
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id(), 4);
  }
  
}

// 测试从磁盘加载hyperscan
TEST(RegexMatcherTest, Serialize) {
  const std::string db_path("test_db");
  const std::string text = R"(my email is "srhino@126.com",  and my phone number is 18579763265 .)";
  std::vector<ExpressionView> expressions;
  // 邮箱
  expressions.emplace_back(ExpressionView(R"([\s\'\":][a-zA-Z0-9_-]+@[a-zA-Z0-9_-]+(\.[a-zA-Z0-9_-]+)[\s\'\"])", 0, true));
  // 手机号
  expressions.emplace_back(ExpressionView(R"((\s|\'|\"|:)(1(3|4|5|6|7|8|9)\d{9}(\s|\'|\")))", 1, true));

  // 运行两次，第一次生成db文件，第二次从db加载hyperscan
  for( int i : {1,2}) {
    std::cout << "round: " << i << std::endl;
    RegexMatcherImpl regex_matcher(RegexMode::RegexModeBlock, true, db_path);
    std::vector<ExpressionView> failed_exprs;
    EXPECT_TRUE(regex_matcher.init(expressions, failed_exprs));

    std::vector<MatchResult> results;
    EXPECT_TRUE(regex_matcher.match(text, results));
    for(auto it : results) {
      std::cout << "match it: " << it.id() << ", start: " << it.start() << ", end: " << it.end() << std::endl;
    }
    EXPECT_EQ(results.size(), 2);
  }
}

} // namespace
} // namespace Regex
} // namespace Libs
} // namespace v1_2_x
} // namespace TestSrhinoPluginFrameWork
