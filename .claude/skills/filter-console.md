# /filter-console

Filter the log panel display by level and search text.

## Level Toggles

Three checkbox buttons with live count badges:
- **Info**: shows trace, debug, and info level messages (level ≤ 2)
- **Warn**: shows warning level messages (level = 3)
- **Error**: shows error and critical messages (level ≥ 4)

Each badge displays the count of messages at that level (unfiltered raw count).

## Search Box

Case-insensitive substring filter on message text. Empty = no filter. All messages matching the substring are shown regardless of which level toggles are on.

## Combined Filter Logic

A message is shown if:
1. Its level passes the toggle filter (Info/Warn/Error) AND
2. Its text contains the search substring (case-insensitive, if any)

## UI Layout

```
Info (42) Warn (3) Error (1) | [Search Box] [Collapse dups] [Export...]
```

## Notes

- Filters do not erase messages; they just hide them
- Search is live and responsive (no Enter key needed)
- Combined filters allow drilling into e.g. only warnings containing "load"
