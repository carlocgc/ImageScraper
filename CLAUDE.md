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

---
