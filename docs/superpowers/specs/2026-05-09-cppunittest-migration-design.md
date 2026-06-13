# CppUnitTest Migration Design

**Date:** 2026-05-09

**Status:** Approved in chat, pending file review

**Goal**

Migrate all existing unit tests in `ImageScraperTests` from vendored Catch2 to Visual Studio's built-in native C++ unit test framework so every unit test is discoverable and runnable in Visual Studio Test Explorer without third-party extensions.

**Non-Goals**

- Splitting tests into multiple projects.
- Changing production behavior while porting tests.
- Preserving Catch2 tags, reporters, or command-line UX where they conflict with native Test Explorer discovery.
- Introducing new third-party test dependencies.

## Current State

The repository currently uses a single native C++ test project, `ImageScraperTests`, implemented as a console application that compiles vendored Catch2 source directly into the test binary.

Relevant current files:

- `ImageScraperTests/ImageScraperTests.vcxproj`
- `ImageScraperTests/include/catch2/catch_amalgamated.hpp`
- `ImageScraperTests/src/catch2/catch_amalgamated.cpp`
- `ImageScraperTests/src/tests/*.cpp`
- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`
- `README.MD`

Observed constraints from the current codebase:

- The solution is a native Visual Studio C++ solution, not a .NET test solution.
- CI currently runs the built test executable directly and, in pull request CI, relies on Catch2 JUnit reporter output.
- The README explicitly recommends the third-party Catch2 Test Adapter extension for Visual Studio Test Explorer.
- Repo rules require new `.cpp` and `.h` files to be added to the relevant `.vcxproj`.

## Decision

Use the Microsoft Native Unit Test Framework for C++ (`CppUnitTest`) as the replacement test framework inside the existing `ImageScraperTests` project.

This is the correct fit for the stated goal because:

- It integrates with Visual Studio Test Explorer without external extensions.
- It keeps the test suite native and inside the existing solution model.
- It avoids bridging native C++ tests through a .NET xUnit adapter path that would solve the wrong problem for this repository.

## Framework Integration Model

The replacement framework is intended to be integrated as a Visual Studio-native C++ test framework, not as vendored source code and not through a new repository package-management flow.

Explicitly:

- No Catch2-style vendored framework source should be committed into `ImageScraperTests`.
- No NuGet, vcpkg, Conan, or other package-manager bootstrap should be introduced solely for the unit test framework.
- The `ImageScraperTests` project should be converted to use the Microsoft native C++ unit test framework configuration that Visual Studio and its test platform already understand.
- CI should invoke the Visual Studio-native test platform against the built test container rather than relying on a framework-owned executable command-line surface.

This changes the test framework dependency model from:

- repository-owned framework source compiled into the test binary

to:

- framework support provided by the installed Visual Studio C++ test tooling on developer machines and CI runners

This is a better fit for the goal because the requirement is native Visual Studio Test Explorer discovery, not test-runner independence from the Visual Studio toolchain.

## Alternatives Considered

### 1. Hard cutover to `CppUnitTest` in the existing `ImageScraperTests` project

Recommended.

Pros:

- Delivers built-in Visual Studio discovery.
- Removes vendored Catch2 completely.
- Keeps migration scope narrow by preserving one test project.
- Aligns local Visual Studio workflow with CI.

Cons:

- Requires rewriting all Catch2 test declarations and assertions.
- Requires converting the project configuration from a Catch2 console-executable pattern to a Visual Studio native test-project pattern.

### 2. Transitional mixed-framework state

Rejected.

Pros:

- Reduces short-term pressure by allowing partial migration.

Cons:

- Creates two test idioms in one native project.
- Complicates build settings and developer expectations.
- Increases the chance of stale framework-specific helpers and CI logic surviving the migration.

### 3. Keep Catch2 and rely on a Visual Studio adapter

Rejected.

Pros:

- Lowest code churn.

Cons:

- Fails the explicit requirement to avoid third-party Visual Studio extensions.
- Keeps vendored Catch2 in the tree.

## Target Architecture

### Test project shape

Retain a single `ImageScraperTests` project in the solution.

The project remains responsible for:

- Building native unit tests against selected production sources.
- Referencing the Microsoft native test framework required for Test Explorer discovery.
- Copying any runtime dependencies required by the tests.

The project should stop being conceptually treated as "a console app with a test main" and instead become "the native Visual Studio test container" for the solution.

Operational implication:

- developer machines and CI runners must have the relevant Visual Studio C++ test components installed, because framework support comes from the toolchain rather than vendored repository code

### Test source organization

Keep test sources in `ImageScraperTests/src/tests/`.

Each current Catch2 file should be ported in place where practical, preserving topical grouping:

- `AuthRequestHelpersTests.cpp`
- `BlueskyUtilsTests.cpp`
- `DownloadUtilsTests.cpp`
- `FilesystemUtilsTests.cpp`
- `FourChanUtilsTests.cpp`
- `GifFrameAnimatorTests.cpp`
- `HistoryEntrySorterTests.cpp`
- `JsonFileTests.cpp`
- `LogPanelTests.cpp`
- `MastodonUtilsTests.cpp`
- `OAuthClientTests.cpp`
- `RateLimitedHttpClientTests.cpp`
- `RedditUtilsTests.cpp`
- `RedgifsUtilsTests.cpp`
- `RequestTests.cpp`
- `RequestTypesTests.cpp`
- `RingBufferTests.cpp`
- `SanityTests.cpp`
- `StringUtilsTests.cpp`
- `ThreadPoolTests.cpp`
- `TumblrUtilsTests.cpp`
- `UiStateTests.cpp`
- `UpdateCheckerTests.cpp`

Preserving file-level ownership keeps the migration reviewable and avoids conflating framework migration with suite reorganization.

### Test declaration model

Catch2 free-function tests such as `TEST_CASE(...)` should be converted into `TEST_CLASS(...)` plus `TEST_METHOD(...)` structure.

Target conventions:

- One `TEST_CLASS` per file unless a file naturally needs more than one grouping.
- Method names should be descriptive and stable.
- Where Catch2 test names currently encode behavior in prose, convert those names into readable PascalCase or underscore-separated method names that still scan well in Test Explorer.

Example direction:

- Catch2: `TEST_CASE( "RingBuffer is empty on construction", "[RingBuffer]" )`
- Native test: `TEST_METHOD(RingBuffer_IsEmptyOnConstruction)`

### Assertion model

Catch2 assertions should be mapped to `CppUnitTest` assertions with behavior preserved before any cleanup work.

Expected mapping categories:

- Equality and inequality assertions.
- Boolean assertions.
- Exception assertions.
- Explicit failure assertions.
- String-content checks.

If native assertions produce unreadable failures for repeated patterns, add a small shared helper layer under `ImageScraperTests/include/` or `ImageScraperTests/src/` rather than open-coding awkward conversions in every file.

### Shared test helpers

Introduce shared migration helpers only when they reduce repetition materially.

Likely candidates:

- String conversion helpers for `std::string` and `std::wstring`.
- A helper for substring containment / non-containment assertions.
- Optional wrappers for exception assertions if repeated heavily.

Do not build a custom mini-framework. The migration should lean on the native test framework directly unless friction is repeated and measurable.

## Project File Changes

`ImageScraperTests/ImageScraperTests.vcxproj` will need a framework-level conversion.

Required changes:

- Remove Catch2 include directories from `AdditionalIncludeDirectories`.
- Remove `src/catch2/catch_amalgamated.cpp` from `<ClCompile>`.
- Remove `include/catch2/catch_amalgamated.hpp` from `<ClInclude>`.
- Add the Microsoft native unit test framework configuration required for discovery and execution in Visual Studio.
- Keep all existing production source file inclusions unless they are proven unnecessary.
- Keep runtime dependency copy steps unless the new execution model makes them obsolete.

Potentially affected areas:

- `ConfigurationType`
- framework imports / props / targets
- linker inputs
- include directories
- test runtime output behavior

The exact project XML should be derived from a valid native Visual Studio C++ unit test project pattern, then adapted minimally to match this repository's current layout and linked production sources.

## File Deletions

After the final migrated build passes, remove vendored Catch2 from source control:

- `ImageScraperTests/include/catch2/catch_amalgamated.hpp`
- `ImageScraperTests/src/catch2/catch_amalgamated.cpp`

No partial deletion should happen before all test files are ported and the new framework is verified.

## CI and Workflow Changes

### Pull request / development CI

Current PR CI in `.github/workflows/ci.yml`:

- builds the solution
- runs `ImageScraperTests.exe --reporter junit --out test-results.xml`
- publishes JUnit through `dorny/test-reporter`

Target behavior:

- build the solution
- run tests through the Visual Studio test platform against the native test container
- publish native test results in a format produced by the platform, preferably TRX
- rename workflow labels so they no longer reference Catch2

The key principle is that CI should execute tests the same way Visual Studio discovers them, rather than relying on a framework-specific executable reporter path.

### Release workflow

`.github/workflows/release.yml` currently runs:

- `bin\ImageScraperTests\x64\Release\ImageScraperTests.exe`

This should be updated to use the same native test-platform invocation model as the main CI workflow, adjusted for Release configuration if that remains the desired release gate.

### Documentation

`README.MD` currently documents:

- the Catch2 Test Adapter extension
- empty Class column expectations
- direct test executable invocation

This must be rewritten to describe:

- built-in Visual Studio Test Explorer usage
- any required native test project prerequisites already present in a standard Visual Studio install
- the new command-line or CI invocation model

If `ImageScraper.runsettings` is only relevant because of the Catch2 adapter workflow, its purpose should be reevaluated and either updated or removed.

## Migration Strategy

Use a hard cutover on one branch with small reviewable commits, not a long-lived mixed-framework state.

Recommended migration order:

1. Convert the project structure so a minimal native test is discoverable.
2. Port a small sentinel file such as `SanityTests.cpp` to prove the framework wiring.
3. Port the remaining test files in focused batches.
4. Update CI and release workflows to the new runner model.
5. Update README and any runsettings guidance.
6. Remove vendored Catch2 files.
7. Run full verification locally and in CI.

This sequencing reduces the chance of finishing a broad syntax rewrite only to discover the project is still not recognized by Test Explorer.

## Risks

### Project conversion risk

The highest-risk part is the `.vcxproj` conversion, not the individual test bodies. Native Visual Studio C++ unit test projects require specific framework wiring, and small XML mistakes can lead to tests compiling but not appearing in Test Explorer.

### Assertion ergonomics risk

`CppUnitTest` is less ergonomic than Catch2 for some string and exception-heavy assertions. Without restraint, the migration could drift into custom assertion glue. That should be resisted unless repeated pain shows a small helper is justified.

### Name and discoverability drift

Catch2 tags currently provide lightweight grouping. Native Test Explorer will surface class and method names instead. Poorly chosen method names will make the suite harder to navigate than it is today.

### CI parity risk

If CI continues to run tests differently from Visual Studio, the migration will only partially solve the problem. The runner change is part of the requirement, not an optional cleanup.

## Verification Criteria

The migration is complete only when all of the following are true:

- Visual Studio Test Explorer discovers all unit tests without third-party extensions.
- Tests can be run from Test Explorer successfully.
- `ImageScraperTests` builds in Debug and Release for x64.
- CI runs only the native-framework tests and no longer depends on Catch2 reporters.
- Release workflow test execution uses the same framework model.
- README no longer instructs users to install the Catch2 adapter.
- No vendored Catch2 source remains in `ImageScraperTests`.

## Open Implementation Notes

These are implementation questions to answer in the plan, not unresolved design decisions:

- The precise `.vcxproj` XML changes required for a valid native test project in this repository.
- The exact command-line runner to use in GitHub Actions for TRX-producing native test execution.
- The exact Visual Studio test workload/components required on local machines and GitHub Actions runners.
- Whether `ImageScraper.runsettings` remains necessary after the migration.
- Whether a small shared assertion helper header is justified after porting the first few files.

## Out of Scope Guardrails

The migration should not:

- reorganize production source files
- split the test suite into multiple projects
- change application runtime behavior
- introduce package managers or external dependency bootstrap steps just to support tests

## Summary

The recommended design is a hard cutover from vendored Catch2 to Visual Studio's native `CppUnitTest` framework inside the existing `ImageScraperTests` project, followed by CI and documentation updates so local developer workflow and automated validation both use the same built-in Visual Studio test infrastructure.
