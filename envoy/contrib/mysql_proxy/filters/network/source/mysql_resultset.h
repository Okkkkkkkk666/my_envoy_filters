#pragma once

#include <set>
#include <string>
#include <vector>

#include "envoy/buffer/buffer.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/logger.h"

#include "contrib/common/fpe/fpe.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_query_rewrite.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MySQLProxy {

class MySQLResultsetRewriter : public Logger::Loggable<Logger::Id::filter> {
public:
  explicit MySQLResultsetRewriter(const std::vector<ProtectedColumnRule>& rules)
      : rules_(rules) {}

  void startQuery(const SelectDecryptPlan& plan);
  void clear();
  bool active() const { return state_ != State::Idle; }
  bool rewritePacket(const Buffer::Instance& payload, Buffer::OwnedImpl& rewritten,
                     ::Envoy::Extensions::Common::FPE::FPEInterface& fpe, bool& changed,
                     bool& completed);

private:
  enum class State {
    Idle,
    AwaitingHeader,
    AwaitingColumns,
    AwaitingRows,
  };

  struct ColumnInfo {
    std::string database;
    std::string table;
    std::string name;
    std::string original_name;
  };

  bool shouldDecrypt(const ColumnInfo& column, size_t index) const;
  bool parseColumnDefinition(Buffer::OwnedImpl& payload, ColumnInfo& info) const;
  bool rewriteRow(Buffer::OwnedImpl& payload, Buffer::OwnedImpl& rewritten,
                  ::Envoy::Extensions::Common::FPE::FPEInterface& fpe, bool& changed) const;

  const std::vector<ProtectedColumnRule>& rules_;
  State state_{State::Idle};
  SelectDecryptPlan plan_;
  uint64_t expected_columns_{0};
  uint64_t column_index_{0};
  std::vector<ColumnInfo> columns_;
  std::set<size_t> decrypt_indexes_;
};

} // namespace MySQLProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
