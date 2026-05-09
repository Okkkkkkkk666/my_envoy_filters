#include "contrib/common/sqlutils/source/sqlutils.h"
namespace Envoy {
namespace Extensions {
namespace Common {
namespace SQLUtils {

bool SQLUtils::setMetadata(const std::string& query, const DecoderAttributes& attr,
                           ProtobufWkt::Struct& metadata) {
  // Strip leading control bytes (sequence id or protocol prefix) and truncate at
  // the first NUL so the SQL parser receives printable SQL text.
  size_t start = 0;
  while (start < query.size()) {
    unsigned char c = static_cast<unsigned char>(query[start]);
    if (c < 0x20) {
      ++start;
      continue;
    }
    break;
  }
  std::string sql_to_parse = (start == 0) ? query : query.substr(start);

  // Truncate at first NUL if any (lexer expects C-string-like data).
  size_t first_nul = sql_to_parse.find('\0');
  if (first_nul != std::string::npos) {
    sql_to_parse.resize(first_nul);
  }

  //抓取注释内容
  std::string extracted_comment;
  size_t first_char_idx = sql_to_parse.find_first_not_of(" \t\r\n");
  if (first_char_idx != std::string::npos && sql_to_parse.compare(first_char_idx, 2, "/*") == 0) {
    size_t end_idx = sql_to_parse.find("*/", first_char_idx + 2);
    if (end_idx != std::string::npos) {
      // 提取注释内容
      extracted_comment = sql_to_parse.substr(first_char_idx + 2, end_idx - first_char_idx - 2);
    }
  }

  auto& fields = *metadata.mutable_fields();
  //塞入 Envoy Metadata
  if (!extracted_comment.empty()) {
    fields["sql_comments"].set_string_value(extracted_comment);
  }
  
  hsql::SQLParserResult result;
  hsql::SQLParser::parse(sql_to_parse, &result);

  if (!result.isValid()) {
    return false;
  }

  std::string database;
  // Check if the attributes map contains database name.
  const auto it = attr.find("database");
  if (it != attr.end()) {
    database = absl::StrCat(".", it->second);
  }

  for (auto i = 0u; i < result.size(); ++i) {
    if (result.getStatement(i)->type() == hsql::StatementType::kStmtShow) {
      continue;
    }
    hsql::TableAccessMap table_access_map;
    // Get names of accessed tables.
    result.getStatement(i)->tablesAccessed(table_access_map);
    for (auto& it : table_access_map) {
      auto& operations = *fields[it.first + database].mutable_list_value();
      // For each table get names of operations performed on that table.
      for (const auto& ot : it.second) {
        operations.add_values()->set_string_value(ot);
      }
    }
  }

  return true;
}

} // namespace SQLUtils
} // namespace Common
} // namespace Extensions
} // namespace Envoy
