# ImageScraper

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B17-orange)
![License](https://img.shields.io/badge/license-MIT-green)

A Windows desktop application for downloading media from social platforms. Built with C++17, Dear ImGui, and OpenGL — no Electron, no web stack.

![Screenshot](ImageScraper/data/screenshot.png)

---

## Features

### Media & Playback
- Downloads images, GIFs, videos, and animated content from Reddit, Tumblr, and 4chan
- In-app media preview with animated GIF frame stepping
- Video playback (MP4, WebM) via FFmpeg — decoded and rendered directly to an OpenGL texture
- Thumbnail previews on hover in download history

### Networking
- OAuth2 authentication flow for Reddit (full PKCE redirect flow via embedded HTTP listener)
- Retry decorator with exponential backoff and 429 rate-limit handling
- Configurable download limits, sort scopes, and timeframes per provider
- TLS via libcurl with bundled CA certificate

### UI
- Dockable panel layout built with Dear ImGui
- Live download progress bars (per-file and total)
- Searchable, scrollable download history with timestamps and source URLs
- Sign-in/sign-out with authenticated username badge

### Architecture
- Abstract `IHttpClient` interface with a `CurlHttpClient` implementation and a `RetryHttpClient` decorator — HTTP logic is centralised and mockable
- `ThreadPool` with per-context FIFO queues and a main-thread dispatch queue for UI callbacks
- `IUiPanel` / `IProviderPanel` interface hierarchy — adding a new platform means adding a panel class, not editing core files
- `IServiceSink` decouples service callbacks from the UI layer
- Unit test suite (Catch2) covering request parsing, URL utilities, thread pool lifecycle, and HTTP response handling

---

## Supported Platforms

| Platform | Auth | Media Types |
|---|---|---|
| Reddit | OAuth2 | Images, GIFs, Videos, Galleries |
| Tumblr | API Key | Images, Videos |
| 4chan | None required | Images, GIFs |
| Discord | OAuth2 (in progress) | — |

---

## Getting Started

### Prerequisites

- **Visual Studio 2022** with the **Desktop development with C++** workload
- **Windows 10 SDK** (10.0)

All dependencies (libcurl, GLFW, curlpp, FFmpeg, Dear ImGui, nlohmann/json, stb_image, cppcodec) are vendored in `ImageScraper/include/` and `ImageScraper/lib/` — no separate installs required.

### Building

1. Clone the repository
2. Open `ImageScraper.sln` in Visual Studio 2022
3. Set the solution platform to **x64**
4. Build → **Build Solution** (`Ctrl+Shift+B`)

The build output lands in `bin/ImageScraper/x64/Debug/` or `.../Release/`. Required DLLs and data files are copied there automatically by a post-build event.

---

## Configuration

Credentials are entered directly in the **Credentials** panel inside the app. Fields are saved automatically to `config.json` next to the executable as you type — no manual file editing required.

### ![Reddit](https://img.shields.io/badge/Reddit-%23FF4500.svg?style=flat&logo=Reddit&logoColor=white)

1. Create a web app at https://www.reddit.com/prefs/apps
2. Set the redirect URI to `http://localhost:8080`
3. Enter the **client ID** and **client secret** in the Credentials panel

### ![Tumblr](https://img.shields.io/badge/Tumblr-%2336465D.svg?style=flat&logo=Tumblr&logoColor=white)

1. Register an app at https://www.tumblr.com/oauth/apps
2. Enter the **OAuth Consumer Key** in the Credentials panel

### 4chan

No setup required — the API is fully public and anonymous.

---

## Development

### Developer Credentials

`ImageScraper/data/config.json` is gitignored. If you populate it with your API credentials, the pre-build event will automatically copy it to the build output directory alongside the exe — so your credentials are available immediately after each build without having to re-enter them in the Credentials panel.

To set up initially:

```
copy ImageScraper\data\config.template.json ImageScraper\data\config.json
```

Then edit `config.json` with your real credentials. The file stays on your machine and is never committed.

### Window Layout

The default docked panel layout is set up programmatically on first run using ImGui's `DockBuilder` API in `FrontEnd::SetupDefaultLayout()`. Windows are docked by name rather than hash, so the layout never goes stale when panels are added or renamed.

The layout is only applied when no saved layout exists (i.e. no `imgui.ini` next to the exe). Once the app has run once, ImGui persists any rearrangements to `imgui.ini` in the output directory and restores them on subsequent launches. That file is gitignored — it is local to each machine.

**To add a new panel to the default layout**, add a `DockBuilderDockWindow("Panel Name", dockNodeId)` call in `FrontEnd::SetupDefaultLayout()` and delete `imgui.ini` from the output directory to force a layout reset on next run.

### Running Tests

The `ImageScraperTests` project runs automatically as a post-build event when building the main project. To run tests in isolation, set `ImageScraperTests` as the startup project and build it directly.

The **Test Adapter for Catch2** (v2.x, by JohnnyHendriks) is an optional Visual Studio extension that integrates the test suite into Test Explorer, enabling per-test run, debug, and results inline. `ImageScraper.runsettings` is pre-configured for the adapter — after installing the extension, select it via **Test → Configure Run Settings → Select Solution Wide runsettings File**.

Catch2 tests are free functions with no C++ class, so the **Class** column in Test Explorer will always be empty. Use **Group By → Traits** in the Test Explorer toolbar to group tests by their `[tag]` instead.

### Updating the CA Certificate

libcurl's TLS verification uses a bundled CA certificate. To update it:

```
curl -o ImageScraper/lib/Debug/curl-ca-bundle.crt https://curl.se/ca/cacert.pem
curl -o ImageScraper/lib/Release/curl-ca-bundle.crt https://curl.se/ca/cacert.pem
```

### Branch Workflow

```
main          ← stable releases only
  └── development  ← integration branch, always buildable
        └── feature/...  ← one branch per feature or fix
```

All work happens on a `feature/` or `fix/` branch off `development`. Open a PR against `development` when ready; merge to `main` for releases.

---

## AI-Assisted Development

This project was developed with [Claude Code](https://claude.ai/code) (Anthropic) as an AI pair programmer throughout. Claude was used for:

- **Architecture design** — designing the `IHttpClient` abstraction, `RetryHttpClient` decorator pattern, `IUiPanel`/`IServiceSink` interface hierarchy, and `ThreadPool` improvements
- **Refactoring** — breaking the monolithic `FrontEnd` class into composable panel classes, extracting testable utility headers from service code
- **Test-driven development** — writing the Catch2 unit test suite covering request parsing, URL utilities, threading, and HTTP response handling
- **Feature implementation** — FFmpeg video playback integration, OAuth2 sign-in/sign-out flow, animated GIF rendering, download history panel
- **Bug diagnosis** — identifying and fixing an `sws_scale` heap corruption caused by unpadded output buffer allocation
- **Planning** — maintaining `PLAN.md` as a living roadmap across phases of development

All code was reviewed and approved by the author before merging. Claude operated on isolated git worktrees, with changes integrated via pull request.

---

## Third-Party Libraries

| Library | Version | License |
|---|---|---|
| [FFmpeg](https://ffmpeg.org) | 6.x | LGPL 2.1+ |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.89.5 | MIT |
| [GLFW](https://github.com/glfw/glfw) | 3.3 | zlib/libpng |
| [libcurl](https://curl.se/libcurl/) | 8.0.1 | curl (MIT-style) |
| [curlpp](https://github.com/jpbarrette/curlpp) | 0.8.1 | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.2 | MIT |
| [stb_image](https://github.com/nothings/stb) | 2.30 | MIT / Public Domain |
| [cppcodec](https://github.com/tplgy/cppcodec) | — | MIT |
| [Catch2](https://github.com/catchorg/Catch2) | 3.14.0 | BSL-1.0 |

Full license texts: [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)

---

## License

This project is licensed under the MIT License — see [LICENSE.txt](LICENSE.txt) for details.
