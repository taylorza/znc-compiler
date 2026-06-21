# datetime — Real-time clock and date/time parsing

Description

Helpers to read RTC values and parse DOS-style date/time into fields.

Types

- None

Constants

- `DT_YEAR` — 0
- `DT_MONTH` — 1
- `DT_DAY` — 2
- `DT_HOUR` — 3
- `DT_MINS` — 4
- `DT_SECS` — 5

Globals

- None

Functions

- `int getDateTime(int[] dt)` — Populate `dt` array with date/time fields and return seconds.

Examples

```c
int dt[6];
getDateTime(dt);
printf("%04d-%02d-%02d %02d:%02d:%02d\r", dt[0], dt[1], dt[2], dt[3], dt[4], dt[5]);
```
