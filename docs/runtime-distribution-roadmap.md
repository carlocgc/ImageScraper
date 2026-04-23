# Runtime / Distribution Roadmap

## Goal

Capture the remaining infrastructure work that spans cancellation, throughput, dependency management, packaging, and longer-term deployment experiments.

## Runtime throughput and cancellation

### 1. Per-task cancellation

The app can cancel a whole run quickly, but it still cannot cancel one download independently while letting the rest of the run continue cleanly.

Recommended slice:

- define cancellation tokens at the download-task level
- make partial-run cleanup behavior explicit
- keep progress/completion reporting understandable when some files are cancelled and others finish

### 2. Thread-pool tuning

The old "one main-thread callback per frame" bottleneck is already gone, so the next runtime pass should be measurement-driven instead of assumption-driven.

Recommended slice:

- benchmark current throughput before changing thread counts
- decide whether the shared pool is actually a bottleneck
- only split API requests and downloads into separate pools if measurements show contention or starvation

Related questions:

- should network thread count increase beyond the current default
- does media decoding compete materially with downloads in real runs
- would a dedicated download pool improve cancellation semantics enough to justify the extra complexity

## Dependencies and packaging

### 3. vcpkg migration

Moving binary dependencies out of `ImageScraper/lib/` is still a viable cleanup, but only if reproducibility and CI simplicity are worth the churn.

Recommended slice:

- prototype a manifest with the current dependency set
- verify MSVC triplets and dynamic-versus-static linking choices
- confirm that FFmpeg packaging remains compatible with the release process before committing to the migration

### 4. FFmpeg distribution strategy

The project still needs a durable decision on where FFmpeg DLLs should live and how they should be shipped.

Options to evaluate:

- keep them in-repo as today
- source them through package-manager installs
- ship them only through release packaging

Constraint:

- preserve LGPL swapability whichever route is chosen

## Exploratory work

### 5. Containerised / self-hosted mode

Treat this as an architecture spike, not as an extension of the desktop backlog.

Questions to answer before any implementation work:

- what service-layer API would replace or sit beside the current desktop UI
- how auth flows would work in a remote/browser context
- whether the current single-user local storage model survives a self-hosted deployment
- whether multi-user support is in scope or explicitly out of scope
