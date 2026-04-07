#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PSTRESS_BIN="${PSTRESS_BIN:-$ROOT_DIR/build/src/pstress-ms}"
MYSQL_BIN="${MYSQL_BIN:-/opt/homebrew/opt/mysql@8.0/bin/mysql}"
SOCKET="${SOCKET:-/tmp/mysql.sock}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-12345678}"
TEST_DB="${TEST_DB:-pstress_fk_test}"
RUN_DIR="${RUN_DIR:-/tmp/pstress-strength-fk-run}"

if [[ ! -x "$PSTRESS_BIN" ]]; then
  echo "pstress binary not found: $PSTRESS_BIN" >&2
  exit 1
fi

if [[ ! -x "$MYSQL_BIN" ]]; then
  echo "mysql client not found: $MYSQL_BIN" >&2
  exit 1
fi

export MYSQL_PWD="$MYSQL_PASSWORD"

mkdir -p "$RUN_DIR"
rm -f "$RUN_DIR"/default.node.tld_ddl_step_*.log \
      "$RUN_DIR"/default.node.tld_step_*_thread-0.sql \
      "$RUN_DIR"/step_*.dll

"$MYSQL_BIN" --protocol=socket --socket="$SOCKET" -u"$MYSQL_USER" -e \
  "DROP DATABASE IF EXISTS \`$TEST_DB\`; CREATE DATABASE \`$TEST_DB\`;"

COMMON_ARGS=(
  --database="$TEST_DB"
  --threads=1
  --logdir="$RUN_DIR"
  --metadata-path="$RUN_DIR"
  --user="$MYSQL_USER"
  --password="$MYSQL_PASSWORD"
  --socket="$SOCKET"
  --engine=INNODB
  --tables=20
  --records=10
  --exact-initial-records
  --columns=8
  --indexes=4
  --index-columns=2
  --fk-prob=100
  --pk-prob=0
  --undo-tbs-sql=0
  --no-partition-tables
  --no-temp-tables
  --no-tbs
  --no-encryption
  --no-table-compression
  --strength-fk
  --log-failed-queries
)

echo "==> Step 1: prepare strength-fk schema"
"$PSTRESS_BIN" \
  "${COMMON_ARGS[@]}" \
  --queries-per-thread=20 \
  --prepare \
  --step=1

echo "==> Verify FK count after prepare"
fk_count="$("$MYSQL_BIN" --protocol=socket --socket="$SOCKET" -u"$MYSQL_USER" -Nse \
  "SELECT COUNT(*) FROM information_schema.referential_constraints WHERE constraint_schema='$TEST_DB'")"
echo "FK count: $fk_count"
if [[ "$fk_count" -le 0 ]]; then
  echo "No FK constraints were created" >&2
  exit 1
fi

echo "==> Step 2: insert-only short workload"
"$PSTRESS_BIN" \
  "${COMMON_ARGS[@]}" \
  --queries-per-thread=30 \
  --seconds=5 \
  --step=2 \
  --no-ddl \
  --no-select \
  --no-update \
  --no-delete \
  --insert-row=100

echo "==> Scan logs for failures"
if rg -n "Failed|error|exception|terminate|No cached parent FK values|Bulk insert failed" \
  "$RUN_DIR" -g '!*.dll'; then
  echo "Validation logs contain failure markers" >&2
  exit 1
fi

echo "==> Validation passed"
echo "Logs: $RUN_DIR"
echo "Binary: $PSTRESS_BIN"
