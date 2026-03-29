#include "config.hpp"
#include "runner.hpp"
#include "stats.hpp"

#include <cstdint>
#include <iostream>

namespace {

void PrintStartup(const pgstress::Config &config) {
  std::cout << "[pgstress] host=" << config.host << " port=" << config.port
            << " db=" << config.dbname << " table=" << config.table
            << " threads=" << config.threads << " seconds=" << config.seconds
            << " init_rows=" << config.init_rows << '\n'
            << "[pgstress] weights: "
            << "insert=" << config.insert_weight << " "
            << "select=" << config.select_weight << " "
            << "update=" << config.update_weight << " "
            << "delete=" << config.delete_weight << " "
            << "range_select=" << config.range_select_weight << std::endl;
}

void PrintSummary(const pgstress::Stats &stats, const pgstress::Config &config,
                  int64_t max_id) {
  const uint64_t total = stats.total.load(std::memory_order_relaxed);
  const uint64_t ok = stats.ok.load(std::memory_order_relaxed);
  const uint64_t fail = stats.fail.load(std::memory_order_relaxed);

  const uint64_t inserts = stats.inserts.load(std::memory_order_relaxed);
  const uint64_t selects = stats.selects.load(std::memory_order_relaxed);
  const uint64_t updates = stats.updates.load(std::memory_order_relaxed);
  const uint64_t deletes = stats.deletes.load(std::memory_order_relaxed);
  const uint64_t range_selects =
      stats.range_selects.load(std::memory_order_relaxed);

  const double qps = static_cast<double>(total) / config.seconds;

  std::cout << "[pgstress] done\n"
            << "  total=" << total << " ok=" << ok << " fail=" << fail
            << " qps=" << qps << "\n"
            << "  inserts=" << inserts << " selects=" << selects
            << " updates=" << updates << " deletes=" << deletes
            << " range_selects=" << range_selects << " max_id=" << max_id
            << std::endl;
}

} // namespace

int main(int argc, char **argv) {
  pgstress::Config config;
  pgstress::ParseArgs(argc, argv, &config);

  PrintStartup(config);

  pgstress::Stats stats;
  int64_t final_max_id = 0;

  if (!pgstress::RunBenchmark(config, &stats, &final_max_id)) {
    return 1;
  }

  PrintSummary(stats, config, final_max_id);
  return stats.fail.load(std::memory_order_relaxed) == 0 ? 0 : 2;
}
