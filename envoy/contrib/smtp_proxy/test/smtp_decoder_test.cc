#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include "contrib/smtp_proxy/smtp_decoder.h"
#include "gtest/gtest.h"
#include "source/common/buffer/buffer_impl.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {
namespace {

struct CommandEvent {
  std::string command;
  std::string args;
};

class RecordingCallbacks : public SmtpDecoderCallbacks {
public:
  void onCommand(absl::string_view command, absl::string_view args) override {
    commands.push_back({std::string(command), std::string(args)});
  }
  void onStartTlsRequested() override { starttls_requested = true; }
  void onQuitRequested() override { quit_requested = true; }
  void onResetRequested() override { reset_requested = true; }

  void onDataChunk(uint64_t chunk_size) override { chunks.push_back(chunk_size); }

  void onProtocolError(absl::string_view error_msg) override {
    errors.push_back(std::string(error_msg));
  }

  std::vector<CommandEvent> commands;
  std::vector<uint64_t> chunks;
  std::vector<std::string> errors;
  bool starttls_requested{false};
  bool quit_requested{false};
  bool reset_requested{false};
};

TEST(SmtpDecoderTest, ParsesBasicCommands) {
  RecordingCallbacks callbacks;
  SmtpDecoder decoder(callbacks, 1024);
  decoder.session().state = State::WaitHeloOrEhlo;

  Buffer::OwnedImpl input(
      "EHLO example.com\r\n"
      "MAIL FROM:<alice@example.com>\r\n"
      "RCPT TO:<bob@example.com>\r\n"
      "NOOP\r\n");

  decoder.onData(input);

  ASSERT_EQ(callbacks.errors.size(), 0);
  ASSERT_EQ(callbacks.commands.size(), 4);
  EXPECT_EQ(callbacks.commands[0].command, "EHLO");
  EXPECT_EQ(callbacks.commands[0].args, "example.com");
  EXPECT_EQ(callbacks.commands[1].command, "MAIL FROM");
  EXPECT_EQ(callbacks.commands[1].args, "<alice@example.com>");
  EXPECT_EQ(callbacks.commands[2].command, "RCPT TO");
  EXPECT_EQ(callbacks.commands[2].args, "<bob@example.com>");
  EXPECT_EQ(callbacks.commands[3].command, "NOOP");
  EXPECT_EQ(callbacks.commands[3].args, "");
  EXPECT_FALSE(callbacks.starttls_requested);
  EXPECT_FALSE(callbacks.quit_requested);
}

TEST(SmtpDecoderTest, HandlesSplitInputAcrossCalls) {
  RecordingCallbacks callbacks;
  SmtpDecoder decoder(callbacks, 1024);
  decoder.session().state = State::WaitHeloOrEhlo;

  Buffer::OwnedImpl partial("EHLO split.example");
  decoder.onData(partial);

  EXPECT_EQ(callbacks.commands.size(), 0);
  EXPECT_EQ(callbacks.errors.size(), 0);

  Buffer::OwnedImpl rest("\r\n");
  decoder.onData(rest);

  ASSERT_EQ(callbacks.commands.size(), 1);
  EXPECT_EQ(callbacks.commands[0].command, "EHLO");
  EXPECT_EQ(callbacks.commands[0].args, "split.example");
  EXPECT_EQ(callbacks.errors.size(), 0);
}

TEST(SmtpDecoderTest, StreamsDataAndReturnsToCommandMode) {
  RecordingCallbacks callbacks;
  SmtpDecoder decoder(callbacks, 1024);
  decoder.session().state = State::WaitHeloOrEhlo;

  Buffer::OwnedImpl prelude(
      "EHLO mx.example\r\n"
      "MAIL FROM:<alice@example.com>\r\n"
      "RCPT TO:<bob@example.com>\r\n"
      "DATA\r\n");
  decoder.onData(prelude);

  Buffer::OwnedImpl chunk1("hello\r\nwor");
  decoder.onData(chunk1);

  Buffer::OwnedImpl chunk2("ld\r\n.\r\nQUIT\r\n");
  decoder.onData(chunk2);

  ASSERT_EQ(callbacks.errors.size(), 0);
  ASSERT_EQ(callbacks.commands.size(), 5);
  EXPECT_EQ(callbacks.commands[3].command, "DATA");
  EXPECT_EQ(callbacks.commands[3].args, "");
  EXPECT_EQ(callbacks.commands[4].command, "DATA_END");
  EXPECT_EQ(callbacks.commands[4].args, "");
  EXPECT_TRUE(callbacks.quit_requested);

  ASSERT_EQ(callbacks.chunks.size(), 2);
  EXPECT_EQ(callbacks.chunks[0], 10);
  EXPECT_EQ(callbacks.chunks[1], 2);
}

TEST(SmtpDecoderTest, ReportsErrorWhenLineExceedsLimit) {
  RecordingCallbacks callbacks;
  SmtpDecoder decoder(callbacks, 8);
  decoder.session().state = State::WaitHeloOrEhlo;

  Buffer::OwnedImpl input("EHLO 123456789\r\n");
  decoder.onData(input);

  ASSERT_EQ(callbacks.errors.size(), 1);
  EXPECT_EQ(callbacks.errors[0], "Line length exceeds the maximum allowed");
  EXPECT_TRUE(callbacks.commands.empty());
}

TEST(SmtpDecoderTest, EmitsStartTlsExplicitCallback) {
  RecordingCallbacks callbacks;
  SmtpDecoder decoder(callbacks, 1024);
  decoder.session().state = State::WaitHeloOrEhlo;

  Buffer::OwnedImpl input("EHLO mx.example\r\nSTARTTLS\r\n");
  decoder.onData(input);

  EXPECT_TRUE(callbacks.starttls_requested);
  ASSERT_EQ(callbacks.commands.size(), 1);
  EXPECT_EQ(callbacks.commands[0].command, "EHLO");
}

TEST(SmtpDecoderTest, EmitsResetExplicitCallback) {
  RecordingCallbacks callbacks;
  SmtpDecoder decoder(callbacks, 1024);
  decoder.session().state = State::WaitHeloOrEhlo;

  Buffer::OwnedImpl input("EHLO mx.example\r\nRSET\r\n");
  decoder.onData(input);

  EXPECT_TRUE(callbacks.reset_requested);
  ASSERT_EQ(callbacks.commands.size(), 1);
  EXPECT_EQ(callbacks.commands[0].command, "EHLO");
  EXPECT_FALSE(callbacks.quit_requested);
  EXPECT_FALSE(callbacks.starttls_requested);
}

} // namespace
} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
