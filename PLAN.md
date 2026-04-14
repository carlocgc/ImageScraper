# ImageScraper — Improvement Plan

## Phase 1 — Unit Test Foundation
> Goal: Catch 3rd party API breakage and provide a regression net before any refactoring.

- [x] Select and integrate a test framework — Catch2 v3.14.0 vendored (no external tools required)
- [x] Run tests automatically on every main project build (post-build event on main project)
- [x] Run tests as post-build event on the test project itself for faster isolated test runs; fix solution dependency so test project builds after main project
- [x] Organise test include files into subfolders — move `MockHttpClient.h` to `include/mocks/`

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

### Threading — ThreadPool fixes
- [x] Clear `m_Threads` and context maps in `Stop` so the pool can be restarted cleanly
- [x] Guard `Submit` against out-of-range context — assert when `context >= m_Threads.size()` to prevent silent deadlock
- [x] Remove dead `m_StopMutex` field and dead `m_MainCondition` / `notify_one` call in `SubmitMain`
- [x] Make `m_IsRunning` `atomic<bool>` to match `m_Stopping`

### Threading — ThreadPool tests

**Start / Stop lifecycle**
- [x] `Start` — pool starts with the requested number of worker threads
- [x] `Start` when already running is a no-op; does not spawn additional threads
- [x] `Stop` joins all threads; pool can be restarted cleanly after `Stop`
- [x] Destructor calls `Stop` without hanging

**Submit (worker thread)**
- [x] `Submit` — future resolves to the correct return value (non-void callable)
- [x] `Submit` — void-returning task completes without error
- [x] `Submit` — tasks on the same context execute in submission order (FIFO)
- [x] `Submit` — tasks on different contexts execute independently and concurrently

**SubmitMain (main-thread queue)**
- [x] `SubmitMain` — task does not execute until `Update()` is called
- [x] `SubmitMain` — future resolves after `Update()` is called
- [x] `SubmitMain` — multiple queued tasks drain one per `Update()` call in FIFO order

**Update**
- [x] `Update` on an empty main queue is a no-op (no crash or block)
- [x] `Update` after `Stop` is a no-op

**Stop edge cases**
- [x] `Stop` when not running is a no-op
- [x] `Stop` while tasks are in flight waits for in-flight tasks to complete before returning
- [x] Pending queued tasks at `Stop` time are dropped — verify and document behaviour

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

### Code quality
- [x] Replace `#define` UI constants with `constexpr` — moved 14 integer constants in `ServiceOptionTypes.h` into the `ImageScraper` namespace as `constexpr int` (`INVALID_CONTENT_PROVIDER` was already `constexpr`)
- [x] `Reset()` should be called explicitly rather than implicitly from `SetInputState` — removed side effect from `SetInputState`; `OnRunComplete` now calls `Reset()` explicitly; `FrontEnd::OnRunComplete` delegates to `DownloadOptionsPanel::OnRunComplete` directly

### Authentication & Session Management
- [x] Reddit sign-out — local-only token clear (server-side revocation deferred)
  - `RevokeAccessTokenRequest` implemented (`POST /api/v1/revoke_token`) but **not called** — Reddit imposes a multi-minute propagation delay after revocation during which re-authentication hangs (OAuth page never redirects); sign-out clears tokens locally only; class retained for future use
  - `virtual void SignOut() {}` default no-op added to `Service` base class
  - `RedditService::SignOut()` clears access token, refresh token, and username in memory and on disk; see header comment in `RevokeAccessTokenRequest.h` for full rationale
  - `DownloadOptionsPanel::UpdateSignInButton()` — replaced disabled "Signed In" button with active "Sign Out" button
- [x] Reddit signed-in username badge — fetch and display the authenticated username in the Download Options panel
  - Add `GetCurrentUserRequest` — `GET https://oauth.reddit.com/api/v1/me` with Bearer token; returns JSON containing `"name"` field
  - Call after successful sign-in (`FetchAccessToken` on complete) and after a successful token refresh (`TryPerformAuthTokenRefresh`); store username in `RedditService::m_Username`
  - Add `virtual std::string GetSignedInUser() const { return {}; }` to `Service` base class
  - `DownloadOptionsPanel::UpdateSignInButton()` — when signed in, render a small styled badge showing `u/<username>` alongside the Sign Out button; clear `m_Username` on sign-out
- [ ] OAuth2 for Tumblr — implement OAuth2 sign-in flow (similar to Reddit); update `TumblrPanel`, request classes, and `config.template.json`
- [ ] Sign-out for Tumblr — same pattern as Reddit once OAuth2 is implemented
- [ ] Remove `CanSignIn()` from `FourChanPanel` / hide sign-in UI for 4chan — API is fully anonymous and public, no authentication exists
- [ ] OAuth2 redirect HTML polish — review and improve the in-app redirect/confirmation page shown after OAuth2 authorisation; better copy, styling, and error states

### New windows
- [x] `MediaPreviewPanel` — loads last downloaded image into an OpenGL texture (stb_image) and renders it in a dockable ImGui window; supports static images and animated GIFs (frame stepping)
- [x] `DownloadHistoryPanel` — ring buffer of completed downloads showing filename, source URL, and timestamp; clicking an entry opens it in explorer
- [ ] `DownloadProgressPanel` — extract current and total download progress bars out of `LogPanel` into a dedicated dockable panel; `LogPanel` retains log lines only
- [ ] `DownloadHistoryPanel` provider tabs — add a tab strip per provider so history is filterable by source; an "All" tab shows the combined view
- [ ] `DownloadHistoryPanel` extra param columns — show provider-specific download parameters as additional columns (e.g. Download Scope / sort for Reddit: Hot, Best, New, Top, etc.)
- [ ] `DownloadHistoryPanel` hover tooltip preview — show a small single-frame thumbnail of the image in an ImGui tooltip when hovering a history entry; skip non-image and large files gracefully
- [ ] `MediaPreviewPanel` video support — webm and mp4 playback via an appropriate decoding library (e.g. libav / FFmpeg); seamless alongside existing stb_image path for images and GIFs

### Persistence
- [ ] Persistent download history — serialize the `DownloadHistoryPanel` ring buffer to disk (e.g. JSON) and reload it on launch so history survives restarts
- [ ] Persistent search history — record per-provider search inputs and surface them as a dropdown in each provider options panel; persist to disk across launches

---

## Phase 4 — Threading Model
> Goal: Parallelise downloads, improve cancellation, unblock UI callback throughput.
> **Depends on: Phase 2**

- [ ] Immediate cancellation — interrupt the in-flight curl transfer on cancel rather than waiting for the current download to complete; requires surfacing a curl abort mechanism through `IHttpClient` and `DownloadRequest`
- [ ] Process multiple UI callbacks per frame (remove single-task-per-frame limit)
- [ ] Per-task cancellation support (cancel individual downloads cleanly)
- [ ] Increase network thread count and benchmark throughput
- [ ] Evaluate dedicated download thread pool separate from API request threads

---

## Phase 4.5 — Credentials Panel
> Goal: Let users enter and persist API credentials inside the app, eliminating the manual config.json copy step.

- [ ] `CredentialsPanel` — new dockable ImGui panel for reading and writing service credentials
  - Reads all known credential keys from `m_UserConfig` (`JsonFile`) on init; if `config.json` does not exist, creates it via `JsonFile::CreateFile()` so first-run works without manual setup
  - One collapsible section per provider (Reddit, Tumblr, Discord); 4chan has no credentials so is omitted
  - Sensitive fields (client secret, API keys) use `ImGuiInputTextFlags_Password` to mask by default with a show/hide toggle
  - Writes back to `m_UserConfig` and calls `Serialise()` immediately on field change — no explicit save button needed
  - Pass `shared_ptr<JsonFile> m_UserConfig` into `CredentialsPanel` via constructor (already available in `App` and `FrontEnd`)
  - Required fields highlighted with a red `ImGui::TextColored` warning label when empty; optional fields have no indicator
- [ ] Services — lazy credential reads
  - Currently `RedditService` caches `m_ClientId` / `m_ClientSecret` in its constructor; switch to reading from `m_UserConfig` at the point of use (before each API request) so credentials updated via the panel take effect without a restart
  - Same pattern for Tumblr and Discord when those are implemented
- [ ] Download gate — block Run when required credentials are missing
  - Add `virtual bool HasRequiredCredentials() const { return true; }` to `Service` base class; override in `RedditService` to return false when `client_id` or `client_secret` is empty in `m_UserConfig`
  - `DownloadOptionsPanel::HandleUserInput()` checks `service->HasRequiredCredentials()` before submitting; shows a log-level warning if missing
- [ ] Deprecate manual config setup — `config.template.json` stays in source control as reference, but the README and in-app first-run experience should guide users to the Credentials panel instead

---

## Phase 5 — New Platforms
> Goal: Expand supported platforms leveraging the improved request and threading layers.
> **Depends on: Phase 3, Phase 4**

- [ ] Discord — implement stub (`DiscordService` already exists, throws `logic_error`)
- [ ] X (Twitter) — new service + request classes
- [ ] Bluesky — new service + request classes
- [ ] Mastodon — new service + request classes (federated, needs instance URL input)
- [ ] Update `config.template.json` and `UserInputOptions` for each new platform

---

## Packaging & Distribution

- [ ] GitHub CI — investigate Actions workflows for: (1) running tests on every push/PR, (2) nightly builds on a schedule, (3) automated tagged release builds that produce a versioned artefact (zip / installer); assess Windows MSVC runner availability, secret handling for any signing step, and artefact retention policy
- [ ] CRT bundling review — investigate whether the MSVC C runtime (`msvcp`/`vcruntime` DLLs) can and should be statically linked or embedded so the release exe is fully self-contained; weigh exe size vs. eliminating the redistributable dependency

## Project Housekeeping

- [ ] Consolidate and migrate roadmap to GitHub issues
  - Close stale / already-done issues: #14 (UI modularisation — done), #9 (preview panel — done)
  - Update issues that are still valid but need detail added from PLAN.md: #3 (sign-out), #10 (immediate cancel), #13 (Tumblr OAuth), #4 (Discord), #7 (Twitter/X), #12 (Mastodon)
  - Decide whether to track #8 (PCH) and #6 (file logger) — add to PLAN.md if wanted, otherwise close as won't-fix
  - Decide whether to add Steam (#11) to Phase 5 new platforms or close
  - Create new issues for items in PLAN.md with no corresponding issue: Credentials panel, username badge, download history tabs, DownloadProgressPanel, persistent history, GitHub CI, CRT bundling, containerisation, etc.
  - Group issues under milestones matching phases (Phase 3, Phase 4, Phase 5, Packaging)
  - Once all items are tracked in GitHub, remove `PLAN.md` from the repo; future planning lives in GitHub issues only
- [ ] README polish — rewrite the README to reflect the current state of the project
  - Replace the sparse setup section with clear step-by-step instructions referencing the in-app Credentials panel (once built) rather than manual config editing
  - Add a Features section covering current capabilities: OAuth2 (Reddit), multi-threaded downloads, media preview, download history, animated GIF support
  - Add a Dependencies / Build section listing the required VS version, platform target, and how to build from source
  - Add a Contributing section referencing the feature branch / PR workflow from `CLAUDE.md`
  - Remove outdated references to manual `config.template.json` copy once the Credentials panel is in place
  - Update credits to include stb_image and Catch2

---

## Long-term / Exploratory

- [ ] Containerisation & self-hosting — investigate running the app headlessly in a Docker container with a web UI so users can self-host and interact via a browser; assess what a service-layer API would look like, how the current ImGui frontend would be replaced or supplemented, and whether the download/auth pipeline needs changes to support a multi-user or remote context
