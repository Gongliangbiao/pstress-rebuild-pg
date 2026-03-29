#ifndef PGSTRESS_CONFIG_HPP
#define PGSTRESS_CONFIG_HPP

#include <cstdint>
#include <string>

namespace pgstress {

struct Config {
  std::string host = "127.0.0.1";
  int port = 5432;
  std::string user = "postgres";
  std::string password;
  std::string dbname = "postgres";
  std::string table = "pgstress_data";

  int threads = 4;
  int seconds = 30;
  int init_rows = 100;
  uint64_t seed = 1;

  int insert_weight = 50;
  int select_weight = 25;
  int update_weight = 15;
  int delete_weight = 5;
  int range_select_weight = 5;
  int range_select_size = 20;
};

void PrintHelp();
void ParseArgs(int argc, char **argv, Config *config);

} // namespace pgstress

#endif
