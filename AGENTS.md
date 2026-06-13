# ImageScraper Agent Notes

## Build

Use PowerShell for MSBuild.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'ImageScraper.sln' /p:Configuration=Debug /p:Platform=x64 /t:ImageScraper /m /nologo
```

Swap `Debug` for `Release` or `/t:ImageScraper` for `/t:ImageScraperTests` as needed. PRs target `development`; releases merge `development` into `master`.

## Release

- Merge `development` into `master`.
- Tag `master` with the release version in `vX.X.X` format.
- Push the `master` update and the tag. The `Release` GitHub Actions workflow runs on the version tag and regenerates `ImageScraper/include/version/Version.h` from that tag.

## Repo-Specific Rules

- Do not edit vendored code under `ImageScraper/include/imgui/`, `ImageScraper/include/nlohmann/`, `ImageScraper/include/curl/`, `ImageScraper/include/GLFW/`, or `ImageScraper/src/imgui/`.
- New `.h` and `.cpp` files must be added to the relevant `.vcxproj`.
- Keep project-authored source files inside named subfolders, not directly under bare `src/` or `include/`.
- `ImageScraper/data/config.json` is gitignored local credential data. If you add a new config key, add its default handling to `JsonFile::Deserialise()`.
- Vendored ImGui is `v1.92.7-docking`. Do not swap it for a non-docking build.

## Gotchas

- Never use em dashes. Use `-`.
- Repo-authored text files use LF. Keep `.bat`, `.cmd`, and `.ps1` on CRLF.
- Use `InfoLog`, `WarningLog`, `LogError`, and `LogDebug`.
- When using ImGui internals, include `imgui/imgui_internal.h`.

## Testing

- New native C++ unit test files belong in `ImageScraperTests/src/tests/` and must be added to `ImageScraperTests.vcxproj`.
- Tests should use `CppUnitTest.h`, one `TEST_CLASS` per file by default, and descriptive `TEST_METHOD` names that read well in Visual Studio Test Explorer.
- Test names may use `::` inside assertion messages or comments, but never em dashes.
