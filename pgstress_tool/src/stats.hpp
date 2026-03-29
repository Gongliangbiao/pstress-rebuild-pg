#ifndef PGSTRESS_STATS_HPP
#define PGSTRESS_STATS_HPP

#include <atomic>
#include <cstdint>

namespace pgstress {

struct Stats {
  std::atomic<uint64_t> total{0};
  std::atomic<uint64_t> ok{0};
  std::atomic<uint64_t> fail{0};

  std::atomic<uint64_t> inserts{0};
  std::atomic<uint64_t> selects{0};
  std::atomic<uint64_t> updates{0};
  std::atomic<uint64_t> deletes{0};
  std::atomic<uint64_t> range_selects{0};
};

} // namespace pgstress

#endif
