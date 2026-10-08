# Contributing

MONOLITH is a personal project, but changes follow a fixed routine so `main` always builds and passes tests.

## Branches

| Branch | Role |
|--------|------|
| `beta` | Changes land here first |
| `main` | Gets the same commits once CI is green on `beta` |

Releases are tagged on `main` (`v1.0.0`). Never force-push either branch.

## Workflow

1. Branch from `beta`, or work on it directly for small changes.
2. Build and run the suite locally:

   ```bash
   cmake --build build -j"$(nproc)"
   ctest --test-dir build --output-on-failure
   ```

3. Commit in small logical steps (see below).
4. Push to `beta` and wait for both CI jobs to pass ([ci.md](ci.md)).
5. Bring `main` up to the same commit.

## Commit messages

Use [Conventional Commits](https://www.conventionalcommits.org/): `type(scope): summary`, lower case, no trailing period.

| Type | Use |
|------|-----|
| `feat` | New behavior |
| `fix` | Bug fix |
| `perf` | Faster or lighter, same behavior |
| `refactor` | Restructuring, same behavior |
| `test` | Tests only |
| `docs` | Documentation only |
| `ci` | Workflow changes |
| `chore` | Version bumps, chunk markers, release bookkeeping |

Examples from the history: `fix(files): check rename text input for empty string, not array address`, `ci: bound apt-get install step with timeout and retries`.

## Code rules

- C++23 with `-Wall -Wextra -Wpedantic`; keep the build free of warnings.
- Apps talk to the shell only through `IWindowController`, never to each other.
- Every file write goes through the atomic writer (`Filesystem::writeFile...` or `detail/AtomicFile`).
- Anything that reads user data needs a size limit. Add new limits to [limits.md](../internals/limits.md).
- Logic that can run without SDL belongs in its own file so it can be tested headless (see `PongLogic`, `DrawingRaster`, `TerminalLexer`).
- Large compressed fragments are edited through the decompress and compress scripts ([building.md](building.md#generated-sources)).

## Adding an app

1. Create `src/app/<Name>App.{hpp,cpp}` implementing `App` (see [architecture.md](../architecture.md#apps)).
2. Add the `.cpp` to `add_executable` in `CMakeLists.txt`.
3. Add a launcher to `WindowManager` (next to the others in `src/window/detail/wm_body_07.inc` or `wm_body_08a.inc`) and a case in `launchByAction`.
4. Add an `AppAction` value and a row in `src/window/AppRegistry.hpp`. That one row puts the app in the Start menu, optionally on the desktop, and in session restore. Details: [desktop-shell.md](../internals/desktop-shell.md#adding-an-app).
5. Add a test in `scripts/` and register it in `run_headless_tests.sh` ([testing.md](testing.md#writing-tests)).
6. Write `docs/apps/<name>.md` in the same shape as the other app pages and link it from [docs/README.md](../README.md) and the root README.

## Documentation rules

- Each app page has the same sections: what it does, using it, keyboard shortcuts, limits, developer notes.
- Put low-level detail in `docs/internals/`, not in the app pages.
- Check every command you document by running it.
- Use tables for shortcuts, commands and limits.
- Write plainly. No em dashes, no marketing words, no emoji.
- `CHANGELOG.md` entries are history: add new entries at the top and do not rewrite old ones.
- `docs/1.0-CHECKLIST.md` is the owner's own text; only tick boxes in it.
