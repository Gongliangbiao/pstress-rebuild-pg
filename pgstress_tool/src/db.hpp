#ifndef PGSTRESS_DB_HPP
#define PGSTRESS_DB_HPP

#include "config.hpp"

#include <cstdint>
#include <string>

#include <libpq-fe.h>

namespace pgstress {

std::string BuildConnInfo(const Config &config);
PGconn *Connect(const Config &config, std::string *error);

bool EnsureSchemaAndInitData(const Config &config, std::string *error);
bool FetchMaxId(PGconn *conn, const std::string &table, int64_t *max_id,
                std::string *error);

bool ExecInsertReturningId(PGconn *conn, const std::string &table,
                           const std::string &payload, int64_t *new_id,
                           std::string *error);
bool ExecSelectById(PGconn *conn, const std::string &table, int64_t id,
                    std::string *error);
bool ExecUpdateById(PGconn *conn, const std::string &table, int64_t id,
                    const std::string &payload, std::string *error);
bool ExecDeleteById(PGconn *conn, const std::string &table, int64_t id,
                    std::string *error);
bool ExecSelectRange(PGconn *conn, const std::string &table, int64_t start_id,
                     int64_t end_id, std::string *error);

} // namespace pgstress

#endif
