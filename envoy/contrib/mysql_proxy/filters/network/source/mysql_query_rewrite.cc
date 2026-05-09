#include "contrib/mysql_proxy/filters/network/source/mysql_query_rewrite.h"

#include <algorithm>
#include <cctype>

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MySQLProxy {

namespace {

bool isIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

std::string lowercase(std::string value) {
  for (char& c : value) {
    c = std::tolower(static_cast<unsigned char>(c));
  }
  return value;
}

} // namespace

std::string QueryRewriter::trim(const std::string& value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

bool QueryRewriter::iequals(char lhs, char rhs) {
  return std::tolower(static_cast<unsigned char>(lhs)) ==
         std::tolower(static_cast<unsigned char>(rhs));
}

std::string QueryRewriter::canonicalIdentifier(std::string value) {
  value = trim(value);
  if (value.size() >= 2 && value.front() == '`' && value.back() == '`') {
    value = value.substr(1, value.size() - 2);
  }
  return lowercase(value);
}

size_t QueryRewriter::findKeyword(const std::string& sql, const std::string& keyword, size_t start) {
  for (size_t pos = start; pos + keyword.size() <= sql.size(); ++pos) {
    bool match = true;
    for (size_t i = 0; i < keyword.size(); ++i) {
      if (!iequals(sql[pos + i], keyword[i])) {
        match = false;
        break;
      }
    }
    if (!match) {
      continue;
    }
    const bool left_ok = pos == 0 || !isIdentifierChar(sql[pos - 1]);
    const bool right_ok =
        pos + keyword.size() >= sql.size() || !isIdentifierChar(sql[pos + keyword.size()]);
    if (left_ok && right_ok) {
      return pos;
    }
  }
  return std::string::npos;
}

size_t QueryRewriter::findTopLevelKeyword(const std::string& sql, const std::string& keyword,
                                          size_t start) {
  bool in_single_quote = false;
  bool in_backtick = false;
  int depth = 0;
  for (size_t pos = start; pos + keyword.size() <= sql.size(); ++pos) {
    const char c = sql[pos];
    if (in_single_quote) {
      if (c == '\\' && pos + 1 < sql.size()) {
        ++pos;
        continue;
      }
      if (c == '\'') {
        in_single_quote = false;
      }
      continue;
    }
    if (in_backtick) {
      if (c == '`') {
        in_backtick = false;
      }
      continue;
    }
    if (c == '\'') {
      in_single_quote = true;
      continue;
    }
    if (c == '`') {
      in_backtick = true;
      continue;
    }
    if (c == '(') {
      ++depth;
      continue;
    }
    if (c == ')') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (depth == 0) {
      size_t match = findKeyword(sql, keyword, pos);
      if (match == pos) {
        return pos;
      }
    }
  }
  return std::string::npos;
}

std::vector<std::string> QueryRewriter::splitTopLevel(const std::string& input, char delimiter) {
  std::vector<std::string> parts;
  bool in_single_quote = false;
  bool in_backtick = false;
  int depth = 0;
  size_t start = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    const char c = input[i];
    if (in_single_quote) {
      if (c == '\\' && i + 1 < input.size()) {
        ++i;
        continue;
      }
      if (c == '\'') {
        in_single_quote = false;
      }
      continue;
    }
    if (in_backtick) {
      if (c == '`') {
        in_backtick = false;
      }
      continue;
    }
    if (c == '\'') {
      in_single_quote = true;
      continue;
    }
    if (c == '`') {
      in_backtick = true;
      continue;
    }
    if (c == '(') {
      ++depth;
      continue;
    }
    if (c == ')') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (depth == 0 && c == delimiter) {
      parts.push_back(input.substr(start, i - start));
      start = i + 1;
    }
  }
  parts.push_back(input.substr(start));
  return parts;
}

bool QueryRewriter::parseIdentifierToken(const std::string& input, size_t& pos,
                                         std::string& identifier) {
  while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
    ++pos;
  }
  if (pos >= input.size()) {
    return false;
  }

  if (input[pos] == '`') {
    const size_t end = input.find('`', pos + 1);
    if (end == std::string::npos) {
      return false;
    }
    identifier = input.substr(pos, end - pos + 1);
    pos = end + 1;
    return true;
  }

  const size_t start = pos;
  while (pos < input.size() && isIdentifierChar(input[pos])) {
    ++pos;
  }
  if (start == pos) {
    return false;
  }
  identifier = input.substr(start, pos - start);
  return true;
}

bool QueryRewriter::parseQualifiedIdentifier(const std::string& input, size_t& pos,
                                             std::string& database, std::string& table) {
  std::string first;
  if (!parseIdentifierToken(input, pos, first)) {
    return false;
  }
  while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
    ++pos;
  }
  if (pos < input.size() && input[pos] == '.') {
    ++pos;
    std::string second;
    if (!parseIdentifierToken(input, pos, second)) {
      return false;
    }
    database = canonicalIdentifier(first);
    table = canonicalIdentifier(second);
    return true;
  }
  database.clear();
  table = canonicalIdentifier(first);
  return true;
}

QueryRewriter::SqlLiteral QueryRewriter::parseLiteral(const std::string& token) {
  SqlLiteral literal;
  const std::string trimmed = trim(token);
  if (trimmed.size() >= 2 && trimmed.front() == '\'' && trimmed.back() == '\'') {
    literal.valid = true;
    literal.quoted = true;
    literal.value = unescapeSqlString(trimmed.substr(1, trimmed.size() - 2));
    return literal;
  }

  bool digits_only = !trimmed.empty();
  for (char c : trimmed) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      digits_only = false;
      break;
    }
  }
  if (digits_only) {
    literal.valid = true;
    literal.value = trimmed;
  }
  return literal;
}

std::string QueryRewriter::unescapeSqlString(const std::string& token) {
  std::string value;
  value.reserve(token.size());
  for (size_t i = 0; i < token.size(); ++i) {
    if (token[i] == '\\' && i + 1 < token.size()) {
      value.push_back(token[i + 1]);
      ++i;
      continue;
    }
    value.push_back(token[i]);
  }
  return value;
}

std::string QueryRewriter::escapeSqlString(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (char c : value) {
    if (c == '\'' || c == '\\') {
      escaped.push_back('\\');
    }
    escaped.push_back(c);
  }
  return escaped;
}

std::string QueryRewriter::encodeLiteral(const SqlLiteral& literal, const std::string& value) {
  if (literal.quoted) {
    return "'" + escapeSqlString(value) + "'";
  }
  return value;
}

bool QueryRewriter::matchRule(const std::string& database, const std::string& table,
                              const std::string& column) const {
  const std::string normalized_db = canonicalIdentifier(database);
  const std::string normalized_table = canonicalIdentifier(table);
  const std::string normalized_column = canonicalIdentifier(column);
  for (const auto& rule : rules_) {
    if (!canonicalIdentifier(rule.table).empty() &&
        canonicalIdentifier(rule.table) != normalized_table) {
      continue;
    }
    if (!canonicalIdentifier(rule.column).empty() &&
        canonicalIdentifier(rule.column) != normalized_column) {
      continue;
    }
    const std::string rule_db = canonicalIdentifier(rule.database);
    if (!rule_db.empty() && rule_db != normalized_db) {
      continue;
    }
    ENVOY_LOG(trace, "mysql_proxy: matched protected column rule db='{}' table='{}' column='{}'",
              rule_db, canonicalIdentifier(rule.table), canonicalIdentifier(rule.column));
    return true;
  }
  ENVOY_LOG(trace,
            "mysql_proxy: no protected column rule matched db='{}' table='{}' column='{}'",
            normalized_db, normalized_table, normalized_column);
  return false;
}

bool QueryRewriter::rewriteInsertOrUpdate(const std::string& query, const std::string& current_db,
                                          std::string& rewritten) const {
  if (rewriteInsert(query, current_db, rewritten)) {
    return true;
  }
  if (rewriteUpdate(query, current_db, rewritten)) {
    return true;
  }
  return rewriteDelete(query, current_db, rewritten);
}

bool QueryRewriter::rewriteInsert(const std::string& query, const std::string& current_db,
                                  std::string& rewritten) const {
  const size_t insert_pos = findKeyword(query, "insert");
  if (insert_pos != 0) {
    return false;
  }
  size_t pos = insert_pos + 6;
  while (pos < query.size() && std::isspace(static_cast<unsigned char>(query[pos]))) {
    ++pos;
  }
  const size_t into_pos = findKeyword(query, "into", pos);
  if (into_pos == pos) {
    pos = into_pos + 4;
  }
  std::string db_name;
  std::string table_name;
  if (!parseQualifiedIdentifier(query, pos, db_name, table_name)) {
    return false;
  }
  if (db_name.empty()) {
    db_name = current_db;
  }
  ENVOY_LOG(trace, "mysql_proxy: parsed INSERT target db='{}' table='{}'", db_name, table_name);

  while (pos < query.size() && std::isspace(static_cast<unsigned char>(query[pos]))) {
    ++pos;
  }
  if (pos >= query.size() || query[pos] != '(') {
    return false;
  }

  const size_t columns_start = pos;
  int depth = 0;
  size_t columns_end = std::string::npos;
  for (size_t i = pos; i < query.size(); ++i) {
    if (query[i] == '(') {
      ++depth;
    } else if (query[i] == ')') {
      --depth;
      if (depth == 0) {
        columns_end = i;
        break;
      }
    }
  }
  if (columns_end == std::string::npos) {
    return false;
  }

  std::vector<std::string> columns = splitTopLevel(query.substr(columns_start + 1, columns_end - columns_start - 1), ',');
  const size_t values_pos = findTopLevelKeyword(query, "values", columns_end + 1);
  if (values_pos == std::string::npos) {
    return false;
  }

  std::string output = query.substr(0, values_pos + 6);
  size_t tuple_pos = values_pos + 6;
  bool changed = false;
  while (tuple_pos < query.size()) {
    while (tuple_pos < query.size() &&
           std::isspace(static_cast<unsigned char>(query[tuple_pos]))) {
      output.push_back(query[tuple_pos++]);
    }
    if (tuple_pos >= query.size() || query[tuple_pos] != '(') {
      output.append(query.substr(tuple_pos));
      break;
    }
    const size_t tuple_start = tuple_pos;
    depth = 0;
    size_t tuple_end = std::string::npos;
    for (size_t i = tuple_pos; i < query.size(); ++i) {
      if (query[i] == '(') {
        ++depth;
      } else if (query[i] == ')') {
        --depth;
        if (depth == 0) {
          tuple_end = i;
          break;
        }
      }
    }
    if (tuple_end == std::string::npos) {
      return false;
    }
    std::vector<std::string> values =
        splitTopLevel(query.substr(tuple_start + 1, tuple_end - tuple_start - 1), ',');
    if (values.size() != columns.size()) {
      output.append(query.substr(tuple_start, tuple_end - tuple_start + 1));
    } else {
      output.push_back('(');
      for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
          output += ", ";
        }
        SqlLiteral literal = parseLiteral(values[i]);
        const std::string column_name = canonicalIdentifier(columns[i]);
        ENVOY_LOG(trace,
                  "mysql_proxy: inspecting INSERT column='{}' raw_value='{}' literal_valid={}",
                  column_name, trim(values[i]), literal.valid);
        if (literal.valid && matchRule(db_name, table_name, column_name)) {
          const std::string context = db_name + "." + table_name + "." + column_name;
          output += encodeLiteral(literal, fpe_.encrypt(literal.value, context));
          changed = true;
        } else {
          output += trim(values[i]);
        }
      }
      output.push_back(')');
    }
    tuple_pos = tuple_end + 1;
    while (tuple_pos < query.size() && std::isspace(static_cast<unsigned char>(query[tuple_pos]))) {
      output.push_back(query[tuple_pos++]);
    }
    if (tuple_pos < query.size() && query[tuple_pos] == ',') {
      output.push_back(query[tuple_pos++]);
      continue;
    }
    output.append(query.substr(tuple_pos));
    break;
  }

  if (!changed) {
    return false;
  }
  rewritten = output;
  return true;
}

bool QueryRewriter::rewriteUpdate(const std::string& query, const std::string& current_db,
                                  std::string& rewritten) const {
  const size_t update_pos = findKeyword(query, "update");
  if (update_pos != 0) {
    return false;
  }
  size_t pos = update_pos + 6;
  while (pos < query.size() && std::isspace(static_cast<unsigned char>(query[pos]))) {
    ++pos;
  }
  std::string db_name;
  std::string table_name;
  if (!parseQualifiedIdentifier(query, pos, db_name, table_name)) {
    return false;
  }
  if (db_name.empty()) {
    db_name = current_db;
  }
  ENVOY_LOG(trace, "mysql_proxy: parsed UPDATE target db='{}' table='{}'", db_name, table_name);
  const size_t set_pos = findTopLevelKeyword(query, "set", pos);
  if (set_pos == std::string::npos) {
    return false;
  }
  size_t where_pos = std::string::npos;
  size_t tail_pos = query.size();
  for (const char* keyword : {"where", "order", "limit"}) {
    const size_t keyword_pos = findTopLevelKeyword(query, keyword, set_pos + 3);
    if (keyword_pos != std::string::npos) {
      if (std::string(keyword) == "where") {
        where_pos = keyword_pos;
      }
      tail_pos = std::min(tail_pos, keyword_pos);
    }
  }

  std::string output = query.substr(0, set_pos + 3);
  if (!output.empty() && !std::isspace(static_cast<unsigned char>(output.back()))) {
    output.push_back(' ');
  }
  std::vector<std::string> assignments =
      splitTopLevel(query.substr(set_pos + 3, tail_pos - set_pos - 3), ',');
  bool changed = false;
  for (size_t i = 0; i < assignments.size(); ++i) {
    if (i > 0) {
      output += ", ";
    }
    const std::string assignment = trim(assignments[i]);
    bool in_single_quote = false;
    bool in_backtick = false;
    int depth = 0;
    size_t eq_pos = std::string::npos;
    for (size_t j = 0; j < assignment.size(); ++j) {
      const char c = assignment[j];
      if (in_single_quote) {
        if (c == '\\' && j + 1 < assignment.size()) {
          ++j;
          continue;
        }
        if (c == '\'') {
          in_single_quote = false;
        }
        continue;
      }
      if (in_backtick) {
        if (c == '`') {
          in_backtick = false;
        }
        continue;
      }
      if (c == '\'') {
        in_single_quote = true;
        continue;
      }
      if (c == '`') {
        in_backtick = true;
        continue;
      }
      if (c == '(') {
        ++depth;
        continue;
      }
      if (c == ')') {
        if (depth > 0) {
          --depth;
        }
        continue;
      }
      if (depth == 0 && c == '=') {
        eq_pos = j;
        break;
      }
    }
    if (eq_pos == std::string::npos) {
      output += assignment;
      continue;
    }
    std::string lhs = trim(assignment.substr(0, eq_pos));
    std::string rhs = trim(assignment.substr(eq_pos + 1));
    std::string lhs_db;
    std::string lhs_col;
    size_t lhs_pos = 0;
    if (!parseQualifiedIdentifier(lhs, lhs_pos, lhs_db, lhs_col)) {
      output += assignment;
      continue;
    }
    const std::string column_name = lhs_col;
    SqlLiteral literal = parseLiteral(rhs);
    if (literal.valid && matchRule(db_name, table_name, column_name)) {
      const std::string context = db_name + "." + table_name + "." + column_name;
      output += lhs + " = " + encodeLiteral(literal, fpe_.encrypt(literal.value, context));
      changed = true;
    } else {
      output += assignment;
    }
  }
  if (where_pos != std::string::npos) {
    const size_t condition_start = where_pos + 5;
    size_t condition_end = query.size();
    for (const char* keyword : {"order", "limit"}) {
      const size_t keyword_pos = findTopLevelKeyword(query, keyword, condition_start);
      if (keyword_pos != std::string::npos) {
        condition_end = std::min(condition_end, keyword_pos);
      }
    }
    output.append(query.substr(where_pos, condition_start - where_pos));
    std::string rewritten_conditions;
    bool where_changed = false;
    if (!rewriteConditionClause(query.substr(condition_start, condition_end - condition_start), db_name,
                                table_name, rewritten_conditions, where_changed)) {
      output.append(query.substr(condition_start, condition_end - condition_start));
    } else {
      output.append(rewritten_conditions);
      changed = changed || where_changed;
    }
    if (condition_end < query.size() && !output.empty() &&
        !std::isspace(static_cast<unsigned char>(output.back())) &&
        !std::isspace(static_cast<unsigned char>(query[condition_end]))) {
      output.push_back(' ');
    }
    output.append(query.substr(condition_end));
  } else {
    if (tail_pos < query.size() && !output.empty() &&
        !std::isspace(static_cast<unsigned char>(output.back())) &&
        !std::isspace(static_cast<unsigned char>(query[tail_pos]))) {
      output.push_back(' ');
    }
    output.append(query.substr(tail_pos));
  }

  if (!changed) {
    return false;
  }
  rewritten = output;
  return true;
}

bool QueryRewriter::rewriteDelete(const std::string& query, const std::string& current_db,
                                  std::string& rewritten) const {
  const size_t delete_pos = findKeyword(query, "delete");
  if (delete_pos != 0) {
    return false;
  }
  size_t pos = delete_pos + 6;
  while (pos < query.size() && std::isspace(static_cast<unsigned char>(query[pos]))) {
    ++pos;
  }
  const size_t from_pos = findKeyword(query, "from", pos);
  if (from_pos != pos) {
    return false;
  }
  pos = from_pos + 4;

  std::string db_name;
  std::string table_name;
  if (!parseQualifiedIdentifier(query, pos, db_name, table_name)) {
    return false;
  }
  if (db_name.empty()) {
    db_name = current_db;
  }
  ENVOY_LOG(trace, "mysql_proxy: parsed DELETE target db='{}' table='{}'", db_name, table_name);

  const size_t where_pos = findTopLevelKeyword(query, "where", pos);
  if (where_pos == std::string::npos) {
    return false;
  }

  size_t condition_end = query.size();
  for (const char* keyword : {"order", "limit"}) {
    const size_t keyword_pos = findTopLevelKeyword(query, keyword, where_pos + 5);
    if (keyword_pos != std::string::npos) {
      condition_end = std::min(condition_end, keyword_pos);
    }
  }

  std::string output = query.substr(0, where_pos + 5);
  std::string rewritten_conditions;
  bool changed = false;
  if (!rewriteConditionClause(query.substr(where_pos + 5, condition_end - where_pos - 5), db_name,
                              table_name, rewritten_conditions, changed) ||
      !changed) {
    return false;
  }
  output.append(rewritten_conditions);
  if (condition_end < query.size() && !output.empty() &&
      !std::isspace(static_cast<unsigned char>(output.back())) &&
      !std::isspace(static_cast<unsigned char>(query[condition_end]))) {
    output.push_back(' ');
  }
  output.append(query.substr(condition_end));
  rewritten = output;
  return true;
}

bool QueryRewriter::rewriteConditionClause(const std::string& clause, const std::string& database,
                                           const std::string& table, std::string& rewritten,
                                           bool& changed) const {
  rewritten.clear();
  changed = false;

  bool in_single_quote = false;
  bool in_backtick = false;
  int depth = 0;
  size_t segment_start = 0;
  size_t pos = 0;

  auto append_segment = [&](size_t end) {
    std::string segment = clause.substr(segment_start, end - segment_start);
    std::string rewritten_segment;
    bool segment_changed = false;
    if (rewriteComparison(segment, database, table, rewritten_segment, segment_changed)) {
      rewritten += rewritten_segment;
      changed = changed || segment_changed;
    } else {
      rewritten += segment;
    }
  };

  while (pos < clause.size()) {
    const char c = clause[pos];
    if (in_single_quote) {
      if (c == '\\' && pos + 1 < clause.size()) {
        pos += 2;
        continue;
      }
      if (c == '\'') {
        in_single_quote = false;
      }
      ++pos;
      continue;
    }
    if (in_backtick) {
      if (c == '`') {
        in_backtick = false;
      }
      ++pos;
      continue;
    }
    if (c == '\'') {
      in_single_quote = true;
      ++pos;
      continue;
    }
    if (c == '`') {
      in_backtick = true;
      ++pos;
      continue;
    }
    if (c == '(') {
      ++depth;
      ++pos;
      continue;
    }
    if (c == ')') {
      if (depth > 0) {
        --depth;
      }
      ++pos;
      continue;
    }
    if (depth == 0) {
      const size_t and_pos = findKeyword(clause, "and", pos);
      const size_t or_pos = findKeyword(clause, "or", pos);
      size_t connector_pos = std::string::npos;
      size_t connector_len = 0;
      if (and_pos == pos) {
        connector_pos = pos;
        connector_len = 3;
      } else if (or_pos == pos) {
        connector_pos = pos;
        connector_len = 2;
      }
      if (connector_pos != std::string::npos) {
        append_segment(connector_pos);
        rewritten.append(clause.substr(connector_pos, connector_len));
        pos += connector_len;
        segment_start = pos;
        continue;
      }
    }
    ++pos;
  }

  append_segment(clause.size());
  return true;
}

bool QueryRewriter::rewriteComparison(const std::string& expression, const std::string& database,
                                      const std::string& table, std::string& rewritten,
                                      bool& changed) const {
  rewritten = expression;
  changed = false;

  bool in_single_quote = false;
  bool in_backtick = false;
  int depth = 0;
  size_t eq_pos = std::string::npos;
  for (size_t i = 0; i < expression.size(); ++i) {
    const char c = expression[i];
    if (in_single_quote) {
      if (c == '\\' && i + 1 < expression.size()) {
        ++i;
        continue;
      }
      if (c == '\'') {
        in_single_quote = false;
      }
      continue;
    }
    if (in_backtick) {
      if (c == '`') {
        in_backtick = false;
      }
      continue;
    }
    if (c == '\'') {
      in_single_quote = true;
      continue;
    }
    if (c == '`') {
      in_backtick = true;
      continue;
    }
    if (c == '(') {
      ++depth;
      continue;
    }
    if (c == ')') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (depth == 0 && c == '=' && (i == 0 || expression[i - 1] != '<') &&
        (i == 0 || expression[i - 1] != '>') && (i == 0 || expression[i - 1] != '!') &&
        (i + 1 >= expression.size() || expression[i + 1] != '=')) {
      eq_pos = i;
      break;
    }
  }
  if (eq_pos == std::string::npos) {
    return false;
  }

  const std::string lhs = trim(expression.substr(0, eq_pos));
  const std::string rhs = trim(expression.substr(eq_pos + 1));
  std::string lhs_db;
  std::string lhs_col;
  size_t lhs_pos = 0;
  if (!parseQualifiedIdentifier(lhs, lhs_pos, lhs_db, lhs_col)) {
    return false;
  }
  SqlLiteral literal = parseLiteral(rhs);
  if (!literal.valid || !matchRule(database, table, lhs_col)) {
    return false;
  }

  const size_t value_start = expression.find(rhs, eq_pos + 1);
  if (value_start == std::string::npos) {
    return false;
  }
  if (literal.valid && matchRule(database, table, lhs_col)) {
    // 使用上下文相关的加密
    std::string context = database + "." + table + "." + canonicalIdentifier(lhs_col);
    rewritten.assign(expression.substr(0, value_start));
    rewritten += encodeLiteral(literal, fpe_.encrypt(literal.value, context));
    rewritten += expression.substr(value_start + rhs.size());
    changed = true;
    return true;
  }
  return false;
}

bool QueryRewriter::buildSelectPlan(const std::string& query, const std::string& current_db,
                                    SelectDecryptPlan& plan) const {
  const size_t select_pos = findKeyword(query, "select");
  if (select_pos != 0) {
    return false;
  }
  const size_t from_pos = findTopLevelKeyword(query, "from", 6);
  if (from_pos == std::string::npos) {
    return false;
  }
  std::string projection = trim(query.substr(6, from_pos - 6));
  size_t pos = from_pos + 4;
  std::string db_name;
  std::string table_name;
  if (!parseQualifiedIdentifier(query, pos, db_name, table_name)) {
    return false;
  }
  if (db_name.empty()) {
    db_name = current_db;
  }

  SelectDecryptPlan candidate;
  candidate.database = canonicalIdentifier(db_name);
  candidate.table = canonicalIdentifier(table_name);

  if (trim(projection) == "*") {
    candidate.wildcard = true;
    plan = candidate;
    return true;
  }

  std::vector<std::string> columns = splitTopLevel(projection, ',');
  for (const std::string& column_expr : columns) {
    std::string expr = trim(column_expr);
    if (expr.empty()) {
      return false;
    }
    const size_t as_pos = findTopLevelKeyword(expr, "as");
    if (as_pos != std::string::npos) {
      expr = trim(expr.substr(0, as_pos));
    }
    std::string expr_db;
    std::string expr_col;
    size_t expr_pos = 0;
    if (!parseQualifiedIdentifier(expr, expr_pos, expr_db, expr_col)) {
      return false;
    }
    candidate.projected_columns.push_back(canonicalIdentifier(expr_col));
  }

  plan = candidate;
  return true;
}

bool QueryRewriter::isUseStatement(const std::string& query, std::string& database) const {
  const size_t use_pos = findKeyword(query, "use");
  if (use_pos != 0) {
    return false;
  }
  size_t pos = 3;
  std::string db_name;
  if (!parseIdentifierToken(query, pos, db_name)) {
    return false;
  }
  database = canonicalIdentifier(db_name);
  return true;
}

} // namespace MySQLProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
