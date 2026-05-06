# Frame-Addressable GIF Playback Design

## Summary

This design replaces the cached-preview GIF scrub approach with a true frame-addressable GIF playback model.

The prior prototype improved drag scrubbing by avoiding synchronous FFmpeg seek and decode during drag, but exact scrub release still blocked on `VideoPlayer::SeekToTimeExact()` on the UI thread. That kept GIF exact positioning coupled to the video path and produced the new release-time hotspot shown in profiling.

The revised design treats warmed GIFs as native GIF timeline data in memory rather than pseudo-video streams. Video behavior remains unchanged.

## Goals

- Keep current video playback and scrubbing behavior unchanged.
- Preserve fast first-frame preview when selecting a GIF.
- Make exact GIF scrub drag and exact GIF scrub release independent of FFmpeg seek and decode.
- Keep download history navigation responsive.
- Bound memory use with explicit retention limits.
- Use a simple, honest control state: GIF media controls are disabled until exact frame-addressable state is ready.

## Non-Goals

- No changes to video media handling.
- No attempt to preload every selected GIF fully before it is needed.
- No broad media system rewrite outside the GIF path.
- No new third-party dependencies.

## What Failed In The Prototype

Facts established by the profile and prototype behavior:

- Drag scrubbing became faster once GIF drag stopped doing synchronous FFmpeg seek and decode.
- Exact release remained slow because the current implementation still called `SeekToTimeExact()` for GIF final positioning.
- That cost was dominated by child time in FFmpeg decode and seek work, not local UI logic.
- The cached-preview path also increased state complexity because GIFs were still partially treated as video-backed media internally.

Conclusion:

- Cached preview frames are not the right primary architecture if exact GIF release latency matters.
- The root problem is the video-backed GIF model, not just drag-time preview generation.

## Proposed Model

### High-Level Behavior

On GIF selection:

1. Show the first frame quickly using the existing lightweight preview path.
2. Start a background full-GIF decode into CPU-resident frame-addressable state.
3. Disable GIF-specific media controls until that decode is complete.
4. If the user navigates away, keep the decode only if the GIF remains inside the retained recent set.

When GIF warm-up completes:

1. Keep all GIF frames and timeline metadata in CPU memory.
2. Keep one reusable display texture on the GPU.
3. Enable GIF media controls.
4. Use timeline lookup plus texture upload for drag, release, and playback frame changes.

This removes FFmpeg seek and decode from the warm GIF scrub path.

### Warmed GIF State

For each fully warmed GIF, retain:

- file identity, at minimum the file path
- width and height
- ordered frame list
- per-frame RGBA pixels in CPU memory
- per-frame delay in milliseconds
- cumulative frame start and end times
- total animation duration
- current cache state such as warming, ready, failed, or evicted
- recency metadata for LRU retention

The active display path should use one reusable OpenGL texture for the currently displayed frame, rather than one GPU texture per GIF frame.

### Scrub Model

For warmed GIFs:

- drag position maps directly to an exact target time within the GIF timeline
- target time resolves to the exact active frame using real frame delays
- that frame's RGBA buffer is uploaded into the reusable display texture
- release uses the same exact lookup and upload path

This means cached preview frames are no longer required as a primary mechanism.

### Playback Model

For warmed GIFs:

- playback advances frame index by accumulated time against frame delays
- no FFmpeg seek or decode is required once the GIF is ready
- pause, resume, scrub, and frame stepping all operate on the same frame-addressable state

This is the core architectural simplification the prototype did not achieve.

## Selection And Warm-Up Flow

When the current file is a GIF:

1. Selection shows an immediate first-frame preview.
2. The panel creates or refreshes a retained GIF entry for that file.
3. The GIF is queued for background full decode if it is not already ready or warming.
4. GIF media controls remain disabled while that entry is warming.
5. While warming, the preview keeps showing the first frame and overlays a centered loading indicator.
6. The loading indicator should be a simple ping-pong bar sized to roughly `30%` of the preview width.
7. Navigation to other history items stays enabled.
8. If the user returns to a ready retained GIF, controls become available immediately.

The key rule is that only the GIF-specific controls are gated. Selection and navigation must remain responsive.

### Privacy Interaction

Privacy mode needs to interact with GIF warm-up explicitly.

Rules:

- if privacy mode is enabled while a GIF is warming, the current GIF warm-up should stop progressing
- implementation may either pause the warm-up or cancel and discard the in-flight work, but it must not continue warming invisibly behind privacy mode
- while privacy mode is enabled, do not start new GIF warm-up work for the current selection
- if privacy mode is later disabled and the current selection is still a GIF that is not ready, queue warm-up again
- if a GIF was already fully warmed before privacy mode was enabled, keep the retained data subject to the normal LRU policy

The simpler implementation is cancel-on-privacy-on, restart-on-privacy-off. A true resumable pause is acceptable only if it does not add much state complexity.

## Retention Policy

Initial retention policy:

- keep up to `3` fully warmed GIFs in CPU memory
- use LRU eviction
- allow at most `1` background GIF warm-up at a time
- schedule pending warm-ups in LIFO order
- do not preempt the active warm-up

This matches the previously agreed browsing policy:

- the most recently relevant GIFs should warm first
- quick back-and-forth navigation should benefit from retention
- overall UI responsiveness takes priority over parallel decode throughput

## Control State

For GIFs:

- before warm-up completes:
  - show first-frame preview
  - overlay a centered loading indicator on top of the preview
  - disable play, pause, scrub, and other timeline-dependent controls
- after warm-up completes:
  - enable GIF media controls
  - allow exact drag and exact release

While privacy mode is enabled:

- suppress the loading indicator
- keep GIF controls disabled
- do not advance GIF warm-up work for the current selection

This is stricter than the prototype, but it is more honest. The UI should not advertise exact GIF scrub behavior before the exact state exists.

## Upload Model

The default GPU strategy is:

- one reusable display texture per active preview
- CPU-resident frame buffers for retained warmed GIFs
- upload the selected frame into the display texture when the frame changes

Rationale:

- avoids extreme VRAM growth from one texture per GIF frame
- keeps exact frame access available
- trades GPU texture count for predictable upload work

If later profiling shows drag-time uploads are still too expensive for very large GIFs, that should be handled as a follow-up optimization rather than by keeping the current preview-cache architecture.

## Failure Behavior

If full GIF warm-up fails:

- keep first-frame preview behavior
- leave GIF timeline controls disabled for that file
- do not fall back to the old video-backed exact scrub path

This is important. Falling back to the old exact-seek path would reintroduce the profiled UI stall and undermine the design.

## Architecture Changes

### MediaPreviewPanel

`MediaPreviewPanel` remains responsible for:

- current file selection
- media control state
- active display texture ownership
- playback and scrub integration
- retained GIF entry lifecycle

It should stop treating warmed GIF exact positioning as a `VideoPlayer` seek problem.

### New GIF Runtime Helper

Add or repurpose a focused helper for GIF-only runtime data:

- full GIF decode into CPU memory
- frame delay normalization consistent with existing GIF playback expectations
- cumulative timeline construction
- exact frame lookup from normalized timeline position

This helper should not own OpenGL textures or UI state.

### Background Work Ownership

Background full-GIF warm-up should be managed independently from:

- first-frame preview decode
- video playback decode

That separation keeps the architecture understandable:

- quick preview path
- retained exact GIF runtime path
- existing video path

## Data Flow

1. User selects a GIF from history.
2. Existing preview path shows the first frame quickly.
3. A retained GIF runtime entry is created or touched.
4. If needed, a background full-GIF warm-up is queued.
5. When the warm-up completes, CPU-resident frame data and timeline metadata are attached to the retained entry.
6. On the main thread, the panel marks the GIF ready and enables GIF controls.
7. User scrubs or plays the GIF.
8. The panel resolves the exact frame from the retained timeline and uploads that frame into the active display texture.

## Testing Strategy

### Functional Cases

- Select a GIF and verify first-frame preview appears immediately.
- While warm-up is in progress, verify GIF controls are disabled, the loading indicator is visible, and history navigation remains responsive.
- After warm-up, verify play and scrub controls become enabled.
- Enable privacy while a GIF is warming and verify warm-up stops and the loading indicator disappears.
- Disable privacy while the same GIF remains selected and verify warm-up restarts.
- Drag the GIF scrub bar and verify the displayed frame tracks exact timeline position.
- Release at multiple positions and verify the final frame is exact.
- Scrub from paused and confirm the GIF stays paused on the selected frame.
- Scrub from playing and confirm playback resumes from the exact selected frame.
- Navigate quickly across several GIFs and verify up to `3` warmed GIFs are retained.
- Exceed `3` retained GIFs and verify LRU eviction.
- Verify video media behavior is unchanged.

### Performance Cases

- Confirm drag scrubbing no longer shows synchronous FFmpeg seek and decode hotspots for warmed GIFs.
- Confirm exact release no longer shows `SeekToTimeExact()` in the main-thread hot path for warmed GIFs.
- Check whether frequent frame uploads during drag are acceptable on large GIFs.
- Check that selection latency remains acceptable because full warm-up stays off the UI thread.

## Risks

- CPU memory use will increase for retained warmed GIFs.
- Very large GIFs may still create drag-time upload cost even without FFmpeg decode.
- The GIF and video paths will diverge more clearly, which is correct architecturally but requires careful state cleanup.
- Existing assumptions that GIF playback can piggyback on `VideoPlayer` may need to be removed rather than patched around.

## Acceptance Criteria

- Video behavior is unchanged.
- GIF selection still shows a fast first-frame preview.
- GIF controls remain disabled until exact frame-addressable state is ready.
- Warmed GIF drag and release no longer depend on synchronous FFmpeg seek and decode.
- Exact GIF scrub release is timeline-correct.
- Retained warmed GIFs are bounded by the agreed `3`-entry LRU policy.
- Download history navigation remains responsive while GIFs warm in the background.
