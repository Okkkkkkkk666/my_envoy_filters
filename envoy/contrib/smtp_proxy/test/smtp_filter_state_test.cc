#include "contrib/smtp_proxy/smtp_filter.h"

#include "test/mocks/network/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {
namespace {

using testing::_;
using testing::NiceMock;

class SmtpFilterStateTest : public testing::Test {
protected:
  SmtpFilterStateTest() {
    config_ = std::make_shared<SmtpConfig>("smtp", 1024);
    filter_ = std::make_unique<SmtpFilter>(config_);
    filter_->initializeReadFilterCallbacks(callbacks_);
    EXPECT_EQ(filter_->onNewConnection(), Network::FilterStatus::Continue);
  }

  Network::FilterStatus feed(const std::string& payload) {
    Buffer::OwnedImpl data(payload);
    return filter_->onData(data, false);
  }

  std::shared_ptr<SmtpConfig> config_;
  std::unique_ptr<SmtpFilter> filter_;
  NiceMock<Network::MockReadFilterCallbacks> callbacks_;
};

TEST_F(SmtpFilterStateTest, AcceptsValidSequenceWithDataStreaming) {
  EXPECT_EQ(feed("EHLO mx.example\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("MAIL FROM:<a@example.com>\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("RCPT TO:<b@example.com>\r\n"), Network::FilterStatus::Continue);

  EXPECT_EQ(feed("DATA\r\n"), Network::FilterStatus::Continue);

  EXPECT_EQ(feed("hello\r\nworld\r\n.\r\n"), Network::FilterStatus::Continue);

  EXPECT_EQ(feed("MAIL FROM:<c@example.com>\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("RCPT TO:<d@example.com>\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("DATA\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("body\r\n.\r\n"), Network::FilterStatus::Continue);
}

TEST_F(SmtpFilterStateTest, RejectsBadSequenceAndClosesConnection) {
  EXPECT_CALL(callbacks_.connection_, write(_, false)).Times(1);
  EXPECT_CALL(callbacks_.connection_, close(Network::ConnectionCloseType::FlushWrite)).Times(1);

  EXPECT_EQ(feed("DATA\r\n"), Network::FilterStatus::StopIteration);

  EXPECT_EQ(feed("EHLO mx.example\r\n"), Network::FilterStatus::StopIteration);
}

TEST_F(SmtpFilterStateTest, RsetReturnsSessionToMailFromState) {
  EXPECT_EQ(feed("EHLO mx.example\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("MAIL FROM:<a@example.com>\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("RCPT TO:<b@example.com>\r\n"), Network::FilterStatus::Continue);

  EXPECT_EQ(feed("RSET\r\n"), Network::FilterStatus::Continue);

  EXPECT_EQ(feed("MAIL FROM:<c@example.com>\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("RCPT TO:<d@example.com>\r\n"), Network::FilterStatus::Continue);
}

TEST_F(SmtpFilterStateTest, QuitClosesConnectionImmediately) {
  EXPECT_EQ(feed("EHLO mx.example\r\n"), Network::FilterStatus::Continue);
  EXPECT_CALL(callbacks_.connection_, close(Network::ConnectionCloseType::NoFlush)).Times(1);

  EXPECT_EQ(feed("QUIT\r\n"), Network::FilterStatus::StopIteration);
  EXPECT_EQ(feed("EHLO again.example\r\n"), Network::FilterStatus::StopIteration);
}

TEST_F(SmtpFilterStateTest, StartTlsRequiresEhloAfterUpgrade) {
  EXPECT_CALL(callbacks_.connection_, addBytesSentCallback(_)).Times(1);
  EXPECT_CALL(callbacks_.connection_, write(_, false)).Times(1);

  EXPECT_EQ(feed("EHLO mx.example\r\n"), Network::FilterStatus::Continue);
  EXPECT_EQ(feed("STARTTLS\r\n"), Network::FilterStatus::StopIteration);

  EXPECT_CALL(callbacks_.connection_, write(_, false)).Times(1);
  EXPECT_CALL(callbacks_.connection_, close(Network::ConnectionCloseType::FlushWrite)).Times(1);
  EXPECT_EQ(feed("MAIL FROM:<a@example.com>\r\n"), Network::FilterStatus::StopIteration);
}

TEST_F(SmtpFilterStateTest, StartTlsBeforeEhloIsRejectedByStateMachine) {
  EXPECT_CALL(callbacks_.connection_, write(_, false)).Times(1);
  EXPECT_CALL(callbacks_.connection_, close(Network::ConnectionCloseType::FlushWrite)).Times(1);

  EXPECT_EQ(feed("STARTTLS\r\n"), Network::FilterStatus::StopIteration);
}

} // namespace
} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
