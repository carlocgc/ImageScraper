# ImageScraper — Improvement Plan

## Phase 1 — Unit Test Foundation
> Goal: Catch 3rd party API breakage and provide a regression net before any refactoring.

- [x] Select and integrate a test framework — Catch2 v3.14.0 vendored (no external tools required)
- [x] Run tests automatically on every main project build (post-build event, Option 1)

### RequestTypes
- [x] `ResponseErrorCodefromInt` — known HTTP codes map correctly
- [x] `ResponseErrorCodefromInt` — unknown codes fall back to InternalServerError
- [x] `ResponseErrorCodeToString` — all codes return correct strings
- [x] `RequestResult::SetError` — sets code and string together
- [x] `RequestResult` — correct default state

### DownloadUtils — URL helpers
- [x] `CreateQueryParamString` — empty, single, multiple params
- [x] `ExtractFileNameAndExtFromUrl` — valid URL, no slash, no extension
- [x] `ExtractExtFromFile` — valid file, no extension
- [x] `UrlToSafeString` — scheme stripping, slash replacement, query/fragment removal
- [x] `RedirectToPreferredFileTypeUrl` — gifv → mp4, non-gifv → empty

### DownloadUtils — Response error detection
- [x] `IsRedditResponseError` — valid response, error field, message field, invalid JSON
- [x] `IsTumblrResponseError` — valid response, error field, invalid JSON
- [x] `IsFourChanResponseError` — valid response, error field, invalid JSON

### Service JSON parsing (private methods — requires refactor to test)
- [ ] Reddit: `TryParseAccessTokenAndExpiry` — valid token response, missing fields
- [ ] Reddit: `TryParseRefreshToken` — valid response, missing field
- [ ] Reddit: `GetMediaUrls` — filters by extension, handles pagination field
- [ ] Tumblr: `GetMediaUrlsFromResponse` — photo posts, video posts, mixed
- [ ] FourChan: `GetPageCountForBoard` — matching board, missing board
- [ ] FourChan: `GetFileNamesFromResponse` — valid threads, empty threads

### Threading
- [ ] `ThreadPool` — task submission and execution on correct context
- [ ] `ThreadPool` — concurrent tasks complete without data races

### Future workflow improvements
- [ ] Run tests as post-build event on the test project itself (Option 2)
- [ ] CI pipeline via GitHub Actions — build and run tests on every push/PR (Option 4)

---

## Phase 2 — Request Layer
> Goal: Centralise HTTP logic, eliminate per-class curlpp boilerplate, add robustness.
> **Depends on: Phase 1**

- [ ] Design HTTP client abstraction over curlpp (mockable interface for unit tests)
- [ ] Centralised retry logic with configurable attempts and backoff
- [ ] Rate limit handling (429 responses — back off and retry)
- [ ] Configurable timeout per request type
- [ ] Migrate existing request classes (Reddit, Tumblr, FourChan) to use new abstraction
- [ ] Update unit tests to use mocked HTTP client

---

## Phase 3 — Threading Model
> Goal: Parallelise downloads, improve cancellation, unblock UI callback throughput.
> **Depends on: Phase 1**

- [ ] Increase network thread count and benchmark throughput
- [ ] Per-task cancellation support (cancel individual downloads cleanly)
- [ ] Process multiple UI callbacks per frame (remove single-task-per-frame limit)
- [ ] Evaluate dedicated download thread pool separate from API request threads

---

## Phase 4 — New Platforms
> Goal: Expand supported platforms leveraging the improved request and threading layers.
> **Depends on: Phase 2, Phase 3**

- [ ] Discord — implement stub (`DiscordService` already exists, throws `logic_error`)
- [ ] X (Twitter) — new service + request classes
- [ ] Bluesky — new service + request classes
- [ ] Mastodon — new service + request classes (federated, needs instance URL input)
- [ ] Update `config.template.json` and `UserInputOptions` for each new platform
