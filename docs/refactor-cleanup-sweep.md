# Refactor / Cleanup Sweep

## Goal

Document the highest-impact refactor opportunities in the current codebase, with extra focus on:

- reducing duplication through helpers and reusable components
- routing state changes through single source-of-truth functions
- replacing index-based loops where better iteration primitives are possible
- improving memory use, responsiveness, and avoidable work

## Priorities

### 1. Centralize run/auth UI state and keep UI mutations on the main thread

Impact: High
Effort: Medium

Why this matters:

- The current run/sign-in lifecycle is spread across multiple flags in `DownloadOptionsPanel`, which makes it easy for the UI to end up in an invalid combination of states.
- Startup authentication also mutates UI-facing state from background callbacks, which is a stability risk before we even start cleanup work.

Evidence:

- `ImageScraper/src/app/App.cpp:117-136`
  - `AuthenticateServices()` decrements `m_AuthenticatingCount` from service callbacks and calls `m_FrontEnd->SetInputState(...)`.
- `ImageScraper/include/app/App.h:40`
  - `m_AuthenticatingCount` is a plain `int`, even though the callbacks run off the main thread in the OAuth-backed services.
- `ImageScraper/src/services/RedditService.cpp:166-177`
- `ImageScraper/src/services/TumblrService.cpp:174-185`
  - `Authenticate()` work is dispatched through `TaskManager::Submit(...)`, so the callback path is not main-thread-only.
- `ImageScraper/src/ui/DownloadOptionsPanel.cpp:82-85, 115, 182, 196, 223, 244, 274, 285-291`
  - `m_InputState`, `m_Running`, `m_DownloadCancelled`, `m_StartProcess`, and `m_SigningInProvider` are mutated from several places.
- `ImageScraper/src/ui/FrontEnd.cpp:113-119, 164-171, 232-236`
  - run state is mirrored into `LogPanel` and `DownloadProgressPanel` from multiple paths.

Recommended refactor:

- Introduce a small session-state coordinator for the download/auth lifecycle.
- Replace direct flag writes with explicit transition helpers such as:
  - `BeginRun()`
  - `FinishRun()`
  - `RequestCancel()`
  - `BeginSignIn(ContentProvider)`
  - `CancelSignIn()`
  - `CompleteSignIn(ContentProvider)`
  - `SetStartupAuthPending(count)`
- Route all UI-facing state transitions through main-thread callbacks only.
- Make startup auth completion post back through `TaskManager::SubmitMain(...)` before touching `FrontEnd` or panel state.

Good first slice:

- Fix `AuthenticateServices()` first.
- Convert the startup auth counter to main-thread ownership instead of shared mutable state.

### 2. Extract the shared service workflow into reusable download/auth helpers

Impact: High
Effort: Medium to High

Why this matters:

- The provider services follow nearly the same orchestration pattern: validate provider, authenticate if needed, gather media URLs, create a folder, download files, write them, notify progress, and complete on the main thread.
- This duplication increases maintenance cost and makes behavioral fixes easy to miss in one provider.

Evidence:

- `ImageScraper/src/services/RedditService.cpp:67-76`
- `ImageScraper/src/services/TumblrService.cpp:87-96`
- `ImageScraper/src/services/FourChanService.cpp:19-28`
  - nearly identical `HandleUserInput()` dispatch shape
- `ImageScraper/src/services/RedditService.cpp:92-135`
- `ImageScraper/src/services/TumblrService.cpp:103-146`
  - nearly identical `HandleExternalAuth()` flow
- `ImageScraper/src/services/RedditService.cpp:152-180`
- `ImageScraper/src/services/TumblrService.cpp:160-188`
  - duplicated refresh/authenticate flow
- `ImageScraper/src/services/RedditService.cpp:364-425`
- `ImageScraper/src/services/TumblrService.cpp:335-402`
- `ImageScraper/src/services/FourChanService.cpp:188-247`
  - duplicated download/write/progress loop

Recommended refactor:

- Add a reusable `DownloadPipeline` helper that owns:
  - cancellation checks
  - per-file progress reporting
  - download-to-disk behavior
  - file naming and completion notifications
- Consider an `AuthenticatedServiceBase` or `OAuthBackedService` helper for:
  - `HandleExternalAuth()`
  - `Authenticate()`
  - `SignOut()`
  - shared signed-in username handling
- Keep provider-specific code focused on "build request options" and "extract media URLs."

Good first slice:

- Extract only the file download/write loop first.
- Then extract the OAuth completion/refresh flow shared by Reddit and Tumblr.

### 3. Break up `DownloadHistoryPanel` into smaller helpers and centralize selection/deletion

Impact: High
Effort: Medium

Why this matters:

- `DownloadHistoryPanel.cpp` is one of the largest first-party files and contains several responsibilities: rendering, selection, deletion, persistence, thumbnail caching, navigation, and filesystem cleanup.
- The same selection/delete logic appears in multiple places, which makes the panel harder to extend and easy to regress.

Evidence:

- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:66-89`
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:320-340`
  - "delete selected" logic is duplicated for button and Delete key handling
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:263-270`
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:395-430`
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:433-461`
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:631-648`
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:716-720`
  - selection + preview propagation is repeated in multiple forms

Recommended refactor:

- Extract focused helpers such as:
  - `DeleteSelectedEntry()`
  - `DeleteEntriesUnder(path)`
  - `SetSelection(index, previewBehavior)`
  - `SelectNearestExisting(index)`
  - `RenderDeleteModals()`
- Separate pure history-model operations from ImGui rendering code.
- Make preview callback triggering part of a single selection API instead of manual repeated calls.

Good first slice:

- Deduplicate the delete-selected path.
- Then centralize selection changes behind one helper.

### 4. Modernize `RingBuffer` so history code can use range-style iteration

Impact: Medium to High
Effort: Medium

Why this matters:

- A lot of the current index-heavy code is there because `RingBuffer` exposes signed indexing and does not provide an easy, correct iteration API for wrapped storage.
- That makes reverse loops fragile and blocks the move to cleaner range-style traversal in the history panel.

Evidence:

- `ImageScraper/include/collections/RingBuffer.h:45-60, 82-89, 133-150`
  - `RemoveAt`, `GetSize`, `GetCapacity`, and `operator[]` all use `int`
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:234, 351, 408, 419, 435, 451, 466, 479, 627, 660`
  - repeated manual index loops over history

Recommended refactor:

- Either:
  - upgrade `RingBuffer` with real iterators/views and `size_t`-based APIs, or
  - replace it with a capped `std::deque` / `std::vector` if the fixed size of 200 items makes simpler storage more valuable than ring semantics
- Also clean up low-level container behavior:
  - `Push(T&&)` / `emplace`
  - `Front()` / `Back()` returning references
  - reverse iteration helpers or snapshot views

Good first slice:

- Decide whether `RingBuffer` should survive.
- If yes, add safe forward/reverse iteration helpers before mass loop cleanup.

### 5. Reduce download-time memory churn by streaming files directly to disk

Impact: High
Effort: Medium

Why this matters:

- The services currently download each asset into a `std::vector<char>` and then write that buffer to disk.
- That means large media files are fully duplicated in memory before they ever touch the filesystem.

Evidence:

- `ImageScraper/src/services/RedditService.cpp:384-414`
- `ImageScraper/src/services/TumblrService.cpp:359-389`
- `ImageScraper/src/services/FourChanService.cpp:192-234`
  - download into memory, then write out
- `ImageScraper/src/requests/DownloadRequest.cpp:91-93`
  - `WriteCallback()` grows the vector with `insert(...)`, which can trigger repeated reallocations and copies

Recommended refactor:

- Add a file-backed download mode to `DownloadRequest`.
- Stream directly into `std::ofstream` or a temporary file, then finalize/rename on success.
- Keep the current in-memory mode only for the rare cases that actually need it.

Good first slice:

- Introduce `PerformToFile(...)` and migrate one provider first.

### 6. Cut avoidable UI-thread I/O in history and preview

Impact: High
Effort: Medium

Why this matters:

- The UI currently performs a lot of filesystem checks and media decoding in render-time paths.
- With a larger history or slower storage, that can show up as stutter or hover lag.

Evidence:

- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:239, 361, 410, 421, 437, 453, 468, 481, 607, 663`
  - `std::filesystem::exists(...)` is called repeatedly from update/navigation/save paths
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:748-804, 834-859`
  - thumbnail loading is synchronous on the UI thread
- `ImageScraper/src/ui/MediaPreviewPanel.cpp:233-259`
  - `ClearPreview()` waits for any in-flight decode to finish
- `ImageScraper/src/ui/MediaPreviewPanel.cpp:546-621`
  - preview decoding reads the whole file into memory before decoding it

Recommended refactor:

- Track missing/deleted files as part of history maintenance instead of probing them every frame.
- Move thumbnail generation behind a small async worker/cache.
- Avoid blocking the UI while waiting for decode cancellation.
- Prefer direct-file decode paths where available instead of staging the whole file in memory.

Good first slice:

- Start with async thumbnail loading and batched file-existence cleanup.

### 7. Reuse HTTP client/request infrastructure instead of rebuilding it in hot paths

Impact: Medium
Effort: Medium

Why this matters:

- Many request objects build their own `RetryHttpClient(CurlHttpClient)` wrapper by default, and services instantiate request objects repeatedly inside loops.
- That is extra allocation/setup churn and works against the request-layer abstraction that already exists.

Evidence:

- `ImageScraper/src/requests/reddit/FetchAccessTokenRequest.cpp:11-18`
- `ImageScraper/src/requests/tumblr/TumblrFetchAccessTokenRequest.cpp:10-17`
  - request objects own their own default client stack
- `ImageScraper/src/services/RedditService.cpp:316-317, 392-393`
- `ImageScraper/src/services/TumblrService.cpp:302-303, 367-368`
- `ImageScraper/src/services/FourChanService.cpp:88-89, 131-132, 212-213`
  - request objects are recreated inside provider workflows

Recommended refactor:

- Share one provider-level or app-level `IHttpClient`.
- Inject it into request objects or collapse thin request types into reusable request builders.
- Keep the mockable request seam for tests, but stop recreating the client stack in tight loops.

### 8. Consolidate provider-panel form building and config persistence

Impact: Medium
Effort: Medium

Why this matters:

- The provider panels repeat the same search-history input widget and the same "load config, clamp value, save config" pattern.
- Config writes are also very eager, which increases duplication and unnecessary disk writes.

Evidence:

- `ImageScraper/src/ui/RedditPanel.cpp:28-117`
- `ImageScraper/src/ui/TumblrPanel.cpp:27-90`
- `ImageScraper/src/ui/FourChanPanel.cpp:27-90`
  - repeated search input + dropdown history + max-download config persistence
- `ImageScraper/src/ui/CredentialsPanel.cpp:63-121, 161-169`
  - reusable field rendering already exists locally as a lambda, but the broader config metadata is still duplicated
- `ImageScraper/src/ui/SearchHistory.cpp:79-96`
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp:651-695`
  - frequent small `Serialise()` calls across the UI

Recommended refactor:

- Extract a reusable `SearchInputWithHistory` control shared by provider panels.
- Move provider settings metadata into descriptor tables so the UI is data-driven.
- Add debounced or batched persistence in `JsonFile` or a small config repository layer.

Good first slice:

- Extract the shared search-history input widget first.

### 9. Drain more than one main-thread callback per frame

Impact: Medium
Effort: Low

Why this matters:

- `ThreadPool::Update()` only executes a single queued main-thread task each frame.
- That limits throughput for completion callbacks and can delay UI-visible updates unnecessarily.

Evidence:

- `ImageScraper/src/async/ThreadPool.cpp:54-71`
  - only one `m_MainQueue` task is popped and run per frame
- `ImageScraper/src/app/App.cpp:98`
  - `TaskManager::Instance().Update()` is called once per frame

Recommended refactor:

- Drain the main queue until empty, or up to a time/task budget per frame.
- Preserve responsiveness by using a small cap if needed, but avoid the fixed one-task bottleneck.

### 10. Finish the still-image decoder migration and retire `stb` if it becomes unused

Impact: Medium
Effort: Medium

Why this mattered:

- We previously had two decode paths for still media: `stb` handled most image preview and thumbnail work, while FFmpeg handled video decode and a few still-image fallbacks.
- That split helped unblock Bluesky preview bugs, but it left duplicate responsibilities in place and made decoder behavior harder to reason about.
- Once FFmpeg covered still images and GIF playback well enough, we could retire the remaining first-party `stb` dependency from the app cleanly.

Evidence:

- `ImageScraper/src/ui/MediaPreviewPanel.cpp`
  - preview decode now routes still images and GIF playback through the FFmpeg-backed `VideoPlayer` helpers instead of `stb`.
- `ImageScraper/src/ui/DownloadHistoryPanel.cpp`
  - thumbnail decoding now reuses the same FFmpeg first-frame path as preview decode.
- `ImageScraper/include/ui/VideoPlayer.h`
  - `DecodeFirstFrameFile(...)` provides the shared FFmpeg decode entry point for still-image preview and thumbnails.
- `ImageScraper/ImageScraper.vcxproj`
  - the app no longer compiles `src/stb/stb_image.cpp`.

Status:

- Completed in `codex/retire-stb`.
- The vendored `stb_image_write` block in `imgui_draw.cpp` remains disabled and is still treated as non-blocking.

Recommended refactor:

- Keep the shared FFmpeg first-frame helper as the single decode path for still-image preview and thumbnail generation.
- If future media types need custom handling, add them around the shared FFmpeg helper rather than reintroducing a parallel still-image decoder.
- Treat the disabled `stb_image_write` block in vendored `imgui_draw.cpp` as non-blocking unless we ever enable that debug code.

Good first slice:

- Completed: thumbnail decode and still-image preview now use the FFmpeg-backed path.
- Completed: the remaining app-side `stb` usage has been removed from preview decode and the project build.

### 11. Migrate repo-authored text files to LF-first line endings in a dedicated cleanup PR

Impact: Medium
Effort: Low to Medium

Why this matters:

- The repo previously enforced `CRLF` broadly, and we had already hit recurring mixed-line-ending churn while touching otherwise small changes.
- Most modern cross-platform tools and Git workflows behave more predictably with `LF` as the default text-file format.
- This is a good cleanup candidate, but it should stay separate from feature work so review stays readable.

Evidence:

- `.gitattributes`
  - repo-authored text files now default to `LF`, while Windows-native scripts stay on `CRLF` and vendored folders keep their existing line endings.
- `.editorconfig`
  - editor defaults now match the `LF`-first policy.
- `AGENTS.md`
  - contributor guidance now tells agents to preserve `LF` in repo-authored text files and avoid mixed endings.

Status:

- Completed in `codex/uniform-line-endings`.
- Remaining follow-up is just to keep future edits aligned with the new policy.

Recommended refactor:

- Wait until the current Bluesky PR stack is merged, then do the line-ending migration as its own branch and PR.
- Update `.gitattributes` and `.editorconfig` to use `LF` by default for source, headers, docs, JSON, YAML, and project-authored metadata.
- Keep `CRLF` only where we intentionally want Windows-native script behavior, such as `*.bat` and `*.cmd`, and decide explicitly whether `*.ps1` stays `CRLF`.
- Renormalize the repo in one pass so future diffs stop carrying mixed-line-ending noise.
- Update `AGENTS.md` and any other contributor docs to match the new policy.

Good first slice:

- Open a dedicated post-Bluesky cleanup branch for the config-file update plus renormalization only.
- Keep the PR description explicit that the change is whitespace-only except for the policy docs.

## Sweep Checklist

### Phase 1: Stability and ownership

- [x] Main-thread-only UI state transitions
- [x] Startup auth counter/state cleanup
- [x] Centralized run/sign-in transition helpers

### Phase 2: Shared workflow extraction

- [x] Shared download/write pipeline
- [x] Shared OAuth-backed service behavior
- [x] Shared provider-panel search input helper

### Phase 3: Iteration and container cleanup

- [x] Decide whether to keep `RingBuffer`
- [x] Add safe iteration APIs
- [x] Replace manual index loops where the container now supports better traversal

### Phase 4: Performance pass

- [x] Stream downloads directly to disk
- [x] Async thumbnail loading
- [x] Reduce preview decode blocking
- [x] Drain more main-thread tasks per frame
- [x] Reuse HTTP client instances

### Phase 5: Post-Bluesky cleanup

- [x] Finish the FFmpeg still-image migration and remove `stb` if it becomes unused
- [x] Migrate repo-authored text files to LF-first line endings in a dedicated PR

## Best First Refactor Set

If we want the most value with the least churn, start with this bundle:

1. Fix startup auth state ownership and main-thread dispatch.
2. Add `DownloadOptionsPanel` transition helpers so state writes stop being ad hoc.
3. Extract the shared file download/write loop from the three services.
4. Deduplicate `DownloadHistoryPanel` selection/deletion helpers.
5. Add a reusable provider search input widget.

That sequence improves correctness first, then removes the duplication that will otherwise make later cleanup harder.
