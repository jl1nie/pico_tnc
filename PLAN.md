# Current plan (help output work)

## 2026-04-04
- [x] Split help implementation into `help.c`/`help.h`.
- [x] Convert help output to non-blocking line-by-line send via `help_poll()`.
- [ ] Validate firmware behavior on hardware (USB terminal responsiveness while help is printing).
