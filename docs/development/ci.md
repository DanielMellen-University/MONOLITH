# Continuous integration

CI is one GitHub Actions workflow: `.github/workflows/headless.yml` ("Headless verification").

## When it runs

- Pushes to `main` or `beta`
- Pull requests targeting `main` or `beta`
- Manual runs (`workflow_dispatch`)

A new push to the same branch cancels the run already in progress for that branch (`concurrency` with `cancel-in-progress: true`).

## Jobs

Both jobs run on `ubuntu-24.04` (GCC 13) with a 30-minute limit and read-only repository permissions.

| Job | Steps |
|-----|-------|
| `verify` | Install packages, configure Release, build `monolith`, run `ctest --test-dir build --output-on-failure` |
| `sanitize` | Install packages, configure Debug with AddressSanitizer and UndefinedBehaviorSanitizer, build, run CTest with the sanitizer `CXXFLAGS`, `ASAN_OPTIONS` and `UBSAN_OPTIONS` |

The commands match [building.md](building.md) and [testing.md](testing.md#sanitizers), so a local run reproduces CI.

Packages: `cmake g++ pkg-config python3 libsdl2-dev libsdl2-ttf-dev`, installed with `--no-install-recommends`.

## Package install limits

The install step has its own 10-minute limit, and `apt-get` runs with `Acquire::Retries=3` and 30-second HTTP and HTTPS timeouts. A stalled mirror then fails or retries quickly instead of using the whole 30-minute job budget.

## Network

The build downloads `stb_image.h` from GitHub (see [building.md](building.md#stb_image)). If that download fails or the upstream file changes, both jobs fail at the build step.

## Checking results

```bash
gh run list --workflow headless.yml --limit 5
gh run watch <run-id>
gh run view <run-id> --log-failed
```

A change is ready when both jobs are green on the branch tip.
