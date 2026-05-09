#include "test/mocks/server/mocks.h"
#include "test/mocks/server/instance.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/api/mocks.h"
#include "test/test_common/utility.h"
#include "test/test_common/environment.h"
#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_server.pb.h"
#include "filters/source/extensions/filters/http/super_glue/server_config.h"
#include "filters/source/extensions/filters/http/super_glue/super_glue_server.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

namespace v3 = envoy::extensions::filters::http::super_glue::v3;

class SuperGlueServerTest : public testing::Test {
public:
  SuperGlueServerTest() = default;

  void setup() {
    v3::SuperGlueServerGlobal global_config;

    global_config_ = std::make_shared<ServerFilterGlobalConfig>(global_config, dispathcer_,
                                                                factory_context_.serverScope());

    filter_ = std::make_shared<ServerFilter>(global_config_, context_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

  std::shared_ptr<ServerFilterGlobalConfig> global_config_;
  std::shared_ptr<ServerFilter> filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Event::MockDispatcher> dispathcer_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;
};

// 验证报头是否添加成功
TEST_F(SuperGlueServerTest, AddHeader) {
  setup();
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.addReference(Http::LowerCaseString("content-type"), "jjsoe");
  headers.addReference(Http::LowerCaseString("iths"), "eiuqhssoe");
  Buffer::OwnedImpl data_("nothing here");
  Buffer::OwnedImpl decoding_buffer;

  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::OK, _, _, _, _)).Times(2);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, true));
  EXPECT_EQ(Http::FilterDataStatus::StopIterationAndBuffer, filter_->decodeData(data_, true));
  filter_->onStreamComplete();
}

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy