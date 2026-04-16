# ImageScraper — Claude Guidelines

## Project Overview

A C++ desktop application that scrapes and downloads images from Reddit, 4chan, Tumblr, and Discord. Built with libcurl, Dear ImGui, GLFW, and OpenGL3.

---

## Build

**Build command** (always use PowerShell, never bash for MSBuild):
```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'G:\Projects\ImageScraper\ImageScraper.sln' /p:Configuration=Debug /p:Platform=x64 /t:ImageScraper /m /nologo
```
Replace `Debug` with `Release` or `/t:ImageScraper` with `/t:ImageScraperTests` as needed. The solution file is always at `G:\Projects\ImageScraper\ImageScraper.sln`.

**The main repo is at `G:\Projects\ImageScraper`.** Feature branches are worked on directly in this repo or via `git worktree add`. Worktrees go under `G:\Projects\ImageScraper\.claude\worktrees\<name>`.

**After a PR is merged**, run the following to clean up:
```powershell
git -C 'G:\Projects\ImageScraper' checkout development
git -C 'G:\Projects\ImageScraper' pull
git -C 'G:\Projects\ImageScraper' branch -D feature/branch-name
git -C 'G:\Projects\ImageScraper' remote prune origin
# If a worktree was used:
git -C 'G:\Projects\ImageScraper' worktree remove --force 'G:\Projects\ImageScraper\.claude\worktrees\<name>'
```

---

## General Rules

**Never use em dashes (`—`).** ImGui renders the em dash as `?` (it falls outside the default font's Latin-1 range), and the Catch2 test adapter breaks on it in test names. Use a regular hyphen (`-`) everywhere - in source strings, comments, test names, and documentation.

**Logging macros** - use `InfoLog`, `WarningLog`, `LogError`, `LogDebug`. Note: `ErrorLog` and `DebugLog` were renamed during the ImGui v1.92 upgrade because those names collided with new `ImGui::` namespace functions declared in `imgui.h`.

---

## Dependencies

Do NOT add third-party libraries or dependencies without explicit user approval. This includes:

- New header-only libraries
- New `.lib` / `.dll` files
- NuGet packages or vcpkg entries
- Any new `#include` of an external header not already present in `ImageScraper/include/`

If a task would benefit from a new dependency, propose it and wait for approval before proceeding.

Do NOT modify any vendored dependency code without explicit approval. This includes anything under:

- `ImageScraper/include/imgui/`
- `ImageScraper/include/nlohmann/`
- `ImageScraper/include/curl/`
- `ImageScraper/include/GLFW/`
- `ImageScraper/src/imgui/`
- `ImageScraperTests/include/catch2/`
- `ImageScraperTests/src/catch2/`

If a bug or limitation in a dependency needs working around, implement the workaround in project code and flag it to the user.

**ImGui internal API** - include `imgui/imgui_internal.h` (not bare `imgui_internal.h`) when DockBuilder or other internal APIs are needed. `imgui.h` and `imgui_internal.h` both declare names that collide with project macros - check `Logger.h` for the current macro names before including ImGui headers.

**ImGui version**: v1.92.7-docking (vendored). The docking variant is required - do not replace with the non-docking release.

---

## Sensitive Information

Never commit sensitive data to source control. The following must always be kept out of git:

- Real API keys, client IDs, client secrets, or access tokens
- OAuth tokens or refresh tokens of any kind

`ImageScraper/data/config.json` is gitignored and holds real local credentials. In Debug builds, the app creates this file automatically on first run via `JsonFile::Deserialise()`. Release builds do not yet guarantee auto-creation for end users.

When adding a new key:
1. Add handling for it in `JsonFile::Deserialise()` with a sensible default (so Debug auto-creation picks it up)
2. Add the real value via the Credentials panel (or directly in your local `config.json`)

---

## Project Files

Whenever a new `.h` or `.cpp` file is created, it must be added to `ImageScraper\ImageScraper.vcxproj`:

- Header files → `<ClInclude Include="..." />`
- Source files → `<ClCompile Include="..." />`

Likewise, any new test files added to `ImageScraperTests` must be added to `ImageScraperTests\ImageScraperTests.vcxproj` as `<ClCompile Include="..." />`.

Files that are only `#include`d will compile correctly but won't appear in Solution Explorer, making them easy to lose track of.

## Source File Organisation

All source and header files must live inside a named subfolder — never directly in a bare `src/` or `include/` root. Group by domain or type (e.g. `include/mocks/`, `src/tests/`, `src/async/`). This applies to both projects. When adding a new file, choose or create the most appropriate subfolder rather than dropping it at the top level.

---

## Testing

### Tests as part of a feature PR

Every feature or fix PR should include Catch2 tests for any new logic that can be tested without a render context or live network call. Pure utility functions, data structures, request parsing, and file I/O are all in scope. UI panel classes and service orchestration that require ImGui or real HTTP are out of scope.

When adding tests:
1. Create a new `*Tests.cpp` file in `ImageScraperTests/src/tests/`
2. Register it in `ImageScraperTests/ImageScraperTests.vcxproj` as `<ClCompile Include="..." />`
3. If the code under test lives in a `.cpp` file not already compiled by the test project, add that source file to the test vcxproj too
4. Run the full build to confirm all assertions pass before raising the PR

### Test naming rules

The **Test Adapter for Catch2** passes test names as command-line arguments to the executable. Certain characters break argument parsing:

- **Do not use em dashes (`—`) in test names.** Use a regular hyphen (`-`) instead.
- Colons (`::`) in test names (e.g. `ClassName::method`) are fine — the adapter handles them.
- **Every test must have at least one tag.**
- **Tags use PascalCase** (e.g. `[RingBuffer]`, `[ThreadPool]`, `[Reddit]`). Never use all-lowercase (`[ringBuffer]`, `[reddit]`) — mixed casing creates duplicate groups in Test Explorer.

Test names follow the pattern: `"Subject does expected thing"`, e.g. `"Push beyond capacity discards the oldest element"`.

---

## Git Workflow

### Branch structure

```
master        ← stable releases only, always tagged
  └── development  ← integration branch, always buildable
        └── feature/...  ← one branch per feature or fix
```

### Starting work

Every piece of work — feature, fix, or chore — gets its own branch off `development`:

```bash
git checkout development
git pull
git checkout -b feature/short-description   # or fix/ or chore/
```

Never commit directly to `development` or `master`.

### Opening a pull request

When the work is done and the build passes, push the branch and open a PR against `development`:

```bash
gh pr create --base development --title "..." --body "..."
```

`gh` is not on the bash shell PATH. Use the full path:

```bash
"/c/Program Files/GitHub CLI/gh.exe" pr create --base development --title "..." --body "..."
```

The user reviews the PR diff and merges directly — no formal approval step is required for this solo project. Once merged, delete the remote branch using the "Delete branch" button on the GitHub PR page.

### Releases: `development` → `master`

When `development` is stable enough to ship, open a PR from `development` → `master`. The user reviews and merges using a regular merge commit (not squash) to preserve integration history:

```bash
gh pr merge <number> --merge
```

Immediately after merging, tag `master` with a version:

```bash
git checkout master && git pull
git tag v0.1.0 -m "v0.1.0"
git push origin v0.1.0
```

### Versioning

Semantic versioning: `MAJOR.MINOR.PATCH`

- `PATCH` — bug fixes, no new features
- `MINOR` — new features, backwards compatible
- `MAJOR` — breaking changes or major milestone

### Merge approval

The user reviews and approves PRs. Claude merges after approval is given.
