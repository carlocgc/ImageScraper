# Refactor / Cleanup Sweep

## Goal

Keep a short list of the remaining structural cleanup work that still looks worth doing after the large UI, threading, download-pipeline, and decoder passes.

Completed cleanup work has been removed from this file on purpose. If something is done, delete it instead of leaving a checked-off archaeology layer behind.

## Priorities

### 1. Finish the `RingBuffer` API migration or declare it stable as-is

Impact: Medium to High
Effort: Medium

Why this still matters:

- `RingBuffer` now has `size_t`-based internals and iterator support, but parts of the public surface still preserve the older signed API shape.
- That leaves callers split between old and new conventions and makes it unclear whether more migration churn is still expected.

Current friction points:

- `operator[]` still takes `int`
- `RemoveAt()` still takes `int`
- `GetSize()` and `GetCapacity()` still exist alongside `Size()` and `Capacity()`

Recommended cleanup:

- decide whether the compatibility layer stays for good or whether the container should become fully `size_t`-first
- if the migration continues, update the remaining callers in one pass instead of leaving a half-migrated API
- if the compatibility layer stays, document that choice and stop planning further partial churn

Good first slice:

- audit every remaining caller of `GetSize()`, `GetCapacity()`, `operator[]`, and `RemoveAt()`
- either migrate them together or freeze the API and move on

### 2. Break `DownloadHistoryPanel` into smaller components

Impact: High
Effort: Medium

Why this still matters:

- `DownloadHistoryPanel.cpp` remains one of the largest first-party files
- it still mixes rendering, filesystem traversal, selection state, deletion flow, thumbnail caching, and preview callback routing
- richer history work will keep getting harder until the panel has clearer seams

Recommended cleanup:

- extract a history model or controller layer from the ImGui rendering code
- isolate thumbnail caching/async decode from tree rendering
- keep preview propagation and deletion behavior routed through one selection API

Good first slice:

- pull selection/deletion helpers into a smaller focused component first
- move thumbnail cache and decode orchestration out of the panel after that

### 3. Centralize provider field metadata

Impact: Medium
Effort: Medium

Why this still matters:

- the remaining UI-help work will be repetitive if every panel keeps hand-building labels, validation rules, config keys, and external help URLs
- `CredentialsPanel` already has local rendering helpers, but the metadata is still spread across panel-specific code

Recommended cleanup:

- define descriptor tables for provider fields and credential fields
- let those descriptors drive:
  - labels
  - required/optional state
  - tooltip copy
  - config keys
  - registration/help URLs
- keep panel code focused on layout instead of field bookkeeping

Good first slice:

- start with `CredentialsPanel`
- then move one provider panel over before touching the rest

## Best Next Cleanup Bundle

If we want the highest-value cleanup set without reopening the whole app architecture:

1. Decide the final `RingBuffer` surface.
2. Split `DownloadHistoryPanel` selection/deletion logic from rendering.
3. Introduce field descriptors for `CredentialsPanel` and one provider panel.

That sequence keeps future UI work cheaper without recreating the broad refactor backlog that has already been burned down.
