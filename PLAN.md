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

### Service JSON parsing (extracted to testable utility headers)
- [x] Reddit: `ParseAccessToken` — valid response, missing access_token, missing expires_in
- [x] Reddit: `ParseRefreshToken` — valid response, missing field
- [x] Reddit: `GetMediaUrls` — filters by extension, url vs url_overridden_by_dest, pagination, early bail behaviour documented
- [x] Tumblr: `GetMediaUrlsFromResponse` — photo posts, video posts, mixed, missing fields
- [x] FourChan: `GetPageCountForBoard` — matching board, missing board, missing boards field
- [x] FourChan: `GetFileNamesFromResponse` — valid threads, empty threads, missing fields, multi-thread

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

- [x] Design HTTP client abstraction over curlpp (`IHttpClient`, `HttpRequest`, `HttpResponse`)
- [x] `CurlHttpClient` — curlpp implementation with timeout support
- [x] `RetryHttpClient` — decorator with exponential backoff and 429 rate limit handling
- [x] Migrate `GetBoardsRequest` to use new abstraction (proof of concept)
- [x] Migrate remaining request classes (GetThreadsRequest, AppOnlyAuthRequest, FetchAccessTokenRequest, RefreshAccessTokenRequest, FetchSubredditPostsRequest, RetrievePublishedPostsRequest)
- [x] Update unit tests to use mocked `IHttpClient`

---

## Phase 3 — UI Refactor & New Windows
> Goal: Break up the FrontEnd monolith, decouple services from the UI, and add new panels without modifying core files.
> **Depends on: Phase 2**

### Decouple services from FrontEnd
- [x] Define `IServiceSink` interface — `IsCancelled`, `OnRunComplete`, `OnCurrentDownloadProgress`, `OnTotalDownloadProgress`, `GetSigningInProvider`, `OnSignInComplete`
- [x] Replace `shared_ptr<FrontEnd>` in `Service` base class and `DownloadRequest` with `shared_ptr<IServiceSink>`
- [x] Implement `IServiceSink` on `FrontEnd`

### Panel abstraction
- [x] Define `IUiPanel` interface — `Update()` — so new windows can be added without touching `FrontEnd`; `InputState` moved here from `FrontEnd`
- [x] Extract `LogPanel` from `FrontEnd` — owns log buffer, filter, auto-scroll, progress bars
- [x] Extract `DownloadOptionsPanel` from `FrontEnd` — owns provider combo, run/cancel/sign-in buttons

### Per-provider panel classes
- [x] Define `IProviderPanel` interface — `Update()`, `CanSignIn()`, `IsReadyToRun()`, `BuildInputOptions()`, `GetContentProvider()`
- [x] Extract `RedditPanel` — owns subreddit name, scope, time frame, max items state
- [x] Extract `TumblrPanel` — owns tumblr user state
- [x] Extract `FourChanPanel` — owns board, max threads, max media state
- [x] `DownloadOptionsPanel` holds a list of `IProviderPanel` — new platforms add a panel, not a FrontEnd edit

### New windows
- [ ] `MediaPreviewPanel` — loads last downloaded image into an OpenGL texture (stb_image) and renders it in a dockable ImGui window; supports static images and animated GIFs (frame stepping)
- [ ] `DownloadHistoryPanel` — ring buffer of completed downloads showing filename, source URL, and timestamp; clicking an entry opens it in explorer

### Code quality
- [ ] Replace `#define INVALID_CONTENT_PROVIDER` and `#define` UI constants with `constexpr`
- [ ] `Reset()` should be called explicitly rather than implicitly from `SetInputState`

---

## Phase 4 — Threading Model
> Goal: Parallelise downloads, improve cancellation, unblock UI callback throughput.
> **Depends on: Phase 2**

- [ ] Increase network thread count and benchmark throughput
- [ ] Per-task cancellation support (cancel individual downloads cleanly)
- [ ] Process multiple UI callbacks per frame (remove single-task-per-frame limit)
- [ ] Evaluate dedicated download thread pool separate from API request threads

---

## Phase 5 — New Platforms
> Goal: Expand supported platforms leveraging the improved request and threading layers.
> **Depends on: Phase 3, Phase 4**

- [ ] Discord — implement stub (`DiscordService` already exists, throws `logic_error`)
- [ ] X (Twitter) — new service + request classes
- [ ] Bluesky — new service + request classes
- [ ] Mastodon — new service + request classes (federated, needs instance URL input)
- [ ] Update `config.template.json` and `UserInputOptions` for each new platform
