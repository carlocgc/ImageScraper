# Provider Expansion Roadmap

## Goal

Document a practical path for adding new providers that fit ImageScraper's current architecture:

- official API first, HTML scraping last
- small, reviewable PRs
- separate planning for authentication, fetching, and downloading
- reuse the existing `Service`, `OAuthClient`, and `DownloadRequest` seams where they actually fit

## Recommended Order

1. Bluesky
2. Mastodon
3. Flickr
4. Wikimedia Commons

This order favors providers that are strong fits for media discovery, have stable public APIs, and do not force the project into brittle browser scraping too early.

## Shared Touch Points

Most provider additions will touch the same first-party areas:

- `ImageScraper/include/services/ServiceOptionTypes.h`
  - add the provider enum entry, display string, and any new input fields
- `ImageScraper/src/app/App.cpp`
  - register the new service
- `ImageScraper/src/ui/DownloadOptionsPanel.cpp`
  - register the provider panel
- `ImageScraper/src/ui/*Panel.cpp` and `ImageScraper/include/ui/*Panel.h`
  - add provider-specific input widgets
- `ImageScraper/include/services/` and `ImageScraper/src/services/`
  - add the provider service
- `ImageScraper/src/requests/<provider>/`
  - add request types for provider API calls
- `ImageScraper/src/utils/`
  - add JSON parsing and media extraction helpers
- `ImageScraper/src/requests/DownloadRequest.cpp`
  - reuse the current downloader first, improve it later in separate cleanup work

## PR Sizing Rules

- Keep one provider per branch.
- Land the provider shell before the provider does real network work.
- Keep authentication, fetch/parsing, and download behavior as distinct work items even if two of them end up in the same PR.
- Prefer public or API-key modes before full sign-in when that is enough for an MVP.
- Add Catch2 tests for pure helpers and parsers in every provider PR that introduces them.

## Bluesky

### Why It Fits

Bluesky is the closest current replacement for "public social feed with media" work. Public feed endpoints are available without sign-in, which makes it a strong first addition.

### Recommended MVP Scope

- input: actor handle or DID
- mode: public author feed only
- media: images first, videos second

### Proposed PR Sequence

| PR | Focus | Notes |
| --- | --- | --- |
| 1 | Provider shell and public-only UI | Add `Bluesky` provider wiring, service skeleton, request folder, and a basic panel for handle/DID input. |
| 2 | Public feed fetch and parsing | Resolve handle to DID if needed, fetch author feed pages, and extract media descriptors. |
| 3 | Download MVP | Download image media, add naming and dedupe rules, and land end-to-end history/progress support. |
| 4 | Authentication spike | Investigate and implement sign-in only if the official desktop auth story is stable enough to justify it. |

### Authentication Tasks

Status: not required for MVP.

- Confirm the current official auth path for a desktop client before adding any credentials UI.
- If Bluesky's browser-based auth flow fits the desktop app cleanly, evaluate reuse of `OAuthClient` and `OAuthServiceHelpers`.
- If Bluesky's recommended path is session creation with app passwords or a non-OAuth flow, keep that provider-specific instead of forcing it through the OAuth abstraction.
- Add signed-in user display only after the auth path is confirmed.

### Fetching Data Tasks

- Add a request to resolve a handle to a DID when the user does not paste a DID directly.
- Fetch `app.bsky.feed.getAuthorFeed` with the media-oriented filter first.
- Paginate by cursor until the requested media limit is reached or the feed is exhausted.
- Extract stable media descriptors from feed items:
  - actor handle
  - post URI or post ID
  - media type
  - direct blob or CDN URL if present
  - fallback thumbnail URL if needed for later preview work
- Skip unsupported embeds such as external cards with clear logs instead of hard-failing the run.

### Downloading Tasks

- Start with image downloads only.
- Use a deterministic file naming scheme based on actor, post ID, and media index.
- Dedupe repeated URLs across reposts and repeated media references.
- Decide whether video downloads are supported in the MVP or deferred to a follow-up once blob URL handling is proven stable.
- Keep gallery or multi-image post support from day one if the feed payload already exposes the images cleanly.

### Main Risks

- Bluesky auth guidance may change faster than the public read endpoints.
- Video/blob URL rules may need more experimentation than images.

## Mastodon

### Why It Fits

Mastodon exposes public timelines, public account statuses, and media attachments through well-documented APIs. The main trade-off is federation: there is no single global server to target.

### Recommended MVP Scope

- input: instance URL plus account handle
- mode: public account statuses only
- media: attachments from statuses on that instance

### Proposed PR Sequence

| PR | Focus | Notes |
| --- | --- | --- |
| 1 | Provider shell and instance-aware UI | Add `Mastodon` wiring plus inputs for instance URL, account handle, and media limit. |
| 2 | Public account fetch and parsing | Resolve account ID, fetch statuses, paginate, and extract `media_attachments`. |
| 3 | Download MVP | Download attachment URLs, handle paging limits, and support common image/video attachment types. |
| 4 | Authentication spike | Investigate per-instance OAuth only if public-only mode proves too limiting. |

### Authentication Tasks

Status: defer for MVP.

- Confirm whether public-only access is enough for the first release.
- If sign-in becomes necessary, plan for per-instance OAuth rather than assuming one global client ID.
- Evaluate whether dynamic client registration is required for a good user experience.
- Keep auth work separate from the public-fetch MVP because the instance registration story is the hard part, not token storage.

### Fetching Data Tasks

- Normalize and validate the instance base URL.
- Resolve the entered account handle to a local account ID on the chosen instance.
- Fetch account statuses from that instance.
- Paginate with Mastodon ID-based parameters until the media limit is reached.
- Parse `media_attachments` into provider-neutral media descriptors.
- Decide how boosts, replies, and duplicate remote attachments are handled:
  - default recommendation: include original statuses, skip boosts in MVP

### Downloading Tasks

- Download the direct media attachment URL, not just preview images.
- Support images first, then validate gifv/video behavior against the current preview pipeline.
- Use a deterministic folder layout such as instance plus account.
- Add duplicate URL filtering because the same remote attachment can appear more than once through federation and boosts.
- Log when an instance disables public preview or returns partial data so the failure mode is understandable.

### Main Risks

- Some instances disable public previews or search features.
- Full OAuth support is materially harder here than on centralized platforms.

## Flickr

### Why It Fits

Flickr is a direct fit for an image downloader. Its API is mature, image-focused, and built around stable media metadata rather than fragile page scraping.

### Recommended MVP Scope

- input: username or NSID
- mode: public photostream only
- media: image downloads only

### Proposed PR Sequence

| PR | Focus | Notes |
| --- | --- | --- |
| 1 | Provider shell and API key credentials | Add `Flickr` wiring plus credentials fields for API key mode and a basic photostream input panel. |
| 2 | Public photo fetch and parsing | Resolve username to user ID if needed, fetch public photos, and extract photo metadata. |
| 3 | Download MVP | Resolve preferred photo sizes and download the chosen original or fallback image URLs. |
| 4 | Optional OAuth follow-up | Add sign-in only if private content, favorites, albums, or other non-public features become a goal. |

### Authentication Tasks

Status: API key only for MVP.

- Add API key validation in the credentials workflow.
- Keep OAuth out of the first vertical slice.
- If OAuth is added later, use it only for clearly scoped user-only features rather than blocking public downloads on sign-in.

### Fetching Data Tasks

- Decide whether the input accepts username, NSID, or both.
- If username is allowed, resolve it to the canonical Flickr user ID first.
- Fetch public photos for that account with pagination.
- Extract stable identifiers needed for download:
  - photo ID
  - title
  - owner
  - original format hints if available
- Decide whether the MVP is photostream-only or includes albums and favorites:
  - default recommendation: photostream-only first

### Downloading Tasks

- Choose a size policy such as `Original > Large > Medium`.
- Fetch photo size data as needed and map it to the final download URL.
- Preserve source filenames when practical, with a safe fallback naming scheme when metadata is sparse.
- Consider writing license and source URL metadata later, but do not block the MVP on sidecar files.

### Main Risks

- The best download URL may require one extra API call per photo.
- Flickr's API terms for commercial use should be kept visible in product and release planning.

## Wikimedia Commons

### Why It Fits

Wikimedia Commons is a strong source for openly licensed media and avoids social-platform auth and scraping problems entirely.

### Recommended MVP Scope

- input: category name or search term
- mode: anonymous only
- media: image files first, other media later

### Proposed PR Sequence

| PR | Focus | Notes |
| --- | --- | --- |
| 1 | Provider shell and anonymous-only UI | Add `Wikimedia` wiring and inputs for category or search-based discovery. |
| 2 | File listing fetch and parsing | Query Commons for file pages, follow pagination tokens, and extract direct file metadata. |
| 3 | Download MVP | Download original file URLs and preserve source filenames where possible. |
| 4 | Metadata polish | Optional follow-up for license/source manifests and better filtering by media type. |

### Authentication Tasks

Status: no authentication planned for MVP.

- Keep the provider anonymous-only.
- Do not add credentials UI unless a later feature explicitly requires authenticated Wikimedia actions.

### Fetching Data Tasks

- Decide whether the first input mode is category-driven, search-driven, or both.
- Query Commons for file results and follow continuation tokens until the media limit is reached.
- Extract stable download metadata:
  - file title
  - direct original URL
  - media type
  - source page URL
- Filter out unsupported file types early so the downloader receives only actionable media.

### Downloading Tasks

- Download original file URLs first.
- Preserve Commons filenames when safe.
- Consider a later sidecar manifest for license and attribution metadata, but do not require that for MVP.
- Decide whether video/audio support is in scope or deferred until after image-only support lands.

### Main Risks

- Commons query shape and pagination will need careful parser tests.
- Search-based input may produce noisier results than category-based input.

## Suggested First Milestone

If only one provider is started next, the best first milestone is:

1. Bluesky PR 1
2. Bluesky PR 2
3. Bluesky PR 3

That sequence gives the project a modern public-social provider without blocking on sign-in work.

## Suggested Follow-Up After The First New Provider

Once the first provider lands, evaluate a small shared cleanup pass before adding the next one:

- extract a reusable media descriptor type for provider fetchers
- extract shared pagination helpers where they are clearly similar
- make provider panel registration less manual if the provider list keeps growing
- revisit `ServiceOptionTypes.h` so provider-specific inputs do not all accumulate in one struct forever
