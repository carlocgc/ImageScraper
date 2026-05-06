# Frame-Addressable GIF Playback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` when implementing this plan.

**Goal:** Replace the prototype cached-preview GIF scrub path with a frame-addressable GIF playback path that keeps exact GIF frame data in CPU memory, uses one reusable display texture, disables GIF controls until ready, and leaves video behavior unchanged.

**Architecture:** Split GIF handling into three explicit layers:
- fast first-frame preview for selection responsiveness
- background full-GIF warm-up into CPU-resident frame/timeline data
- `MediaPreviewPanel` integration that uploads exact frames into a single active texture for playback and scrubbing

**Tech Stack:** C++, FFmpeg, ImGui, OpenGL texture upload, existing async/future-based background work

---

## File Scope

### Likely New Files

- `G:\Projects\ImageScraper\ImageScraper\include\ui\GifPlaybackCache.h`
- `G:\Projects\ImageScraper\ImageScraper\src\ui\GifPlaybackCache.cpp`

### Likely Modified Files

- `G:\Projects\ImageScraper\ImageScraper\include\ui\MediaPreviewPanel.h`
- `G:\Projects\ImageScraper\ImageScraper\src\ui\MediaPreviewPanel.cpp`
- `G:\Projects\ImageScraper\ImageScraper\ImageScraper.vcxproj`
- `G:\Projects\ImageScraper\ImageScraper\ImageScraper.vcxproj.filters`

### Candidate Reuse Or Cleanup

- inspect whether any of the prototype `GifScrubCache` helper code is worth reusing for:
  - GIF frame delay normalization
  - cumulative timeline construction
  - exact frame lookup
- remove or retire prototype cached-preview state once the new path is complete

Do not carry the preview-cache subsystem forward by default. Reuse only the parts that fit the new architecture cleanly.

## Task 1: Write the runtime helper interface

**Files:**
- Create: `G:\Projects\ImageScraper\ImageScraper\include\ui\GifPlaybackCache.h`

- [ ] Define a helper focused on exact GIF runtime state, not scrub previews.

It should expose data shapes for:
- frame RGBA data
- frame delay
- cumulative timeline metadata
- exact frame lookup by normalized position
- full-GIF decode result

- [ ] Keep the interface free of:
- OpenGL texture ownership
- panel UI state
- futures, mutexes, or scheduling policy

- [ ] Include an explicit ready/failure result contract so decoder errors are distinguishable from clean EOF.

## Task 2: Implement full-GIF warm-up into CPU memory

**Files:**
- Create: `G:\Projects\ImageScraper\ImageScraper\src\ui\GifPlaybackCache.cpp`
- Modify if needed: `G:\Projects\ImageScraper\ImageScraper\include\ui\GifPlaybackCache.h`

- [ ] Decode the entire GIF sequentially into CPU memory.

The implementation must:
- open the GIF with FFmpeg
- decode frames in order
- preserve valid `10 ms` timing values
- normalize only truly invalid or zero delays
- store RGBA for every frame
- compute cumulative frame start and end times
- compute total duration

- [ ] Provide exact frame lookup helpers.

Required operations:
- normalized scrub position to exact target time
- target time to exact frame index using the real GIF timeline

- [ ] Keep FFmpeg details inside the `.cpp`.

- [ ] Build and verify the helper compiles before integrating panel behavior.

## Task 3: Add helper files to the Visual Studio project

**Files:**
- Modify: `G:\Projects\ImageScraper\ImageScraper\ImageScraper.vcxproj`
- Modify: `G:\Projects\ImageScraper\ImageScraper\ImageScraper.vcxproj.filters`

- [ ] Add the new helper files to the project and filter groups.

- [ ] If the prototype `GifScrubCache` files are being superseded, decide whether they should:
- remain temporarily during migration
- or be removed from the project in the same change

Do not leave both architectures half-active.

## Task 4: Add retained warmed-GIF state to MediaPreviewPanel

**Files:**
- Modify: `G:\Projects\ImageScraper\ImageScraper\include\ui\MediaPreviewPanel.h`

- [ ] Add retained GIF runtime entry state for:
- file identity
- dimensions
- full frame/timeline payload
- warm-up state
- recency metadata

- [ ] Add scheduling state for:
- one active warm-up job
- LIFO pending queue
- retained-entry lookup
- LRU eviction

- [ ] Add control-state helpers for:
- whether current GIF controls should be enabled
- whether current GIF exact frame data is ready

- [ ] Keep OpenGL ownership in the panel and runtime decode data in the helper.

## Task 5: Queue and retain full-GIF warm-up work

**Files:**
- Modify: `G:\Projects\ImageScraper\ImageScraper\src\ui\MediaPreviewPanel.cpp`

- [ ] Start warm-up when a GIF becomes current, but only after the fast first-frame preview has been accepted.

- [ ] Implement the agreed scheduling policy:
- max `1` active warm-up
- pending requests in LIFO order
- no preemption of the active warm-up
- retain up to `3` warmed GIFs by LRU

- [ ] If the user navigates away:
- keep the warm-up if the GIF still belongs to the retained set
- discard results for evicted entries

- [ ] Respect privacy mode in the scheduler.

Required behavior:
- do not start new warm-up work for the current selection while privacy mode is enabled
- if privacy mode is enabled during an active warm-up, stop progressing that work
- on privacy mode disable, if the selected item is still a non-ready GIF, queue warm-up again

Prefer cancel-and-restart semantics unless the existing scheduler makes a clean pause trivial.

- [ ] Ensure selection and navigation do not block on warm-up.

## Task 6: Gate GIF controls on ready state

**Files:**
- Modify: `G:\Projects\ImageScraper\ImageScraper\src\ui\MediaPreviewPanel.cpp`
- Modify if needed: `G:\Projects\ImageScraper\ImageScraper\include\ui\MediaPreviewPanel.h`
- Modify if needed: related media control panel integration points

- [ ] Disable GIF timeline-dependent controls while the current GIF is warming.

This includes at minimum:
- play or pause
- scrub interactions
- any exact timeline actions that require frame-addressable state

- [ ] While warming, render a centered loading overlay on top of the first-frame preview.

Initial loading indicator contract:
- simple ping-pong bar
- centered in the preview
- width approximately `30%` of the current preview region
- visible only while the current GIF is warming

- [ ] Suppress the loading overlay while privacy mode is enabled.

- [ ] Keep non-GIF behavior unchanged.

- [ ] Keep history navigation and item switching responsive while controls are disabled.

- [ ] Make the UI state explicit rather than silently falling back to the old exact-seek behavior.

## Task 7: Replace video-backed GIF scrub and playback with exact frame uploads

**Files:**
- Modify: `G:\Projects\ImageScraper\ImageScraper\src\ui\MediaPreviewPanel.cpp`

- [ ] For warmed GIFs, route playback advancement through retained frame/timeline data rather than `VideoPlayer`.

- [ ] Keep one reusable display texture for the active GIF preview.

- [ ] Implement a helper that uploads a chosen CPU-resident GIF frame into that texture.

- [ ] Use that same mechanism for:
- exact drag updates
- exact scrub release
- playback frame advancement

- [ ] Remove the need for cached preview textures in the GIF path.

This is the core architectural replacement. Do not leave `SeekToTimeExact()` in the warmed GIF scrub path.

## Task 8: Clean out prototype cached-preview state

**Files:**
- Modify: `G:\Projects\ImageScraper\ImageScraper\include\ui\MediaPreviewPanel.h`
- Modify: `G:\Projects\ImageScraper\ImageScraper\src\ui\MediaPreviewPanel.cpp`
- Modify or remove: prototype helper files if no longer needed
- Modify: project files if source membership changes

- [ ] Remove panel state that only exists for cached preview textures and preview-build application.

- [ ] Remove dead branches that still treat exact GIF positioning as a hybrid video-backed operation.

- [ ] Keep any reusable GIF timing utilities only if they fit the new helper cleanly.

- [ ] Re-test cleanup paths to avoid stale texture handles, stale retained entries, or mismatched frame indices.

## Task 9: Verify behavior and profile again

**Files:**
- No new files required unless minor fixes are needed

- [ ] Build `ImageScraper` successfully with the new path.

- [ ] Manually validate:
- GIF first-frame preview remains fast
- GIF controls stay disabled while warming
- the loading overlay appears while warming and disappears when ready
- enabling privacy while warming stops warm-up and hides the loading overlay
- disabling privacy while the same GIF is still selected restarts warm-up
- controls enable once warm-up completes
- drag scrubbing is exact for warmed GIFs
- release lands on the exact frame
- playback resumes correctly after scrub
- navigation across history remains responsive
- warmed GIF retention and eviction behave as expected
- video behavior is unchanged

- [ ] Re-profile the exact release path.

Success criterion:
- warmed GIF release should no longer be dominated by `SeekToTimeExact()` on the UI thread

- [ ] Only add a second-stage optimization if profiling proves direct frame uploads during drag are still too expensive.

That optimization, if needed later, should be framed as an upload optimization on top of frame-addressable GIF state, not a return to the prototype architecture.

## Self-Review Checklist

- [ ] The new design keeps video untouched.
- [ ] The new design keeps first-frame preview cheap.
- [ ] The new design disables GIF controls until exact state is ready.
- [ ] The new design uses one reusable GPU texture rather than one texture per GIF frame.
- [ ] The new design keeps retained warmed GIFs bounded by the agreed `3`-entry LRU policy.
- [ ] The new design removes synchronous FFmpeg exact-seek work from warmed GIF drag and release.
- [ ] The prototype cached-preview path is removed or clearly retired rather than left half-integrated.
