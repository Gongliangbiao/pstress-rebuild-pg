#ifndef PGSTRESS_WORKLOAD_HPP
#define PGSTRESS_WORKLOAD_HPP

#include "config.hpp"
#include "stats.hpp"

#include <atomic>
#include <cstdint>
#include <random>
#include <string>

#include <libpq-fe.h>

namespace pgstress {

enum class QueryType {
  Insert,
  SelectById,
  UpdateById,
  DeleteById,
  SelectRange,
};

class WorkloadExecutor {
public:
  WorkloadExecutor(const Config &config, Stats *stats,
                   std::atomic<int64_t> *max_id, uint64_t seed);

  bool RunSomeQuery(PGconn *conn, std::string *error, QueryType *query_type);

private:
  QueryType PickQueryType();
  std::string RandomPayload(int len);
  int64_t RandomIdInRange(int64_t low, int64_t high);

  const Config &config_;
  Stats *stats_;
  std::atomic<int64_t> *max_id_;
  std::mt19937_64 rng_;
};

} // namespace pgstress

#endif
