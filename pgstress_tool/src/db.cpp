#include "db.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <random>
#include <sstream>
#include <string>

namespace pgstress {
namespace {

using PgResultPtr = std::unique_ptr<PGresult, decltype(&PQclear)>;

PgResultPtr MakeResult(PGresult *res) { return PgResultPtr(res, &PQclear); }

bool IsCommandOk(const PGresult *res) {
  if (res == nullptr) {
    return false;
  }
  const ExecStatusType st = PQresultStatus(res);
  return st == PGRES_COMMAND_OK;
}

bool IsTuplesOk(const PGresult *res) {
  return res != nullptr && PQresultStatus(res) == PGRES_TUPLES_OK;
}

std::string BuildError(PGconn *conn, PGresult *res, const char *prefix) {
  std::ostringstream oss;
  oss << prefix << ": ";
  if (res != nullptr) {
    const char *msg = PQresultErrorMessage(res);
    if (msg != nullptr && msg[0] != '\0') {
      oss << msg;
      return oss.str();
    }
  }
  const char *msg = PQerrorMessage(conn);
  if (msg != nullptr) {
    oss << msg;
  }
  return oss.str();
}

bool ExecSimple(PGconn *conn, const std::string &sql, std::string *error) {
  PgResultPtr res = MakeResult(PQexec(conn, sql.c_str()));
  if (!IsCommandOk(res.get()) && !(res != nullptr && PQresultStatus(res.get()) == PGRES_TUPLES_OK)) {
    if (error != nullptr) {
      *error = BuildError(conn, res.get(), "exec failed");
    }
    return false;
  }
  return true;
}

std::string RandomPayload(std::mt19937_64 &rng, int len) {
  static const char alphabet[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::uniform_int_distribution<int> pick(0, static_cast<int>(sizeof(alphabet) - 2));
  std::string out;
  out.reserve(static_cast<size_t>(len));
  for (int i = 0; i < len; ++i) {
    out.push_back(alphabet[pick(rng)]);
  }
  return out;
}

} // namespace

std::string BuildConnInfo(const Config &config) {
  std::ostringstream oss;
  oss << "host=" << config.host << " port=" << config.port
      << " user=" << config.user << " dbname=" << config.dbname
      << " connect_timeout=5";
  if (!config.password.empty()) {
    oss << " password=" << config.password;
  }
  return oss.str();
}

PGconn *Connect(const Config &config, std::string *error) {
  const std::string conninfo = BuildConnInfo(config);
  PGconn *conn = PQconnectdb(conninfo.c_str());
  if (PQstatus(conn) != CONNECTION_OK) {
    if (error != nullptr) {
      *error = std::string("connect failed: ") + PQerrorMessage(conn);
    }
    PQfinish(conn);
    return nullptr;
  }
  return conn;
}

bool EnsureSchemaAndInitData(const Config &config, std::string *error) {
  PGconn *conn = Connect(config, error);
  if (conn == nullptr) {
    return false;
  }

  const std::string create_sql =
      "CREATE TABLE IF NOT EXISTS " + config.table +
      " (id BIGSERIAL PRIMARY KEY, payload TEXT NOT NULL, "
      "created_at TIMESTAMPTZ NOT NULL DEFAULT now())";

  if (!ExecSimple(conn, create_sql, error)) {
    PQfinish(conn);
    return false;
  }

  const std::string count_sql = "SELECT COUNT(*) FROM " + config.table;
  PgResultPtr count_res = MakeResult(PQexec(conn, count_sql.c_str()));
  if (!IsTuplesOk(count_res.get()) || PQntuples(count_res.get()) != 1) {
    if (error != nullptr) {
      *error = BuildError(conn, count_res.get(), "count rows failed");
    }
    PQfinish(conn);
    return false;
  }

  int64_t existing_rows = std::atoll(PQgetvalue(count_res.get(), 0, 0));

  std::mt19937_64 rng(config.seed ^ 0x9E3779B97F4A7C15ULL);
  const std::string insert_sql = "INSERT INTO " + config.table + "(payload) VALUES ($1)";

  for (int64_t i = existing_rows; i < config.init_rows; ++i) {
    const std::string payload = RandomPayload(rng, 32);
    const char *values[1] = {payload.c_str()};
    PgResultPtr ins = MakeResult(PQexecParams(conn, insert_sql.c_str(), 1, nullptr,
                                              values, nullptr, nullptr, 0));
    if (!IsCommandOk(ins.get())) {
      if (error != nullptr) {
        *error = BuildError(conn, ins.get(), "init insert failed");
      }
      PQfinish(conn);
      return false;
    }
  }

  PQfinish(conn);
  return true;
}

bool FetchMaxId(PGconn *conn, const std::string &table, int64_t *max_id,
                std::string *error) {
  const std::string sql = "SELECT COALESCE(MAX(id), 0) FROM " + table;
  PgResultPtr res = MakeResult(PQexec(conn, sql.c_str()));
  if (!IsTuplesOk(res.get()) || PQntuples(res.get()) != 1) {
    if (error != nullptr) {
      *error = BuildError(conn, res.get(), "query max id failed");
    }
    return false;
  }
  *max_id = std::atoll(PQgetvalue(res.get(), 0, 0));
  return true;
}

bool ExecInsertReturningId(PGconn *conn, const std::string &table,
                           const std::string &payload, int64_t *new_id,
                           std::string *error) {
  const std::string sql = "INSERT INTO " + table +
                          "(payload) VALUES ($1) RETURNING id";
  const char *values[1] = {payload.c_str()};
  PgResultPtr res = MakeResult(
      PQexecParams(conn, sql.c_str(), 1, nullptr, values, nullptr, nullptr, 0));

  if (!IsTuplesOk(res.get()) || PQntuples(res.get()) != 1) {
    if (error != nullptr) {
      *error = BuildError(conn, res.get(), "insert failed");
    }
    return false;
  }

  *new_id = std::atoll(PQgetvalue(res.get(), 0, 0));
  return true;
}

bool ExecSelectById(PGconn *conn, const std::string &table, int64_t id,
                    std::string *error) {
  const std::string sql = "SELECT id, payload FROM " + table + " WHERE id=$1";
  const std::string id_str = std::to_string(id);
  const char *values[1] = {id_str.c_str()};
  PgResultPtr res = MakeResult(
      PQexecParams(conn, sql.c_str(), 1, nullptr, values, nullptr, nullptr, 0));

  if (!IsTuplesOk(res.get())) {
    if (error != nullptr) {
      *error = BuildError(conn, res.get(), "select by id failed");
    }
    return false;
  }
  return true;
}

bool ExecUpdateById(PGconn *conn, const std::string &table, int64_t id,
                    const std::string &payload, std::string *error) {
  const std::string sql = "UPDATE " + table + " SET payload=$1 WHERE id=$2";
  const std::string id_str = std::to_string(id);
  const char *values[2] = {payload.c_str(), id_str.c_str()};
  PgResultPtr res = MakeResult(
      PQexecParams(conn, sql.c_str(), 2, nullptr, values, nullptr, nullptr, 0));

  if (!IsCommandOk(res.get())) {
    if (error != nullptr) {
      *error = BuildError(conn, res.get(), "update by id failed");
    }
    return false;
  }
  return true;
}

bool ExecDeleteById(PGconn *conn, const std::string &table, int64_t id,
                    std::string *error) {
  const std::string sql = "DELETE FROM " + table + " WHERE id=$1";
  const std::string id_str = std::to_string(id);
  const char *values[1] = {id_str.c_str()};
  PgResultPtr res = MakeResult(
      PQexecParams(conn, sql.c_str(), 1, nullptr, values, nullptr, nullptr, 0));

  if (!IsCommandOk(res.get())) {
    if (error != nullptr) {
      *error = BuildError(conn, res.get(), "delete by id failed");
    }
    return false;
  }
  return true;
}

bool ExecSelectRange(PGconn *conn, const std::string &table, int64_t start_id,
                     int64_t end_id, std::string *error) {
  const std::string sql = "SELECT id FROM " + table +
                          " WHERE id BETWEEN $1 AND $2 ORDER BY id";
  const std::string start_str = std::to_string(start_id);
  const std::string end_str = std::to_string(end_id);
  const char *values[2] = {start_str.c_str(), end_str.c_str()};

  PgResultPtr res = MakeResult(
      PQexecParams(conn, sql.c_str(), 2, nullptr, values, nullptr, nullptr, 0));

  if (!IsTuplesOk(res.get())) {
    if (error != nullptr) {
      *error = BuildError(conn, res.get(), "select range failed");
    }
    return false;
  }
  return true;
}

} // namespace pgstress
