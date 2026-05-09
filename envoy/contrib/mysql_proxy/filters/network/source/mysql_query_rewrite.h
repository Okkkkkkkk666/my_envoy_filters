#pragma once

#include <memory>
#include <string>
#include <vector>
#include "source/common/common/logger.h"
#include "contrib/common/fpe/fpe.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MySQLProxy {

struct ProtectedColumnRule {
  std::string database;
  std::string table;
  std::string column;
};

struct SelectDecryptPlan {
  std::string database;
  std::string table;
  bool wildcard{false};
  std::vector<std::string> projected_columns;
};

class QueryRewriter : public Logger::Loggable<Logger::Id::filter> {
public:
  QueryRewriter(const std::vector<ProtectedColumnRule>& rules,
                ::Envoy::Extensions::Common::FPE::FPEInterface& fpe)
      : rules_(rules), fpe_(fpe) {}

  bool rewriteInsertOrUpdate(const std::string& query, const std::string& current_db,
                             std::string& rewritten) const;
  bool buildSelectPlan(const std::string& query, const std::string& current_db,
                       SelectDecryptPlan& plan) const;
  bool isUseStatement(const std::string& query, std::string& database) const;
  static std::string canonicalIdentifier(std::string value);
  static std::string trim(const std::string& value);

private:
  struct SqlLiteral {
    bool valid{false};
    bool quoted{false};
    std::string value;
  };

  bool rewriteInsert(const std::string& query, const std::string& current_db,
                     std::string& rewritten) const;
  bool rewriteUpdate(const std::string& query, const std::string& current_db,
                     std::string& rewritten) const;
  bool rewriteDelete(const std::string& query, const std::string& current_db,
                     std::string& rewritten) const;
  bool matchRule(const std::string& database, const std::string& table,
                 const std::string& column) const;
  bool rewriteConditionClause(const std::string& clause, const std::string& database,
                              const std::string& table, std::string& rewritten,
                              bool& changed) const;
  bool rewriteComparison(const std::string& expression, const std::string& database,
                         const std::string& table, std::string& rewritten,
                         bool& changed) const;
  static bool iequals(char lhs, char rhs);
  static size_t findKeyword(const std::string& sql, const std::string& keyword, size_t start = 0);
  static size_t findTopLevelKeyword(const std::string& sql, const std::string& keyword,
                                    size_t start = 0);
  static std::vector<std::string> splitTopLevel(const std::string& input, char delimiter);
  static bool parseIdentifierToken(const std::string& input, size_t& pos, std::string& identifier);
  static bool parseQualifiedIdentifier(const std::string& input, size_t& pos, std::string& database,
                                       std::string& table);
  static SqlLiteral parseLiteral(const std::string& token);
  static std::string encodeLiteral(const SqlLiteral& literal, const std::string& value);
  static std::string unescapeSqlString(const std::string& token);
  static std::string escapeSqlString(const std::string& value);

  const std::vector<ProtectedColumnRule>& rules_;
  ::Envoy::Extensions::Common::FPE::FPEInterface& fpe_;
};

using QueryRewriterPtr = std::unique_ptr<QueryRewriter>;

} // namespace MySQLProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
