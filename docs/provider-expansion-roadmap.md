# Provider Expansion Roadmap

## Goal

Track upcoming provider work now that the app already has live integrations for Reddit, Tumblr, 4chan, and Bluesky.

Guiding principles:

- official API first, HTML scraping last
- small, reviewable PRs
- authentication, fetch/parsing, and download behavior planned separately
- reuse `Service`, `OAuthClient`, `DownloadPipeline`, and request-layer seams where they actually fit

## Shared Touch Points

Most provider additions will touch the same first-party areas:

- `ImageScraper/include/services/ServiceOptionTypes.h`
  - add the provider enum entry, display string, `UserInputOptions` fields, and limits
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
- `ImageScraper/include/utils/` and `ImageScraper/src/utils/`
  - add parser and media-extraction helpers
- `ImageScraperTests/src/tests/`
  - add Catch2 coverage for pure helpers and response parsing

## Recommended Order

1. Redgifs
2. Mastodon
3. Flickr
4. Wikimedia Commons
5. X / Twitter

This order favors providers that either unlock media the app already encounters or have stable public APIs. X / Twitter stays last on purpose because the API access story is expensive, fragile, and likely to rot.

## PR Sizing Rules

- Keep one provider per branch.
- Land the provider shell before the provider does real network work.
- Prefer public or API-key modes before full sign-in when that is enough for an MVP.
- Treat `UserInputOptions` changes as part of the provider slice, not as a separate follow-up.
- Add Catch2 tests for any new parser/helper code in the same PR that introduces it.

## Redgifs

### Why it fits

- Reddit already surfaces Redgifs URLs, so this unlocks media the app currently skips.
- The domain is media-focused, which makes it a better next step than a general social platform.

### Recommended MVP scope

- first decide whether the best slice is:
  - a standalone Redgifs provider panel, or
  - Redgifs URL resolution/download support used by existing providers
- support direct downloadable media URLs before broader discovery/search features

### Main tasks

- confirm the current public API and whether discovery requires auth
- map Redgifs URLs to stable downloadable assets
- decide whether HLS-style handling belongs in the provider or in shared download helpers
- add parser tests for URL resolution and media metadata extraction

### Main risks

- public API access rules may have changed since older third-party references were written
- a helper-only integration may be more valuable than a full provider panel

## Mastodon

### Why it fits

Mastodon exposes public account timelines and `media_attachments` through stable APIs. The main trade-off is federation: there is no single global server to target.

### Recommended MVP scope

- input: instance URL plus account handle
- mode: public account statuses only
- media: attachments from statuses on that instance

### Main tasks

- normalize and validate instance URLs
- resolve the entered account handle to an account ID on the chosen instance
- fetch statuses and paginate with Mastodon ID-based cursors
- parse `media_attachments` into provider-neutral downloads
- decide how boosts and replies are handled in MVP

### Main risks

- some instances disable public preview/search features
- per-instance OAuth is materially harder than public-only mode

## Flickr

### Why it fits

Flickr is still a strong candidate for image-first downloading because its API is mature and built around explicit media metadata instead of page scraping.

### Recommended MVP scope

- input: username or NSID
- mode: public photostream only
- media: image downloads only

### Main tasks

- resolve username to canonical user ID when needed
- fetch public photos with pagination
- map photos to preferred downloadable sizes
- decide whether API-key-only mode is enough for the first slice

## Wikimedia Commons

### Why it fits

Wikimedia Commons avoids social auth entirely and offers openly licensed media through stable APIs.

### Recommended MVP scope

- input: category name or search term
- mode: anonymous only
- media: image files first

### Main tasks

- decide whether category search, free-text search, or both ship first
- query Commons and follow continuation tokens
- extract original file URLs plus enough metadata for sane filenames
- keep license/attribution sidecars as a follow-up, not an MVP blocker

## X / Twitter

### Status

Exploratory only. Do not treat this as the next provider by default.

### Why it is last

- API access costs and terms are a poor fit for a hobby desktop downloader
- auth and rate-limit behavior are likely to dominate the work before media downloading even starts
- long-term maintenance risk is high

### If it is revisited

- start with a feasibility spike, not an implementation branch
- confirm the current API/product terms first
- only move forward if the project owner still wants the maintenance and access-cost burden

## Suggested Follow-Up After The Next Provider

Once another provider lands, evaluate a small shared cleanup pass:

- extract a reusable media descriptor type for provider fetchers
- pull shared pagination helpers together where they are actually similar
- keep provider panel registration from becoming more manual than it needs to be
