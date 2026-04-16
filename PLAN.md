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
- [ ] Reddit deauthorise all sessions — explicit server-side token revocation as a separate destructive action distinct from sign-out
  - Calls `RevokeAccessTokenRequest` (already implemented) to hit `POST /api/v1/revoke_token`; invalidates the refresh token and all associated access tokens on Reddit's servers
  - Exposed as a separate button (e.g. "Deauthorise App") rather than part of the normal Sign Out flow, since it carries the multi-minute re-auth delay; user should be warned before proceeding
  - After the revoke request completes, calls `SignOut()` to clear tokens locally
  - Useful when the user suspects their credentials are compromised or wants to fully revoke app access from Reddit's end
- [ ] OAuth2 for Tumblr — implement OAuth2 sign-in flow (similar to Reddit); update `TumblrPanel`, request classes, and `config.template.json`
- [ ] Sign-out for Tumblr — same pattern as Reddit once OAuth2 is implemented
- [x] Remove `CanSignIn()` from `FourChanPanel` / hide sign-in UI for 4chan — `CanSignIn()` returns `false`; `DownloadOptionsPanel::UpdateSignInButton()` early-returns, so no sign-in UI is rendered for 4chan
- [x] OAuth2 redirect HTML polish — dark-themed page with Reddit brand colour, checkmark icon, clear "Signed in to Reddit — close this tab" copy; removed external image fetch

### New windows
- [x] `MediaPreviewPanel` — loads last downloaded image into an OpenGL texture (stb_image) and renders it in a dockable ImGui window; supports static images and animated GIFs (frame stepping)
- [x] `DownloadHistoryPanel` — ring buffer of completed downloads showing filename, source URL, and timestamp; clicking an entry opens it in explorer
- [x] `DownloadProgressPanel` — extract current and total download progress bars out of `LogPanel` into a dedicated dockable panel; `LogPanel` retains log lines only
- [ ] `DownloadHistoryPanel` provider tabs — add a tab strip per provider so history is filterable by source; an "All" tab shows the combined view
- [ ] `DownloadHistoryPanel` extra param columns — show provider-specific download parameters as additional columns (e.g. Download Scope / sort for Reddit: Hot, Best, New, Top, etc.)
- [x] `DownloadHistoryPanel` hover tooltip preview — show a small single-frame thumbnail of the image in an ImGui tooltip when hovering a history entry; skip non-image and large files gracefully
- [x] `MediaPreviewPanel` video support — webm and mp4 playback via an appropriate decoding library (e.g. libav / FFmpeg); seamless alongside existing stb_image path for images and GIFs
- [ ] `MediaPreviewPanel` video audio playback — decode and play audio streams alongside video using WASAPI (no new third-party dependency); requires A/V sync via audio PTS as master clock

### Persistence
- [ ] Persistent download history — serialize the `DownloadHistoryPanel` ring buffer to disk (e.g. JSON) and reload it on launch so history survives restarts
- [ ] Persistent search history — record per-provider search inputs and surface them as a dropdown in each provider options panel; persist to disk across launches

---

## Phase 4 — Threading Model
> Goal: Parallelise downloads, improve cancellation, unblock UI callback throughput.
> **Depends on: Phase 2**

- [x] Immediate cancellation — `DownloadRequest::ProgressCallback` returns non-zero when `IsCancelled()` is true, triggering `CURLE_ABORTED_BY_CALLBACK` which unblocks `request.perform()` immediately; cancellation-caused `RuntimeError` is logged at debug level rather than as an error
- [ ] Process multiple UI callbacks per frame (remove single-task-per-frame limit)
- [ ] Per-task cancellation support (cancel individual downloads cleanly)
- [ ] Increase network thread count and benchmark throughput
- [ ] Evaluate dedicated download thread pool separate from API request threads

---

## Phase 4.5 — Credentials Panel
> Goal: Let users enter and persist API credentials inside the app, eliminating the manual config.json copy step.

- [x] `CredentialsPanel` — new dockable ImGui panel for reading and writing service credentials
  - Reads all known credential keys from `m_UserConfig` (`JsonFile`) on init; if `config.json` does not exist, creates it via `JsonFile::CreateFile()` so first-run works without manual setup
  - One collapsible section per provider (Reddit, Tumblr, Discord); 4chan has no credentials so is omitted
  - Sensitive fields (client secret, API keys) use `ImGuiInputTextFlags_Password` to mask by default with a show/hide toggle
  - Writes back to `m_UserConfig` and calls `Serialise()` immediately on field change — no explicit save button needed
  - Pass `shared_ptr<JsonFile> m_UserConfig` into `CredentialsPanel` via constructor (already available in `App` and `FrontEnd`)
  - Required fields highlighted with a red `ImGui::TextColored` warning label when empty; optional fields have no indicator
  - ImGui modal warning popup shown when Run is clicked with missing credentials — avoids soft-lock
- [x] Services — lazy credential reads
  - `RedditService` now reads `m_ClientId` / `m_ClientSecret` from `m_UserConfig` at the point of use; credentials updated via the panel take effect without a restart
  - Same pattern applied to `TumblrService`
- [x] Download gate — block Run when required credentials are missing
  - `virtual bool HasRequiredCredentials() const { return true; }` added to `Service` base; `RedditService` and `TumblrService` override to check their required keys
  - `DownloadOptionsPanel::HandleUserInput()` checks `HasRequiredCredentials()` and shows a modal warning popup if missing; `m_Running` only set after service confirms dispatch to prevent soft-lock
- [x] Deprecate manual config setup — `config.template.json` stays in source control as reference; README and in-app Credentials panel guide users; developer credentials stored in `data/config.json` (gitignored) and copied to output dir by pre-build event
- [x] Dev credentials backup — `CredentialsPanel` shows a **Debug-only** "Save credentials to source data/" checkbox; when checked, every field save copies `config.json` back to `ImageScraper/data/config.json` via `std::filesystem::copy_file` using `__FILE__`-derived path, keeping dev credentials persistent across rebuilds without manual xcopy
- [ ] Remove `config.template.json` — the app auto-creates `config.json` on first run via `JsonFile::Deserialise()`; the template is redundant and misleading
  - Remove `config.template.json` from source control
  - Remove `xcopy config.template.json` from all pre-build event configurations in `ImageScraper.vcxproj`
  - Remove from release zip contents in `.github/workflows/release.yml` and `PLAN.md` release notes
  - Update README to remove any remaining references to the template file

---

## Phase 5 — New Platforms
> Goal: Expand supported platforms leveraging the improved request and threading layers.
> **Depends on: Phase 3, Phase 4**

- [ ] Discord — implement stub (`DiscordService` already exists, throws `logic_error`)
- [ ] X (Twitter) — new service + request classes
- [ ] Bluesky — new service + request classes
- [ ] Mastodon — new service + request classes (federated, needs instance URL input)
- [ ] Redgifs — new service + request classes for redgifs.com; Reddit frequently embeds Redgifs URLs for GIF content so this unlocks a large class of media currently skipped; requires investigating the Redgifs public API (content discovery endpoint, pagination, auth if needed) and adding a provider panel; primary API reference is github.com/scrazzz/redgifs (api.py)
- [ ] Update `config.template.json` and `UserInputOptions` for each new platform

---

## Dependency Management

- [ ] Migrate binary dependencies to vcpkg — replace committed `lib/` binaries (libcurl, libcurlpp, glfw3, FFmpeg) with vcpkg manifests; eliminates binary blobs from git history, gives reproducible builds with pinned versions, and makes CI straightforward; assess MSVC triplet compatibility and whether dynamic vs. static linking choices are preserved

---

## Packaging & Distribution

### Versioning
- [x] Add `VERSION` file to repo root containing `MAJOR.MINOR.PATCH` (e.g. `1.0.0`) — canonical version source consumed by CI for artefact naming and release titles
- [x] Add `Version.h` generation step to CI — a pre-workflow step reads `VERSION` and writes `ImageScraper/include/version/Version.h` with `constexpr` version strings; generated file is not committed, only exists during the CI run; local builds compile with whatever `Version.h` was last committed (update manually before tagging a release)
  - `Version.h` exposes `VERSION_MAJOR`, `VERSION_MINOR`, `VERSION_PATCH`, and `VERSION_STRING`
  - Add `ImageScraper/include/version/` subfolder and register `Version.h` in `ImageScraper.vcxproj` as a `<ClInclude>`

### Static CRT
- [x] CRT distribution — copy `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` from the VS redist package in `release.yml` using `vswhere`; DLLs land in the output directory before zipping so the release is self-contained with no user prerequisite

### GitHub Actions
- [x] Create `.github/workflows/ci.yml` — PR and push gate on `development`
  - Trigger: `pull_request` targeting `development`; `push` to `development`
  - Runner: `windows-latest` (VS2022, v143 toolset, Windows 10 SDK — matches vcxproj exactly)
  - Steps: `actions/checkout` → `microsoft/setup-msbuild` → MSBuild `Debug|x64` → re-run test exe with `--reporter junit` → publish JUnit XML to PR summary
  - MSBuild invocation: `msbuild ImageScraper.sln /p:Configuration=Debug /p:Platform=x64 /m /nologo /verbosity:minimal`
  - Post-build event already runs tests and exits non-zero on failure — MSBuild step fails automatically if any test fails
  - Upload `ImageScraperTests.exe` and its DLL as a workflow artefact (7-day retention) for local investigation of CI failures
  - Permissions: `contents: read`

- [x] Create `.github/workflows/release.yml` — release build and publish
  - Trigger: `push` of tag matching `v*.*.*`; `workflow_dispatch` (manual)
  - Runner: `windows-latest`
  - Steps:
    1. `actions/checkout`
    2. `microsoft/setup-msbuild`
    3. Generate `Version.h` from `VERSION` file (PowerShell one-liner)
    4. MSBuild `Release|x64` — xcopy post-build events automatically copy all DLLs and data files to `bin\ImageScraper\x64\Release\`
    5. Re-run test exe (Release build) — block release if any test fails
    6. `Compress-Archive` the Release output directory to `ImageScraper-v{VERSION}-x64.zip`
    7. `softprops/action-gh-release@v2` — create GitHub Release with the zip as an asset; use tag annotation as release body
    8. Upload `.pdb` as a workflow artefact (30-day retention) for crash debugging
  - Permissions: `contents: write`
  - Release zip contents (assembled by existing post-build xcopy events): `ImageScraper.exe`, `libcurl.dll`, `curl-ca-bundle.crt`, `config.template.json`, `auth.html`, `imgui.ini`, `avformat-62.dll`, `avcodec-62.dll`, `avutil-60.dll`, `avdevice-62.dll`, `avfilter-11.dll`, `swscale-9.dll`, `swresample-6.dll`
  - x64 only; no x86 (Win32 configurations are non-functional)

### CLAUDE.md correction
- [x] Update `CLAUDE.md` branch diagram — stable branch is `master` not `main`; correct the diagram and any references

## Licensing & Open Source Readiness

- [x] Add `THIRD_PARTY_LICENSES.md` — document every vendored dependency with its name, version, copyright notice, and full license text; required for LGPL (FFmpeg) and good practice for all others
  - FFmpeg (LGPL 2.1+): avcodec, avformat, avutil, avdevice, avfilter, swscale, swresample — dynamic linking satisfies LGPL; users must be able to swap DLLs
  - Dear ImGui v1.89.5 (MIT)
  - GLFW 3.3 (BSD 3-Clause)
  - libcurl 8.0.1 (curl/MIT-style)
  - curlpp 0.8.1 (MIT)
  - nlohmann/json v3.11.2 (MIT)
  - cppcodec (MIT)
  - stb_image v2.30 (MIT / Public Domain dual)
  - Catch2 v3.14.0 (Boost Software License 1.0)
- [x] Add FFmpeg attribution to README — brief credits section naming FFmpeg and linking to https://ffmpeg.org; note that LGPL-licensed DLLs are distributed alongside the application
- [ ] Decide how to distribute FFmpeg DLLs — the existing `lib/` approach (matching curl/GLFW) is simplest; alternatively attach to GitHub Releases to avoid binary blobs in git history

---

## Project Housekeeping

- [ ] Consolidate and migrate roadmap to GitHub issues
  - Close stale / already-done issues: #14 (UI modularisation — done), #9 (preview panel — done)
  - Update issues that are still valid but need detail added from PLAN.md: #3 (sign-out), #10 (immediate cancel), #13 (Tumblr OAuth), #4 (Discord), #7 (Twitter/X), #12 (Mastodon)
  - Decide whether to track #8 (PCH) and #6 (file logger) — add to PLAN.md if wanted, otherwise close as won't-fix
  - Decide whether to add Steam (#11) to Phase 5 new platforms or close
  - Create new issues for items in PLAN.md with no corresponding issue: Credentials panel, username badge, download history tabs, DownloadProgressPanel, persistent history, GitHub CI, CRT bundling, containerisation, etc.
  - Group issues under milestones matching phases (Phase 3, Phase 4, Phase 5, Packaging)
  - Once all items are tracked in GitHub, remove `PLAN.md` from the repo; future planning lives in GitHub issues only
- [x] README polish — rewrite the README to reflect the current state of the project

---

## Long-term / Exploratory

- [ ] Containerisation & self-hosting — investigate running the app headlessly in a Docker container with a web UI so users can self-host and interact via a browser; assess what a service-layer API would look like, how the current ImGui frontend would be replaced or supplemented, and whether the download/auth pipeline needs changes to support a multi-user or remote context
