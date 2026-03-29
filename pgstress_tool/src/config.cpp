#include "config.hpp"

#include <cstdlib>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

namespace pgstress {
namespace {

bool IsValidIdentifier(const std::string &name) {
  static const std::regex pattern("^[A-Za-z_][A-Za-z0-9_]*$");
  return std::regex_match(name, pattern);
}

[[noreturn]] void Die(const std::string &msg) {
  std::cerr << "ERROR: " << msg << std::endl;
  std::exit(1);
}

void Require(bool ok, const std::string &msg) {
  if (!ok) {
    Die(msg);
  }
}

bool ConsumeArg(const std::string &arg, const char *name, std::string *out) {
  const std::string prefix = std::string("--") + name + "=";
  if (arg.rfind(prefix, 0) == 0) {
    *out = arg.substr(prefix.size());
    return true;
  }
  return false;
}

} // namespace

void PrintHelp() {
  std::cout
      << "pgstress 参数:\n"
      << "  --host=127.0.0.1\n"
      << "  --port=5432\n"
      << "  --user=postgres\n"
      << "  --password=***\n"
      << "  --dbname=postgres\n"
      << "  --table=pgstress_data\n"
      << "  --threads=4\n"
      << "  --seconds=30\n"
      << "  --init-rows=100\n"
      << "  --seed=1\n"
      << "  --insert-weight=50\n"
      << "  --select-weight=25\n"
      << "  --update-weight=15\n"
      << "  --delete-weight=5\n"
      << "  --range-select-weight=5\n"
      << "  --range-select-size=20\n";
}

void ParseArgs(int argc, char **argv, Config *config) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    std::string value;

    if (ConsumeArg(arg, "host", &config->host) ||
        ConsumeArg(arg, "user", &config->user) ||
        ConsumeArg(arg, "password", &config->password) ||
        ConsumeArg(arg, "dbname", &config->dbname) ||
        ConsumeArg(arg, "table", &config->table)) {
      continue;
    }

    if (ConsumeArg(arg, "port", &value)) {
      config->port = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "threads", &value)) {
      config->threads = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "seconds", &value)) {
      config->seconds = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "init-rows", &value)) {
      config->init_rows = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "seed", &value)) {
      config->seed = static_cast<uint64_t>(std::stoull(value));
      continue;
    }

    if (ConsumeArg(arg, "insert-weight", &value)) {
      config->insert_weight = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "select-weight", &value)) {
      config->select_weight = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "update-weight", &value)) {
      config->update_weight = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "delete-weight", &value)) {
      config->delete_weight = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "range-select-weight", &value)) {
      config->range_select_weight = std::stoi(value);
      continue;
    }
    if (ConsumeArg(arg, "range-select-size", &value)) {
      config->range_select_size = std::stoi(value);
      continue;
    }

    if (arg == "--help") {
      PrintHelp();
      std::exit(0);
    }

    Die("unknown arg: " + arg);
  }

  Require(config->port > 0, "port must be > 0");
  Require(config->threads > 0, "threads must be > 0");
  Require(config->seconds > 0, "seconds must be > 0");
  Require(config->init_rows >= 0, "init-rows must be >= 0");
  Require(config->range_select_size > 0, "range-select-size must be > 0");

  Require(config->insert_weight >= 0, "insert-weight must be >= 0");
  Require(config->select_weight >= 0, "select-weight must be >= 0");
  Require(config->update_weight >= 0, "update-weight must be >= 0");
  Require(config->delete_weight >= 0, "delete-weight must be >= 0");
  Require(config->range_select_weight >= 0,
          "range-select-weight must be >= 0");

  const int total = config->insert_weight + config->select_weight +
                    config->update_weight + config->delete_weight +
                    config->range_select_weight;
  Require(total > 0, "sum of all weights must be > 0");

  Require(IsValidIdentifier(config->table),
          "table name invalid, only [A-Za-z_][A-Za-z0-9_]* is allowed");
}

} // namespace pgstress
