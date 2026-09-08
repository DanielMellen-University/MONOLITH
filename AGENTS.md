# AGENTS.md

Grok loads this file automatically. The full playbook is [`.agents.md`](.agents.md) - read it at session start and before every chunk. That file is the source of truth (vision, chunks, git, debts).

## Non-negotiable (also in the playbook)

- Personal mini-OS you enter. Not a Linux DE clone. Not a language project until the human unparks it.
- Work on **`beta`**. One chunk per commit. Merge to **`main`**. Push both. Keep the tips even.
- Rebuild (`cd build && make`) must be clean. Run the matching `scripts/` test.
- Update `.agents.md` (mark chunk done, set `CURRENT_CHUNK`), `CHANGELOG.md`, and the relevant `docs/` file with the code.
- Multi-instance apps: `claimNextAppInstanceTitle`. Compact on close. `restoreTrackedInstanceTitle()` after Drawing/Editor New.
- WM implementation is `src/window/detail/wm_body_*.inc` - grep `src/window/`.
- No em dashes in new docs or comments.
- Next chunk is whatever `.agents.md` lists as `CURRENT_CHUNK` (currently **4.1 Terminal quoted arguments**).
