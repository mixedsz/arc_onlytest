# Debug logging

## Where logs live

`logs\debug_YYYY-MM-DD.log` (under the project root when Logger creates it)

Created on Debug (and any build that calls `Logger_Init`) when **Auto-Save Logs** is on. Off = no file I/O. Console output is unchanged.

Rotation keeps the 5 newest `debug_*.log` files.

## Format

```
[2026-08-20 20:15:03] [INFO] session start pid=48210
[2026-08-20 20:15:03] [INFO] [Main] Console attached
[2026-08-20 20:15:04] [ERROR] transport ReadFailed
```

Levels: DEBUG, INFO, WARNING, ERROR, CRITICAL.

`DBG_LOG` -> INFO. `DBG_ERR` -> ERROR.

## Menu toggles (Misc / Debug)

- Auto-Save Logs — file sink
- Log Errors Only — file writes WARNING+
- Log Transport Errors — off by default; status names only, 250ms rate limit, no addresses

## Error flag

A line sets the sticky error flag if it is ERROR/CRITICAL level, or the text contains `ERROR`, `CRITICAL`, `nullptr`, or a **token** `0x0` (next character is not a hex digit). `obj=0x00007FF...` does not match.

## Common signatures

| Log text | Meaning |
|---|---|
| `[Boot] CR3 resolve failed` | driver/attach failed |
| `[InitializeViewPort] failed after 30 attempts` | viewport not up yet or offsets wrong |
| `chunks=0x0` | GObjects decrypt dead |
| `transport NotReady` / `ReadFailed` | ctx read failed (only if Log Transport Errors is on) |

Paste `logs\debug_*.log` into Cursor instead of copying the console.

Do not commit `logs/` (gitignored).