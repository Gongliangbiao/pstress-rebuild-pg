#ifndef PGSTRESS_RUNNER_HPP
#define PGSTRESS_RUNNER_HPP

#include "config.hpp"
#include "stats.hpp"

#include <cstdint>

namespace pgstress {

bool RunBenchmark(const Config &config, Stats *stats, int64_t *final_max_id);

} // namespace pgstress

#endif
