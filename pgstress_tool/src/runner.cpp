#include "runner.hpp"

#include "db.hpp"
#include "workload.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace pgstress {
namespace {

void WorkerMain(int worker_id, const Config &config,
                std::chrono::steady_clock::time_point deadline, Stats *stats,
                std::atomic<int64_t> *max_id) {
  std::string error;
  PGconn *conn = Connect(config, &error);
  if (conn == nullptr) {
    stats->fail.fetch_add(1, std::memory_order_relaxed);
    std::cerr << "[worker " << worker_id << "] connect failed: " << error;
    return;
  }

  WorkloadExecutor executor(
      config, stats, max_id,
      config.seed + static_cast<uint64_t>(worker_id) * 1000003ULL);

  while (std::chrono::steady_clock::now() < deadline) {
    QueryType type;
    error.clear();

    const bool ok = executor.RunSomeQuery(conn, &error, &type);
    if (!ok && !error.empty() && PQstatus(conn) != CONNECTION_OK) {
      PQfinish(conn);
      conn = Connect(config, &error);
      if (conn == nullptr) {
        std::cerr << "[worker " << worker_id << "] reconnect failed: "
                  << error;
        break;
      }
    }
  }

  PQfinish(conn);
}

} // namespace

bool RunBenchmark(const Config &config, Stats *stats, int64_t *final_max_id) {
  std::string error;

  if (!EnsureSchemaAndInitData(config, &error)) {
    std::cerr << "prepare schema failed: " << error;
    return false;
  }

  PGconn *probe = Connect(config, &error);
  if (probe == nullptr) {
    std::cerr << "probe connect failed: " << error;
    return false;
  }

  int64_t initial_max = 0;
  if (!FetchMaxId(probe, config.table, &initial_max, &error)) {
    PQfinish(probe);
    std::cerr << "query max id failed: " << error;
    return false;
  }
  PQfinish(probe);

  std::atomic<int64_t> max_id(initial_max);

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(config.seconds);

  std::vector<std::thread> workers;
  workers.reserve(static_cast<size_t>(config.threads));

  for (int i = 0; i < config.threads; ++i) {
    workers.emplace_back(WorkerMain, i, std::cref(config), deadline, stats,
                         &max_id);
  }

  for (auto &worker : workers) {
    worker.join();
  }

  *final_max_id = max_id.load(std::memory_order_relaxed);
  return true;
}

} // namespace pgstress
