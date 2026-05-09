#include "contrib/mysql_proxy/filters/network/source/mysql_resultset.h"

#include "contrib/mysql_proxy/filters/network/source/mysql_utils.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MySQLProxy {

namespace {

bool isEofPacket(const Buffer::Instance& payload) {
  if (payload.length() == 0) {
    return false;
  }
  uint8_t marker = 0;
  Buffer::OwnedImpl copy;
  copy.add(payload);
  if (BufferHelper::readUint8(copy, marker) != DecodeStatus::Success) {
    return false;
  }
  return marker == EOF_MARKER && payload.length() < 9;
}

bool isErrPacket(const Buffer::Instance& payload) {
  uint8_t marker = 0;
  Buffer::OwnedImpl copy;
  copy.add(payload);
  return BufferHelper::readUint8(copy, marker) == DecodeStatus::Success && marker == ERR_MARKER;
}

bool isOkPacket(const Buffer::Instance& payload) {
  if (payload.length() == 0) {
    return false;
  }
  uint8_t marker = 0;
  Buffer::OwnedImpl copy;
  copy.add(payload);
  return BufferHelper::readUint8(copy, marker) == DecodeStatus::Success && marker == MYSQL_RESP_OK;
}

} // namespace

void MySQLResultsetRewriter::startQuery(const SelectDecryptPlan& plan) {
  plan_ = plan;
  state_ = State::AwaitingHeader;
  expected_columns_ = 0;
  column_index_ = 0;
  columns_.clear();
  decrypt_indexes_.clear();
  ENVOY_LOG(trace, "mysql_proxy: resultset rewriter armed for db='{}' table='{}' wildcard={}",
            plan_.database, plan_.table, plan_.wildcard);
}

void MySQLResultsetRewriter::clear() {
  state_ = State::Idle;
  expected_columns_ = 0;
  column_index_ = 0;
  columns_.clear();
  decrypt_indexes_.clear();
}

bool MySQLResultsetRewriter::shouldDecrypt(const ColumnInfo& column, size_t index) const {
  const std::string column_db = QueryRewriter::canonicalIdentifier(column.database);
  const std::string column_table = QueryRewriter::canonicalIdentifier(column.table);
  const std::string column_name = QueryRewriter::canonicalIdentifier(
      column.original_name.empty() ? column.name : column.original_name);

  if (!plan_.wildcard) {
    if (index >= plan_.projected_columns.size() ||
        plan_.projected_columns[index] != column_name) {
      return false;
    }
  }

  if (!plan_.database.empty() && !column_db.empty() && plan_.database != column_db) {
    return false;
  }
  if (!plan_.table.empty() && !column_table.empty() && plan_.table != column_table) {
    return false;
  }

  for (const auto& rule : rules_) {
    if (QueryRewriter::canonicalIdentifier(rule.table) != column_table ||
        QueryRewriter::canonicalIdentifier(rule.column) != column_name) {
      continue;
    }
    const std::string rule_db = QueryRewriter::canonicalIdentifier(rule.database);
    if (!rule_db.empty() && !column_db.empty() && rule_db != column_db) {
      continue;
    }
    ENVOY_LOG(trace,
              "mysql_proxy: resultset column matched for decrypt index={} db='{}' table='{}' column='{}'",
              index, column_db, column_table, column_name);
    return true;
  }
  ENVOY_LOG(trace,
            "mysql_proxy: resultset column skipped index={} db='{}' table='{}' column='{}'",
            index, column_db, column_table, column_name);
  return false;
}

bool MySQLResultsetRewriter::parseColumnDefinition(Buffer::OwnedImpl& payload, ColumnInfo& info) const {
  std::string catalog;
  if (BufferHelper::readLengthEncodedString(payload, catalog) != DecodeStatus::Success) {
    return false;
  }
  if (BufferHelper::readLengthEncodedString(payload, info.database) != DecodeStatus::Success ||
      BufferHelper::readLengthEncodedString(payload, info.table) != DecodeStatus::Success) {
    return false;
  }
  std::string original_table;
  if (BufferHelper::readLengthEncodedString(payload, original_table) != DecodeStatus::Success ||
      BufferHelper::readLengthEncodedString(payload, info.name) != DecodeStatus::Success ||
      BufferHelper::readLengthEncodedString(payload, info.original_name) != DecodeStatus::Success) {
    return false;
  }
  return true;
}

bool MySQLResultsetRewriter::rewriteRow(Buffer::OwnedImpl& payload, Buffer::OwnedImpl& rewritten,
                                        ::Envoy::Extensions::Common::FPE::FPEInterface& fpe,
                                        bool& changed) const {
  for (size_t i = 0; i < columns_.size(); ++i) {
    uint8_t marker = 0;
    if (BufferHelper::peekUint8(payload, marker) != DecodeStatus::Success) {
      return false;
    }
    if (marker == LENENCODINT_1BYTE) {
      payload.drain(1);
      BufferHelper::addUint8(rewritten, LENENCODINT_1BYTE);
      continue;
    }

    std::string value;
    if (BufferHelper::readLengthEncodedString(payload, value) != DecodeStatus::Success) {
      return false;
    }
    
    const std::string& db_name = QueryRewriter::canonicalIdentifier(columns_[i].database);
    const std::string& table_name = QueryRewriter::canonicalIdentifier(columns_[i].table);
    const std::string& column_name = QueryRewriter::canonicalIdentifier(
        columns_[i].original_name.empty() ? columns_[i].name : columns_[i].original_name);
      
    if (decrypt_indexes_.find(i) != decrypt_indexes_.end()) {
      std::string context = db_name + "." + table_name + "." + column_name;
      value = fpe.decrypt(value, context);
      changed = true;
    }
    BufferHelper::addLengthEncodedString(rewritten, value);
  }
  return true;
}

bool MySQLResultsetRewriter::rewritePacket(const Buffer::Instance& packet_payload,
                                           Buffer::OwnedImpl& rewritten,
                                           ::Envoy::Extensions::Common::FPE::FPEInterface& fpe,
                                           bool& changed,
                                           bool& completed) {
  changed = false;
  completed = false;
  rewritten.add(packet_payload);

  if (state_ == State::Idle) {
    return true;
  }
  if (isErrPacket(packet_payload)) {
    clear();
    completed = true;
    return true;
  }

  Buffer::OwnedImpl payload;
  payload.add(packet_payload);
  if (state_ == State::AwaitingHeader) {
    if (BufferHelper::readLengthEncodedInteger(payload, expected_columns_) != DecodeStatus::Success) {
      clear();
      completed = true;
      return true;
    }
    ENVOY_LOG(trace, "mysql_proxy: resultset header expects {} columns", expected_columns_);
    state_ = State::AwaitingColumns;
    rewritten.drain(rewritten.length());
    rewritten.add(packet_payload);
    return true;
  }

  if (state_ == State::AwaitingColumns) {
    if (isEofPacket(packet_payload)) {
      state_ = State::AwaitingRows;
      if (expected_columns_ == 0) {
        clear();
        completed = true;
      }
      return true;
    }

    // MySQL 8 may negotiate CLIENT_DEPRECATE_EOF and omit the EOF packet between
    // column definitions and row data. Once we've seen the expected number of
    // columns, treat the current packet as the first row packet.
    if (column_index_ >= expected_columns_) {
      state_ = State::AwaitingRows;
    } else {
      ColumnInfo column;
      if (!parseColumnDefinition(payload, column)) {
        clear();
        completed = true;
        return true;
      }
      ENVOY_LOG(trace,
                "mysql_proxy: resultset column definition index={} db='{}' table='{}' name='{}' original='{}'",
                column_index_, column.database, column.table, column.name, column.original_name);
      columns_.push_back(column);
      if (shouldDecrypt(column, column_index_)) {
        decrypt_indexes_.insert(column_index_);
      }
      ++column_index_;
      return true;
    }
  }

  if (state_ == State::AwaitingRows) {
    if (isEofPacket(packet_payload) || isOkPacket(packet_payload)) {
      clear();
      completed = true;
      return true;
    }

    rewritten.drain(rewritten.length());
    Buffer::OwnedImpl row_payload;
    row_payload.add(packet_payload);
    if (!rewriteRow(row_payload, rewritten, fpe, changed)) {
      rewritten.drain(rewritten.length());
      rewritten.add(packet_payload);
    }
  }

  return true;
}

} // namespace MySQLProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
