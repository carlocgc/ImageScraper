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
