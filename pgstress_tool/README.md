# pgstress (PostgreSQL 压测工具)

`pgstress` 是一个纯 PostgreSQL 并发压测工具，参考 `pstress` 的思路实现，但采用更清晰的模块化结构，便于后续在 GitHub 上持续维护和扩展。

当前版本已支持 5 类随机 case：
- `INSERT`：插入随机 payload，并返回新 `id`
- `SELECT by id`：按随机 id 点查
- `UPDATE by id`：按随机 id 更新 payload
- `DELETE by id`：按随机 id 删除
- `SELECT range`：按随机起点做区间查询

## 模块结构

`src` 目录说明：
- `main.cpp`：程序入口与结果汇总输出
- `config.hpp/.cpp`：参数定义、CLI 解析、校验
- `db.hpp/.cpp`：`libpq` 连接与 SQL 执行封装
- `workload.hpp/.cpp`：`run_some_query` 与随机 case 调度
- `runner.hpp/.cpp`：多线程 worker 生命周期与 benchmark 编排
- `stats.hpp`：线程安全统计结构

## 依赖

- C++17 编译器（clang/g++）
- CMake >= 3.18
- PostgreSQL `libpq` 开发库（头文件 + 动态库）

Linux 安装命令：

Debian / Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libpq-dev
```

RHEL / Rocky / AlmaLinux / CentOS Stream:

```bash
sudo dnf install -y gcc-c++ make cmake pkgconf-pkg-config postgresql-devel
```

本机已验证依赖路径：
- CMake: `~/Library/Python/3.9/bin/cmake`
- libpq: `/Users/gongliangbiao/Desktop/Codes/.local/pg`

## 编译

```bash
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
cd /Users/gongliangbiao/Desktop/Codes/pstress/pgstress_tool
cmake -S . -B build -DLIBPQ_ROOT=/Users/gongliangbiao/Desktop/Codes/.local/pg
cmake --build build -j
```

产物：`build/pgstress`

## 运行

```bash
./build/pgstress \
  --host=127.0.0.1 \
  --port=5432 \
  --user=postgres \
  --password=postgres \
  --dbname=postgres \
  --table=pgstress_data \
  --threads=8 \
  --seconds=60 \
  --init-rows=1000 \
  --insert-weight=50 \
  --select-weight=25 \
  --update-weight=15 \
  --delete-weight=5 \
  --range-select-weight=5 \
  --range-select-size=20 \
  --seed=42
```

帮助：

```bash
./build/pgstress --help
```

## 参数说明

连接参数：
- `--host` 默认 `127.0.0.1`
- `--port` 默认 `5432`
- `--user` 默认 `postgres`
- `--password` 默认空
- `--dbname` 默认 `postgres`
- `--table` 默认 `pgstress_data`

运行参数：
- `--threads` 默认 `4`
- `--seconds` 默认 `30`
- `--init-rows` 默认 `100`
- `--seed` 默认 `1`

workload 权重参数：
- `--insert-weight` 默认 `50`
- `--select-weight` 默认 `25`
- `--update-weight` 默认 `15`
- `--delete-weight` 默认 `5`
- `--range-select-weight` 默认 `5`
- `--range-select-size` 默认 `20`

约束：
- 所有权重必须 `>= 0`
- 权重和必须 `> 0`
- 表名必须匹配 `[A-Za-z_][A-Za-z0-9_]*`

## 输出示例

```text
[pgstress] host=127.0.0.1 port=5432 db=postgres table=pgstress_data threads=8 seconds=60 init_rows=1000
[pgstress] weights: insert=50 select=25 update=15 delete=5 range_select=5
[pgstress] done
  total=238412 ok=238410 fail=2 qps=3973.53
  inserts=119104 selects=59578 updates=35812 deletes=11910 range_selects=12008 max_id=130277
```

## 后续可扩展方向

- 事务 case（多语句、提交/回滚比例）
- 预处理语句缓存（prepared statement）
- 延迟统计（P50/P95/P99）
- 错误分类和按类统计
- 多表与 DDL 组合 case
