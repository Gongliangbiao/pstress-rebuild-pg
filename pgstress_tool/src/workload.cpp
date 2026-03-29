#include "workload.hpp"

#include "db.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

namespace pgstress {

WorkloadExecutor::WorkloadExecutor(const Config &config, Stats *stats,
                                   std::atomic<int64_t> *max_id,
                                   uint64_t seed)
    : config_(config), stats_(stats), max_id_(max_id), rng_(seed) {}

QueryType WorkloadExecutor::PickQueryType() {
  const int total = config_.insert_weight + config_.select_weight +
                    config_.update_weight + config_.delete_weight +
                    config_.range_select_weight;

  std::uniform_int_distribution<int> pick(1, total);
  int x = pick(rng_);

  if (x <= config_.insert_weight) {
    return QueryType::Insert;
  }
  x -= config_.insert_weight;

  if (x <= config_.select_weight) {
    return QueryType::SelectById;
  }
  x -= config_.select_weight;

  if (x <= config_.update_weight) {
    return QueryType::UpdateById;
  }
  x -= config_.update_weight;

  if (x <= config_.delete_weight) {
    return QueryType::DeleteById;
  }
  return QueryType::SelectRange;
}

std::string WorkloadExecutor::RandomPayload(int len) {
  static const char alphabet[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::uniform_int_distribution<int> pick(0,
                                          static_cast<int>(sizeof(alphabet) - 2));
  std::string out;
  out.reserve(static_cast<size_t>(len));
  for (int i = 0; i < len; ++i) {
    out.push_back(alphabet[pick(rng_)]);
  }
  return out;
}

int64_t WorkloadExecutor::RandomIdInRange(int64_t low, int64_t high) {
  std::uniform_int_distribution<int64_t> pick(low, high);
  return pick(rng_);
}

bool WorkloadExecutor::RunSomeQuery(PGconn *conn, std::string *error,
                                    QueryType *query_type) {
  const QueryType type = PickQueryType();
  if (query_type != nullptr) {
    *query_type = type;
  }

  stats_->total.fetch_add(1, std::memory_order_relaxed);

  const int64_t upper = max_id_->load(std::memory_order_relaxed);

  bool ok = false;
  switch (type) {
  case QueryType::Insert: {
    stats_->inserts.fetch_add(1, std::memory_order_relaxed);
    const std::string payload = RandomPayload(32);
    int64_t new_id = 0;
    ok = ExecInsertReturningId(conn, config_.table, payload, &new_id, error);
    if (ok) {
      int64_t prev = max_id_->load(std::memory_order_relaxed);
      while (new_id > prev &&
             !max_id_->compare_exchange_weak(prev, new_id,
                                             std::memory_order_relaxed)) {
      }
    }
    break;
  }
  case QueryType::SelectById: {
    stats_->selects.fetch_add(1, std::memory_order_relaxed);
    if (upper < 1) {
      ok = true;
    } else {
      const int64_t id = RandomIdInRange(1, upper);
      ok = ExecSelectById(conn, config_.table, id, error);
    }
    break;
  }
  case QueryType::UpdateById: {
    stats_->updates.fetch_add(1, std::memory_order_relaxed);
    if (upper < 1) {
      ok = true;
    } else {
      const int64_t id = RandomIdInRange(1, upper);
      const std::string payload = RandomPayload(40);
      ok = ExecUpdateById(conn, config_.table, id, payload, error);
    }
    break;
  }
  case QueryType::DeleteById: {
    stats_->deletes.fetch_add(1, std::memory_order_relaxed);
    if (upper < 1) {
      ok = true;
    } else {
      const int64_t id = RandomIdInRange(1, upper);
      ok = ExecDeleteById(conn, config_.table, id, error);
    }
    break;
  }
  case QueryType::SelectRange: {
    stats_->range_selects.fetch_add(1, std::memory_order_relaxed);
    if (upper < 1) {
      ok = true;
    } else {
      const int64_t start = RandomIdInRange(1, upper);
      const int64_t end =
          std::min<int64_t>(upper, start + config_.range_select_size - 1);
      ok = ExecSelectRange(conn, config_.table, start, end, error);
    }
    break;
  }
  }

  if (ok) {
    stats_->ok.fetch_add(1, std::memory_order_relaxed);
  } else {
    stats_->fail.fetch_add(1, std::memory_order_relaxed);
  }
  return ok;
}

} // namespace pgstress
