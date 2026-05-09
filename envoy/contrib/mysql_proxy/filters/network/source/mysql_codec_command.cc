#include "contrib/mysql_proxy/filters/network/source/mysql_codec_command.h"

#include "envoy/buffer/buffer.h"

#include "source/common/common/logger.h"
#include "source/common/common/macros.h"

#include "contrib/mysql_proxy/filters/network/source/mysql_codec.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_utils.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MySQLProxy {

Command::Cmd Command::parseCmd(Buffer::Instance& data) {
  uint8_t cmd;
  if (BufferHelper::readUint8(data, cmd) != DecodeStatus::Success) {
    return Command::Cmd::Null;
  }
  return static_cast<Command::Cmd>(cmd);
}

void Command::setCmd(Command::Cmd cmd) { cmd_ = cmd; }

void Command::setDb(const std::string& db) { db_ = db; }

DecodeStatus Command::parseMessage(Buffer::Instance& buffer, uint32_t len) {
  Command::Cmd cmd = parseCmd(buffer);
  setCmd(cmd);
  if (cmd == Command::Cmd::Null) {
    return DecodeStatus::Failure;
  }

  switch (cmd) {
  case Command::Cmd::InitDb:
  case Command::Cmd::CreateDb:
  case Command::Cmd::DropDb: {
    std::string db;
    BufferHelper::readStringBySize(buffer, len - 1, db);
    setDb(db);
    break;
  }
  case Command::Cmd::Query:
    is_query_ = true;
    query_prefix_.clear();
    // MySQL 8 clients may negotiate query attributes and send COM_QUERY as:
    // [0x03][parameter_count lenenc][parameter_set_count lenenc][query...].
    // For queries without attributes the common prefix is 0x00 0x01.
    if (len >= 3) {
      uint8_t parameter_count = 0;
      uint8_t parameter_set_count = 0;
      if (BufferHelper::peekUint8(buffer, parameter_count) == DecodeStatus::Success &&
          parameter_count == 0) {
        Buffer::OwnedImpl copy;
        copy.add(buffer);
        copy.drain(1);
        if (BufferHelper::peekUint8(copy, parameter_set_count) == DecodeStatus::Success &&
            parameter_set_count == 1) {
          query_prefix_.push_back(static_cast<char>(parameter_count));
          query_prefix_.push_back(static_cast<char>(parameter_set_count));
          buffer.drain(2);
          len -= 2;
        }
      }
    }
    FALLTHRU;
  default:
    BufferHelper::readStringBySize(buffer, len - 1, data_);
    break;
  }
  return DecodeStatus::Success;
}

void Command::setData(const std::string& data) { data_.assign(data); }

void Command::encode(Buffer::Instance& out) const {
  BufferHelper::addUint8(out, static_cast<int>(cmd_));
  switch (cmd_) {
  case Command::Cmd::InitDb:
  case Command::Cmd::CreateDb:
  case Command::Cmd::DropDb: {
    BufferHelper::addString(out, db_);
    break;
  }
  case Command::Cmd::Query:
    if (!query_prefix_.empty()) {
      BufferHelper::addString(out, query_prefix_);
    }
    BufferHelper::addString(out, data_);
    break;
  default:
    BufferHelper::addString(out, data_);
    break;
  }
}

DecodeStatus CommandResponse::parseMessage(Buffer::Instance& buffer, uint32_t len) {
  if (BufferHelper::readStringBySize(buffer, len, data_) != DecodeStatus::Success) {
    ENVOY_LOG(debug, "error when parsing command response");
    return DecodeStatus::Failure;
  }
  return DecodeStatus::Success;
}

void CommandResponse::encode(Buffer::Instance& out) const { BufferHelper::addString(out, data_); }

} // namespace MySQLProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
