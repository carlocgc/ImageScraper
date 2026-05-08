# UI Panel Refactor Plan

## Goal

Reduce the structural complexity in `DownloadHistoryPanel` and `MediaPreviewPanel` by extracting deep component objects that own workflow and state, while keeping current user-visible behaviour stable.

This document is intended to be worked through across multiple sessions. Each phase should be small enough to review independently and should leave the application in a working state.

## Scope

In scope:

- `ImageScraper/src/ui/DownloadHistoryPanel.cpp`
- `ImageScraper/include/ui/DownloadHistoryPanel.h`
- `ImageScraper/src/ui/MediaPreviewPanel.cpp`
- `ImageScraper/include/ui/MediaPreviewPanel.h`
- `ImageScraper/src/ui/MediaPreviewControlPanel.cpp` only as a caller that may need small adjustments
- `ImageScraperTests/src/tests/UiStateTests.cpp`
- new focused UI support classes under `ImageScraper/include/ui/` and `ImageScraper/src/ui/`

Out of scope:

- visual redesign
- new playback or history features
- vendored code
- adding third-party dependencies
- broad UI architecture changes outside these panels

## Refactoring Principles

- Extract workflow owners, not cosmetic helper functions.
- Keep public panel interfaces small and stable where practical.
- Prefer boundary tests around extracted objects over growing tests against panel internals.
- Preserve persisted config keys and current selection / preview behaviour unless a phase explicitly changes them.
- Keep each phase independently shippable.

## Session Rules

For each phase:

1. create or update tests first where practical
2. extract one coherent responsibility
3. keep the panel compiling and behaving the same at the end of the session
4. update this document if scope, sequencing, or risks change

## Recommended Sequence

1. `DownloadDeleteController`
2. `DownloadHistorySelectionController`
3. `ThumbnailCacheService`
4. `DownloadHistoryTreeModel`
5. `PreviewLoadCoordinator`
6. `MediaPlaybackController`
7. Optional `PreviewPresentationModel`

The sequence matters. Early phases remove the highest workflow density with the least UI churn. Later phases become easier once the selection, deletion, and loading seams are explicit.

---

## Phase 1 - Extract `DownloadDeleteController`

### Problem

`DownloadHistoryPanel` currently owns delete initiation, delete plan construction, asynchronous execution, progress bookkeeping, minimum-visible timing, and post-delete selection recovery. That makes delete changes risky because UI code and delete workflow code are interleaved.

### Responsibilities To Move

- delete context creation
- delete plan construction for files and directories
- worker future lifecycle for delete execution
- delete progress state and stage transitions
- minimum visible duration policy for progress modal

### Proposed Interface

The panel should remain responsible for opening the confirmation popup and for deciding when a delete action is requested. The extracted controller should own execution and progress state.

Illustrative shape:

```cpp
class DownloadDeleteController
{
public:
    struct Request;
    struct Progress;
    enum class Stage;
    struct Result;

    bool IsActive( ) const;
    void Start( Request request );
    void Update( );
    std::optional<Result> ConsumeCompletedResult( );
    Progress GetProgress( ) const;
    Stage GetStage( ) const;
};
```

### Files Likely To Change

- Modify: `ImageScraper/include/ui/DownloadHistoryPanel.h`
- Modify: `ImageScraper/src/ui/DownloadHistoryPanel.cpp`
- Create: `ImageScraper/include/ui/DownloadDeleteController.h`
- Create: `ImageScraper/src/ui/DownloadDeleteController.cpp`
- Modify: `ImageScraper/ImageScraper.vcxproj`
- Modify: `ImageScraper/ImageScraper.vcxproj.filters`
- Modify: `ImageScraperTests/src/tests/UiStateTests.cpp`
- Modify: `ImageScraperTests/ImageScraperTests.vcxproj`
- Modify: `ImageScraperTests/ImageScraperTests.vcxproj.filters` only if a new test file is added

### Test Strategy

- keep current delete progress math tests
- add controller-level tests for:
  - file delete progress setup
  - directory delete plan ordering
  - progress transitions from scanning to deleting
  - minimum-visible completion timing
  - failed delete preserving an error result

### Risks

- accidental behaviour changes in selection recovery after deletion
- coupling between worker-thread progress updates and panel-owned callbacks

### Non-Goals

- changing delete confirmation UX
- changing delete rules or permissions

### Exit Criteria

- `DownloadHistoryPanel` no longer owns delete worker orchestration
- delete modal still shows the same progress semantics
- delete-related tests still pass

### Session Handoff Checklist

- note whether post-delete selection still lives in the panel or was partially moved
- note any thread-safety assumptions added by the controller
- record any remaining delete-only helpers still stranded in the panel

### Session Notes

- 2026-05-08: initial Phase 1 slice extracted `DownloadDeleteController` for delete execution, progress state, and timing.
- post-delete selection recovery still lives in `DownloadHistoryPanel` by design and should remain there until Phase 2.

---

## Phase 2 - Extract `DownloadHistorySelectionController`

### Problem

Selection state currently drives persistence, auto-preview, scroll-to-selected, keyboard navigation, and deletion recovery. That policy is spread through multiple panel methods, which makes it hard to reason about what selecting a path is supposed to do.

### Responsibilities To Move

- selected path state
- selected path persistence
- preview callback routing for file vs directory selection
- next / previous navigation
- selection advancement after deletion

### Proposed Interface

The panel and delete workflow should talk to one selection API instead of mutating selection fields directly.

Illustrative shape:

```cpp
class DownloadHistorySelectionController
{
public:
    void Load( std::shared_ptr<JsonFile> appConfig, const std::string& downloadsRoot );
    void SetSelection( const std::string& path, bool scrollToSelected, bool requestPreview );
    void ClearSelection( bool requestPreview );
    void AdvanceAfterRemoval( const std::vector<std::filesystem::path>& navigableFiles, int preferredIndex );

    const std::string& GetSelectedPath( ) const;
    bool HasSelection( ) const;
    bool ShouldScrollToSelected( ) const;
    void ConsumeScrollToSelected( );
};
```

### Files Likely To Change

- Modify: `ImageScraper/include/ui/DownloadHistoryPanel.h`
- Modify: `ImageScraper/src/ui/DownloadHistoryPanel.cpp`
- Create: `ImageScraper/include/ui/DownloadHistorySelectionController.h`
- Create: `ImageScraper/src/ui/DownloadHistorySelectionController.cpp`
- Modify: `ImageScraper/ImageScraper.vcxproj`
- Modify: `ImageScraper/ImageScraper.vcxproj.filters`
- Modify: `ImageScraperTests/src/tests/UiStateTests.cpp`

### Test Strategy

- add focused tests for:
  - selecting a file requests preview
  - selecting a directory clears preview
  - invalid selection clears safely
  - persistence round-trip for selected path
  - next / previous navigation behaviour with a supplied navigable list

### Risks

- hidden dependence on panel-owned fields such as `m_ScrollToSelected`
- selection and preview callbacks may still be too tightly coupled if the interface is too thin

### Non-Goals

- changing tree navigation order
- changing persisted config keys

### Exit Criteria

- selection state is no longer spread across the panel
- delete flow and keyboard navigation both use the same selection owner

### Session Handoff Checklist

- note whether preview callback policy is fully centralized
- note whether path-existence checks are still duplicated between panel and controller

---

## Phase 3 - Extract `ThumbnailCacheService`

### Problem

Thumbnail loading is currently mixed into hover tooltip rendering. The panel owns cache entries, in-flight tracking, decode futures, upload handoff, and eviction policy, even though rendering only needs thumbnail state for a path.

### Responsibilities To Move

- thumbnail cache map
- in-flight request suppression
- worker decode scheduling
- decoded thumbnail handoff to the main thread
- cache eviction for file and directory removal

### Proposed Interface

The panel should ask for a thumbnail view object and call one update hook per frame.

Illustrative shape:

```cpp
class ThumbnailCacheService
{
public:
    struct Entry
    {
        GLuint m_Texture{ 0 };
        int    m_Width{ 0 };
        int    m_Height{ 0 };
        bool   m_IsLoading{ false };
    };

    ~ThumbnailCacheService( );
    void Update( );
    Entry GetOrRequest( const std::string& filepath );
    void Evict( const std::string& filepath );
    void EvictWithin( const std::filesystem::path& rootPath );
};
```

### Files Likely To Change

- Modify: `ImageScraper/include/ui/DownloadHistoryPanel.h`
- Modify: `ImageScraper/src/ui/DownloadHistoryPanel.cpp`
- Create: `ImageScraper/include/ui/ThumbnailCacheService.h`
- Create: `ImageScraper/src/ui/ThumbnailCacheService.cpp`
- Modify: `ImageScraper/ImageScraper.vcxproj`
- Modify: `ImageScraper/ImageScraper.vcxproj.filters`
- Add or modify tests in `ImageScraperTests/src/tests/`

### Test Strategy

- add tests for:
  - supported media path detection
  - repeated requests not creating duplicate in-flight work
  - file eviction clearing cached state
  - directory eviction clearing all descendants

### Risks

- OpenGL texture ownership must stay main-thread safe
- current tooltip code may assume cache and UI state share the same object lifetime

### Non-Goals

- changing thumbnail size or rendering style
- retry logic for failed thumbnail decodes

### Exit Criteria

- `DownloadHistoryPanel` no longer owns thumbnail futures or cache containers
- tooltip rendering becomes a thin consumer of thumbnail state

### Session Handoff Checklist

- note whether decode helper functions also need reuse by preview code later
- note any OpenGL lifetime constraints that must stay documented

---

## Phase 4 - Extract `DownloadHistoryTreeModel`

### Problem

The panel currently owns filesystem traversal, sorting, snapshot caching, open directory state, and navigable file generation. That leaves rendering tightly coupled to filesystem and cache policy.

### Responsibilities To Move

- tree snapshot building
- sort-column and sort-direction application
- open directory state
- rebuild invalidation and cooldown policy
- navigable file list generation

### Proposed Interface

The panel should ask the model for a render snapshot and forward UI events that change open-state or invalidate the tree.

Illustrative shape:

```cpp
class DownloadHistoryTreeModel
{
public:
    struct NodeSnapshot;

    void SetRoot( const std::filesystem::path& downloadsRoot );
    void Invalidate( bool fromDownload );
    void SetSort( ImGuiID columnUserId, ImGuiSortDirection direction );
    const std::optional<NodeSnapshot>& GetSnapshot( ) const;
    const std::vector<std::filesystem::path>& GetNavigableFiles( ) const;
    void SetDirectoryOpen( const std::string& path, bool isOpen );
    bool IsDirectoryOpen( const std::string& path ) const;
};
```

### Files Likely To Change

- Modify: `ImageScraper/include/ui/DownloadHistoryPanel.h`
- Modify: `ImageScraper/src/ui/DownloadHistoryPanel.cpp`
- Create: `ImageScraper/include/ui/DownloadHistoryTreeModel.h`
- Create: `ImageScraper/src/ui/DownloadHistoryTreeModel.cpp`
- Modify: `ImageScraper/ImageScraper.vcxproj`
- Modify: `ImageScraper/ImageScraper.vcxproj.filters`
- Add or modify tests in `ImageScraperTests/src/tests/`

### Test Strategy

- build tree-model tests against temporary directories
- verify:
  - default sort behaviour
  - sort changes invalidating cache
  - open-state affecting navigable-file order
  - download-triggered rebuild cooldown policy

### Risks

- tree node snapshot structs are currently panel-local and may need shared placement
- rendering code still depends on selection knowledge for auto-opening ancestors

### Non-Goals

- changing table columns or sort semantics
- converting filesystem traversal away from current Win32-backed approach

### Exit Criteria

- panel rendering reads from a tree model instead of owning traversal and caches
- keyboard navigation depends on model output, not panel-built caches

### Session Handoff Checklist

- note whether ancestor auto-open logic belongs in the model or in the panel
- record any snapshot fields that remain coupled to ImGui-only concerns

---

## Phase 5 - Extract `PreviewLoadCoordinator`

### Problem

`MediaPreviewPanel` currently owns latest-path mailbox state, decode cancellation, async decode launch, pending upload handoff, audio prepare launch, and load/autoplay policy. That is a workflow layer, not a rendering concern.

### Responsibilities To Move

- latest requested path mailbox
- decode task lifecycle and cancellation
- pending decoded media handoff
- audio prepare task lifecycle and handoff
- load policy around force-load and play-on-upload

### Proposed Interface

The panel should request previews and poll a coordinator each frame for completed work.

Illustrative shape:

```cpp
class PreviewLoadCoordinator
{
public:
    struct DecodedMedia;
    struct CompletedLoad;

    ~PreviewLoadCoordinator( );
    void NotifyDownloaded( const std::string& filepath );
    void RequestPreview( const std::string& filepath, bool forceLoad, bool playOnUpload );
    void Clear( );
    void Update( );
    std::unique_ptr<DecodedMedia> ConsumeDecoded( );
    std::unique_ptr<MediaAudioPlayer> ConsumePreparedAudio( );
    bool IsDecoding( ) const;
};
```

### Files Likely To Change

- Modify: `ImageScraper/include/ui/MediaPreviewPanel.h`
- Modify: `ImageScraper/src/ui/MediaPreviewPanel.cpp`
- Create: `ImageScraper/include/ui/PreviewLoadCoordinator.h`
- Create: `ImageScraper/src/ui/PreviewLoadCoordinator.cpp`
- Modify: `ImageScraper/ImageScraper.vcxproj`
- Modify: `ImageScraper/ImageScraper.vcxproj.filters`
- Add or modify tests in `ImageScraperTests/src/tests/`

### Test Strategy

- add coordinator-focused tests for:
  - new request superseding old request
  - clear cancelling pending work
  - prepared audio ignored when file no longer matches
  - force-load not being dropped while playback is active

### Risks

- current decode code depends on private panel helpers and nested types
- lifetime of `VideoPlayer` and `MediaAudioPlayer` must stay explicit

### Non-Goals

- changing decode formats or supported extensions
- changing autoplay policy

### Exit Criteria

- panel no longer owns async decode and audio prepare plumbing
- `Update()` in `MediaPreviewPanel` becomes mostly state application plus rendering

### Session Handoff Checklist

- note whether decode helper functions should move with the coordinator or to a separate decoder utility
- note whether GIF full-frame decode remains a separate branch or is unified

---

## Phase 6 - Extract `MediaPlaybackController`

### Problem

Playback state is currently spread across play/pause, mute, video clock, video frame advancement, scrubbing, restart, and audio sync methods. That is a state machine hidden inside the panel.

### Responsibilities To Move

- media playback state enum and transitions
- play / pause / stop policy
- mute and volume state
- video playback clock
- scrub lifecycle
- audio sync for video playback

### Proposed Interface

The panel and control panel should talk to a playback owner with a small command/query surface.

Illustrative shape:

```cpp
class MediaPlaybackController
{
public:
    void LoadMedia( /* decoded media state */ );
    void Clear( );
    void Update( float deltaSeconds );

    void TogglePlayPause( );
    void ToggleMute( );
    void SetVolume( float volume );

    bool IsPlaying( ) const;
    bool IsMuted( ) const;
    bool CanPlayPause( ) const;
    bool CanMute( ) const;
    bool CanScrub( ) const;
    float GetProgress( ) const;
    std::string GetProgressLabel( ) const;

    void BeginScrub( );
    void UpdateScrub( float normalized );
    void EndScrub( float normalized );
};
```

### Files Likely To Change

- Modify: `ImageScraper/include/ui/MediaPreviewPanel.h`
- Modify: `ImageScraper/src/ui/MediaPreviewPanel.cpp`
- Possibly modify: `ImageScraper/include/ui/MediaPreviewControlPanel.h`
- Possibly modify: `ImageScraper/src/ui/MediaPreviewControlPanel.cpp`
- Create: `ImageScraper/include/ui/MediaPlaybackController.h`
- Create: `ImageScraper/src/ui/MediaPlaybackController.cpp`
- Modify: `ImageScraper/ImageScraper.vcxproj`
- Modify: `ImageScraper/ImageScraper.vcxproj.filters`
- Add or modify tests in `ImageScraperTests/src/tests/`

### Test Strategy

- add controller-level tests for:
  - GIF play / pause transitions
  - video play / pause transitions
  - scrub pause / resume behaviour
  - mute and volume interactions
  - restart when playback reaches duration

### Risks

- playback depends on texture manager, frame animator, video player, and audio player lifetimes
- moving too much at once could destabilize scrubbing

### Non-Goals

- redesigning the control panel
- changing how the scrub bar feels

### Exit Criteria

- playback state transitions live outside the panel
- control panel remains a caller, not a second owner of playback rules

### Session Handoff Checklist

- note whether metadata and playback state are still mixed
- record any remaining panel-owned playback fields that should be removed next

---

## Phase 7 - Optional Extract `PreviewPresentationModel`

### Problem

Metadata badges, privacy toggle display state, and overlay strings are lower-risk concerns, but they still mix presentation decisions with media state and load state.

### Responsibilities To Move

- metadata badge preparation
- current file display labels
- privacy toggle label / tooltip text
- loading overlay presentation state

### Proposed Interface

Keep this phase optional. Only do it if `MediaPreviewPanel` still feels crowded after phases 5 and 6.

Illustrative shape:

```cpp
class PreviewPresentationModel
{
public:
    void SetCurrentFile( const std::string& filepath, const std::filesystem::path& downloadRoot );
    void Clear( );

    std::string GetFileName( ) const;
    std::vector<std::string> BuildMetadataBadges( int width, int height ) const;
};
```

### Files Likely To Change

- Modify: `ImageScraper/include/ui/MediaPreviewPanel.h`
- Modify: `ImageScraper/src/ui/MediaPreviewPanel.cpp`
- Optionally create: `ImageScraper/include/ui/PreviewPresentationModel.h`
- Optionally create: `ImageScraper/src/ui/PreviewPresentationModel.cpp`
- Modify project files if new source files are added

### Test Strategy

- simple tests for metadata labels and badge generation

### Risks

- limited payoff if earlier phases already reduce the file enough

### Non-Goals

- changing overlay visuals
- changing config storage for privacy or volume

### Exit Criteria

- only pursue if it materially improves readability after core workflow extraction is complete

### Session Handoff Checklist

- record whether this phase is still justified or should be dropped

---

## Cross-Phase Testing Strategy

- keep existing `UiStateTests.cpp` coverage for public static helpers that remain useful
- add new targeted tests when extracted components gain meaningful public behaviour
- avoid overfitting tests to private implementation details
- run the most relevant test target after each completed phase

Suggested focus after each phase:

- delete phases: delete progress and recovery tests
- selection and tree phases: navigation and persistence tests
- preview phases: playback and load coordination tests

## Documentation And Tracking

After each completed phase:

- update this document with what changed
- note any sequence changes
- record known leftovers explicitly instead of relying on session memory

If a phase reveals a better seam than the one described here, prefer the better seam and update the document rather than forcing the original structure.

## Recommended First Session

Start with Phase 1, `DownloadDeleteController`.

Why:

- it already has the clearest workflow boundary
- it is high value but relatively self-contained
- it reduces `DownloadHistoryPanel` size and state density without forcing a large render rewrite
- it creates a pattern for later workflow extractions
