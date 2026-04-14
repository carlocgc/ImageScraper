# ImageScraper — Claude Guidelines

## Project Overview

A C++ desktop application that scrapes and downloads images from Reddit, 4chan, Tumblr, and Discord. Built with libcurl, Dear ImGui, GLFW, and OpenGL3.

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

---

## Sensitive Information

Never commit sensitive data to source control. The following must always be kept out of git:

- Real API keys, client IDs, client secrets, or access tokens
- OAuth tokens or refresh tokens of any kind

`ImageScraper/data/config.template.json` is the committed template with placeholder values. `ImageScraper/data/config.json` is gitignored and holds real local credentials.

When adding a new key:
1. Add it with a placeholder value to `config.template.json` and commit
2. Add it with the real value to your local `config.json`

End users create their config by copying the template:
```bash
cp ImageScraper/data/config.template.json ImageScraper/data/config.json
```

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

## Git Workflow

### Branch structure

```
main          ← stable releases only, always tagged
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

Never commit directly to `development` or `main`.

### Opening a pull request

When the work is done and the build passes, push the branch and open a PR against `development`:

```bash
gh pr create --base development --title "..." --body "..."
```

The user reviews the PR diff and merges directly — no formal approval step is required for this solo project. Once merged, delete the remote branch using the "Delete branch" button on the GitHub PR page.

### Releases: `development` → `main`

When `development` is stable enough to ship, open a PR from `development` → `main`. The user reviews and merges using a regular merge commit (not squash) to preserve integration history:

```bash
gh pr merge <number> --merge
```

Immediately after merging, tag `main` with a version:

```bash
git checkout main && git pull
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
