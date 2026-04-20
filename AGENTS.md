# ImageScraper Agent Notes

This file is the short version of the repo guidance for coding agents.
See [CLAUDE.md](./CLAUDE.md) for the fuller project instructions.

## Build

Use PowerShell for MSBuild commands.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'G:\Projects\ImageScraper\ImageScraper.sln' /p:Configuration=Debug /p:Platform=x64 /t:ImageScraper /m /nologo
```

Swap `Debug` for `Release` or `/t:ImageScraper` for `/t:ImageScraperTests` as needed.

## Repo Rules

- Do not add new third-party dependencies without explicit approval.
- Do not modify vendored dependency code under `ImageScraper/include/` third-party folders, `ImageScraper/src/imgui/`, or Catch2 sources.
- Never commit real credentials, API keys, or OAuth tokens.
- New `.h` and `.cpp` files must be added to the relevant `.vcxproj`.
- Keep project-authored source files inside named subfolders, not directly under bare `src/` or `include/`.

## Project-Specific Gotchas

- Do not use em dashes in source, comments, test names, or docs. Use a normal hyphen.
- Use the logging macros `InfoLog`, `WarningLog`, `LogError`, and `LogDebug`.
- Include `imgui/imgui_internal.h` when ImGui internal APIs are needed.

## Workflow

- Start each change on its own branch from `development`.
- Do not commit directly to `development` or `master`.
- Keep PRs small and focused.
- Before opening a PR, merge `development` into the feature branch and resolve any conflicts locally.
- Open PRs against `development`.

## Testing

- Add Catch2 tests for new logic when it can be tested without live network calls or a render context.
- Run the relevant build or test target before opening a PR.
