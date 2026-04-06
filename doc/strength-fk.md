# `strength-fk` Implementation Notes

## Goal

Add an opt-in FK generation mode that relaxes the old restriction:

- default mode keeps the legacy behavior
- `--strength-fk` allows FK child tables to reference indexed parent columns
- the first supported parent column types are `INT`, `INTEGER`, `CHAR`, and `VARCHAR`

## Legacy Behavior

Without `--strength-fk`, pstress still behaves as before:

- FK tables are created only when the parent table has a primary key
- the FK points to the parent primary key
- the FK child column is created with the legacy integer-based layout
- child preload values still come from `Thd1::unique_keys`

## New Behavior

With `--strength-fk` enabled:

- FK target discovery is no longer limited to the parent primary key
- a parent column is eligible only when:
  - it has an index or is the primary key
  - it is one of `INT`, `INTEGER`, `CHAR`, `VARCHAR`
  - it is not `AUTO_INCREMENT`
- the FK child column is rebuilt to match the chosen parent column type
- FK preload values are read from the parent column's cached inserted values

## Data Loading Strategy

The preload failure came from assuming all FK values were integer primary-key values.
The new implementation avoids that assumption:

- every column now keeps an `inserted_values` cache
- during parent preload, the exact SQL literal inserted for each column is stored
- during FK child preload, the FK column picks values from the referenced parent column cache
- random row inserts for FK tables also reuse the same cached parent values

This keeps FK child inserts aligned with real parent values that were inserted earlier.

## Metadata Changes

FK metadata now stores the referenced parent column name in addition to:

- parent table name
- `ON UPDATE`
- `ON DELETE`

This is required so step reload can restore which parent column the FK points to.

## Scope Limits

The first version intentionally does not support these parent FK targets:

- `BLOB`
- `GENERATED`
- `FLOAT`
- `DOUBLE`
- `BOOL`

## Known Follow-up

The current cache tracks inserted parent values well enough for preload and later FK inserts,
but it does not try to stay perfectly synchronized with every later random update, delete, or
truncate. That can still produce legitimate FK failures during later workload execution if the
parent rows are changed after the cache was populated.
