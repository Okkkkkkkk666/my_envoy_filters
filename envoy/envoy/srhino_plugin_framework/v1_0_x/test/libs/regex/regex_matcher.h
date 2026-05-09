#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/libs/regex/regex_matcher.h"

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Regex {

using namespace SrhinoPluginFramework::Libs::Regex;
using testing::_;
using testing::A;
using testing::Return;

class MockRegexMatcher : public SrhinoPluginFramework::Libs::Regex::RegexMatcher {
public:
  MockRegexMatcher() {
    EXPECT_CALL(*this, init(_, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*this, match(_, _, _, _))
        .WillRepeatedly(
            testing::Invoke([&](const char* data, size_t len, std::vector<MatchResult>& results,
                                EncodingType encode) -> bool {
              if (!match_results_.empty()) {
                results = match_results_;
                return true;
              }
              return false;
            })
        );
    EXPECT_CALL(*this, match(A<const std::string&>(), _, _))
        .WillRepeatedly(
            testing::Invoke([&](const std::string& data, std::vector<MatchResult>& results,
                                EncodingType encode) -> bool {
              if (!match_results_.empty()) {
                results = match_results_;
                return true;
              }
              return false;
            })
        );
    EXPECT_CALL(*this, match(A<const std::string_view&>(), _, _))
        .WillRepeatedly(
            testing::Invoke([&](const std::string_view& data, std::vector<MatchResult>& results,
                                EncodingType encode) -> bool {
              if (!match_results_.empty()) {
                results = match_results_;
                return true;
              }
              return false;
            })
        );
    EXPECT_CALL(*this, streamOpen(_, _, _))
        .WillRepeatedly(
            testing::Invoke([&](Regex::StreamContext& ctx, uint32_t, EncodingType) -> bool {
              ctx.id = malloc(32);
              return true;
            }));
    EXPECT_CALL(*this, streamScan(_, _, _))
        .WillRepeatedly(testing::Invoke([&](Regex::StreamContext& ctx, const std::string_view& data,
                                            std::vector<Regex::MatchResult>& results) -> bool {
          results = match_results_;
          return true;
        }));
    EXPECT_CALL(*this, streamClose(_, _))
        .WillRepeatedly(testing::Invoke(
            [&](Regex::StreamContext& ctx, std::vector<Regex::MatchResult>&) -> bool {
              if (ctx.id) {
                free(ctx.id);
                ctx.id = nullptr;
              }
              return true;
            }));

  }
public:
  MOCK_METHOD(bool, init, (const std::vector<ExpressionView>&, std::vector<ExpressionView>&), ());
  MOCK_METHOD(bool, addExpression, (const ExpressionView&), ());
  MOCK_METHOD(bool, delExpression, (const uint32_t), ());
  MOCK_METHOD(bool, match, (const char*, size_t, std::vector<MatchResult>&, EncodingType), ());
  MOCK_METHOD(bool, match, (const std::string&, std::vector<MatchResult>&, EncodingType), ());
  MOCK_METHOD(bool, match, (const std::string_view&, std::vector<MatchResult>&, EncodingType),
              ());
  MOCK_METHOD(bool, streamOpen, (StreamContext&, uint32_t, EncodingType), ());
  MOCK_METHOD(bool, streamScan,
              (StreamContext&, const std::string_view&, std::vector<MatchResult>&), ());
  MOCK_METHOD(bool, streamClose, (StreamContext&, std::vector<MatchResult>&), ());
  MOCK_METHOD(bool, vectorMatch,
              (const std::vector<const char*>&, const std::vector<uint32_t>&,
               std::vector<MatchResult>&, EncodingType),
              ());
  MOCK_METHOD(RegexMode, mode, (), (const));
  MOCK_METHOD(bool, care_position, (), (const));
  MOCK_METHOD(const std::string&, db_path, (), (const));
public:
  void mockSetMatchResults(const std::vector<Regex::MatchResult>& match_results) {
    match_results_ = match_results;
  }

private:
  std::vector<Regex::MatchResult> match_results_;
};

} // namespace Regex
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework