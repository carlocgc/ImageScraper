# CppUnitTest Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace vendored Catch2 in `ImageScraperTests` with Visual Studio's native `CppUnitTest` framework so all unit tests are discoverable and runnable in Visual Studio Test Explorer without third-party extensions.

**Architecture:** Keep one native C++ test project and convert it from a Catch2 console-runner pattern into a Visual Studio native test container. Port the existing test files in place, introduce only minimal shared assertion helpers where repeated friction justifies them, and switch CI and release validation to the Visual Studio test platform so local and automated execution use the same discovery model.

**Tech Stack:** Visual Studio 2022 MSBuild, Microsoft Native Unit Test Framework for C++ (`CppUnitTest`), GitHub Actions, PowerShell

---

### Task 1: Branch, Baseline, And Native Test Harness Skeleton

**Files:**
- Create: `docs/superpowers/plans/2026-05-09-cppunittest-migration.md`
- Modify: `ImageScraperTests/ImageScraperTests.vcxproj`
- Modify: `ImageScraperTests/src/tests/SanityTests.cpp`
- Test: `ImageScraper.sln`

- [ ] **Step 1: Create the feature branch from `development`**

```bash
git switch development
git switch -c codex/cppunittest-migration
```

- [ ] **Step 2: Capture a failing baseline for the current Catch2-only runner assumption**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo
```

Expected: `ImageScraperTests` builds successfully before conversion.

- [ ] **Step 3: Convert `ImageScraperTests.vcxproj` away from Catch2-owned source**

Replace the Catch2-specific project items with a native test-framework shape. The important XML changes are:

```xml
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <ClCompile>
    <WarningLevel>Level3</WarningLevel>
    <SDLCheck>true</SDLCheck>
    <PreprocessorDefinitions>_DEBUG;_CONSOLE;CURL_STATICLIB;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    <ConformanceMode>true</ConformanceMode>
    <AdditionalIncludeDirectories>$(ProjectDir)include;$(SolutionDir)ImageScraper\include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    <LanguageStandard>stdcpp17</LanguageStandard>
  </ClCompile>
</ItemDefinitionGroup>

<ItemGroup>
  <ClCompile Include="..\ImageScraper\src\async\ThreadPool.cpp" />
  <ClCompile Include="src\tests\SanityTests.cpp" />
</ItemGroup>

<ItemGroup>
  <ClInclude Include="include\mocks\MockHttpClient.h" />
</ItemGroup>
```

And add the native test framework include/library configuration patterned after a valid Visual Studio Native Unit Test Project so `CppUnitTest.h` resolves and Test Explorer can discover the output.

- [ ] **Step 4: Port the sentinel test file to native test syntax**

Update `ImageScraperTests/src/tests/SanityTests.cpp` to a minimal discoverable test:

```cpp
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{
    TEST_CLASS(SanityTests)
    {
    public:
        TEST_METHOD(SanityCheck)
        {
            Assert::IsTrue( true );
        }
    };
}
```

- [ ] **Step 5: Build the converted sentinel project**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo
```

Expected: `ImageScraperTests` builds with `CppUnitTest.h` resolving and no Catch2 compile unit in the project.

- [ ] **Step 6: Verify native test discovery from the Visual Studio test platform**

Run:

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
& $vstest 'bin\ImageScraperTests\x64\Debug\ImageScraperTests.exe' /Platform:x64 /Logger:trx
```

Expected: one passing `SanityCheck` test in the generated TRX.

- [ ] **Step 7: Commit the working native harness skeleton**

```bash
git add ImageScraperTests/ImageScraperTests.vcxproj ImageScraperTests/src/tests/SanityTests.cpp
git commit -m "test: convert ImageScraperTests to CppUnitTest skeleton"
```

### Task 2: Shared Native Test Helpers And Utility Test Port

**Files:**
- Create: `ImageScraperTests/include/helpers/TestStringAssert.h`
- Modify: `ImageScraperTests/ImageScraperTests.vcxproj`
- Modify: `ImageScraperTests/src/tests/RequestTypesTests.cpp`
- Modify: `ImageScraperTests/src/tests/StringUtilsTests.cpp`
- Modify: `ImageScraperTests/src/tests/RingBufferTests.cpp`
- Modify: `ImageScraperTests/src/tests/FilesystemUtilsTests.cpp`
- Test: `ImageScraperTests/src/tests/RequestTypesTests.cpp`

- [ ] **Step 1: Add a minimal shared helper for repeated string assertions**

Create `ImageScraperTests/include/helpers/TestStringAssert.h`:

```cpp
#pragma once

#include "CppUnitTest.h"

#include <string>

namespace ImageScraperTests::Helpers
{
    inline std::wstring ToWide( const std::string& value )
    {
        return std::wstring( value.begin(), value.end() );
    }

    inline void AssertContains( const std::string& haystack, const std::string& needle )
    {
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(
            haystack.find( needle ) != std::string::npos,
            ToWide( "Expected substring not found" ).c_str() );
    }

    inline void AssertDoesNotContain( const std::string& haystack, const std::string& needle )
    {
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(
            haystack.find( needle ) == std::string::npos,
            ToWide( "Unexpected substring found" ).c_str() );
    }
}
```

- [ ] **Step 2: Add the new helper header to `ImageScraperTests.vcxproj`**

Add:

```xml
<ClInclude Include="include\helpers\TestStringAssert.h" />
```

- [ ] **Step 3: Port a simple assertion-heavy file first**

Update `ImageScraperTests/src/tests/RequestTypesTests.cpp` to the native pattern:

```cpp
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{
    TEST_CLASS(RequestTypesTests)
    {
    public:
        TEST_METHOD(ResponseErrorCodefromInt_MapsKnownHttpCodes)
        {
            Assert::AreEqual( ResponseErrorCode::Unauthorized, ResponseErrorCodefromInt( 401 ) );
            Assert::AreEqual( ResponseErrorCode::NotFound, ResponseErrorCodefromInt( 404 ) );
        }
    };
}
```

- [ ] **Step 4: Port the utility files with the same class-per-file pattern**

For each of:

- `StringUtilsTests.cpp`
- `RingBufferTests.cpp`
- `FilesystemUtilsTests.cpp`

apply this shape:

```cpp
#include "CppUnitTest.h"
#include "helpers/TestStringAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{
    TEST_CLASS(StringUtilsTests)
    {
    public:
        TEST_METHOD(UrlEncode_PercentEncodesReservedCharacters)
        {
            Assert::AreEqual( std::string( "%3F%26" ), UrlEncode( "?&" ) );
        }
    };
}
```

- [ ] **Step 5: Build and run only the converted utility subset**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo

$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
& $vstest 'bin\ImageScraperTests\x64\Debug\ImageScraperTests.exe' `
  /Platform:x64 `
  /Tests:SanityTests::SanityCheck,RequestTypesTests::ResponseErrorCodefromInt_MapsKnownHttpCodes `
  /Logger:trx
```

Expected: targeted native tests pass and the helper compiles cleanly.

- [ ] **Step 6: Commit the helper layer and first migration batch**

```bash
git add ImageScraperTests/ImageScraperTests.vcxproj ImageScraperTests/include/helpers/TestStringAssert.h ImageScraperTests/src/tests/RequestTypesTests.cpp ImageScraperTests/src/tests/StringUtilsTests.cpp ImageScraperTests/src/tests/RingBufferTests.cpp ImageScraperTests/src/tests/FilesystemUtilsTests.cpp
git commit -m "test: port utility tests to CppUnitTest"
```

### Task 3: Port Provider Utility And Parser Tests

**Files:**
- Modify: `ImageScraperTests/src/tests/BlueskyUtilsTests.cpp`
- Modify: `ImageScraperTests/src/tests/FourChanUtilsTests.cpp`
- Modify: `ImageScraperTests/src/tests/MastodonUtilsTests.cpp`
- Modify: `ImageScraperTests/src/tests/RedditUtilsTests.cpp`
- Modify: `ImageScraperTests/src/tests/RedgifsUtilsTests.cpp`
- Modify: `ImageScraperTests/src/tests/TumblrUtilsTests.cpp`
- Modify: `ImageScraperTests/src/tests/UpdateCheckerTests.cpp`
- Test: `ImageScraperTests/src/tests/TumblrUtilsTests.cpp`

- [ ] **Step 1: Port each provider/parser file to one native test class per file**

Use this exact structural pattern:

```cpp
#include "CppUnitTest.h"
#include "helpers/TestStringAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImageScraperTests
{
    TEST_CLASS(TumblrUtilsTests)
    {
    public:
        TEST_METHOD(GetMediaUrlsFromResponse_ReturnsEmptyForNoPosts)
        {
            const auto urls = GetMediaUrlsFromResponse( R"({"response":{"posts":[]}})", 10 );
            Assert::IsTrue( urls.empty() );
        }
    };
}
```

- [ ] **Step 2: Replace Catch2 string matchers with explicit helper calls**

For cases that currently read like substring expectations, use:

```cpp
Helpers::AssertContains( actualBody, "grant_type=refresh_token" );
Helpers::AssertDoesNotContain( htmlBody, "video_url" );
```

- [ ] **Step 3: Preserve strict behavior coverage before any naming cleanup**

For version parsing and URL extraction tests, prefer direct boolean and equality assertions:

```cpp
Assert::IsTrue( TryParseSemanticVersion( "v1.2.3", parsed ) );
Assert::AreEqual( 1, parsed.Major );
Assert::IsFalse( TryParseSemanticVersion( "release-1.2.3", parsed ) );
```

- [ ] **Step 4: Build and run the provider/parser batch**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo

$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
& $vstest 'bin\ImageScraperTests\x64\Debug\ImageScraperTests.exe' /Platform:x64 /Logger:trx
```

Expected: converted provider/parser tests are discovered and pass under the native runner.

- [ ] **Step 5: Commit the provider/parser migration batch**

```bash
git add ImageScraperTests/src/tests/BlueskyUtilsTests.cpp ImageScraperTests/src/tests/FourChanUtilsTests.cpp ImageScraperTests/src/tests/MastodonUtilsTests.cpp ImageScraperTests/src/tests/RedditUtilsTests.cpp ImageScraperTests/src/tests/RedgifsUtilsTests.cpp ImageScraperTests/src/tests/TumblrUtilsTests.cpp ImageScraperTests/src/tests/UpdateCheckerTests.cpp
git commit -m "test: port provider utility tests to CppUnitTest"
```

### Task 4: Port Request And Authentication Tests

**Files:**
- Modify: `ImageScraperTests/src/tests/AuthRequestHelpersTests.cpp`
- Modify: `ImageScraperTests/src/tests/OAuthClientTests.cpp`
- Modify: `ImageScraperTests/src/tests/RequestTests.cpp`
- Test: `ImageScraperTests/src/tests/RequestTests.cpp`

- [ ] **Step 1: Convert request-helper tests to explicit native methods**

Use per-behavior methods instead of long Catch2 prose names:

```cpp
TEST_METHOD(BuildQueryString_SingleParamAppendedCorrectly)
{
    const std::string actual = BuildQueryString( "https://example.test", { { "a", "1" } } );
    Helpers::AssertContains( actual, "a=1" );
}
```

- [ ] **Step 2: Port `OAuthClientTests.cpp` without changing the production mock setup**

Keep the same fake client wiring and convert assertions only:

```cpp
TEST_METHOD(FetchAccessToken_ReturnsFailureWhenClientIdMissing)
{
    OAuthClient client( /* existing constructor args */ );
    const auto result = client.FetchAccessToken( "", "secret", "code" );
    Assert::IsFalse( result.Success );
}
```

- [ ] **Step 3: Port `RequestTests.cpp` in place and preserve request-body/header checks**

Use explicit helper assertions for headers and body fragments:

```cpp
TEST_METHOD(AppOnlyAuthRequest_SendsBasicAuthorizationHeader)
{
    AppOnlyAuthRequest request( /* existing args */ );
    const auto result = request.Execute();

    Assert::IsTrue( result.Success );
    Helpers::AssertContains( mockClient.LastAuthorizationHeader, "Basic " );
}
```

- [ ] **Step 4: Build and run the request/auth batch**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo

$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
& $vstest 'bin\ImageScraperTests\x64\Debug\ImageScraperTests.exe' /Platform:x64 /Logger:trx
```

Expected: request and auth coverage remains green under the new framework.

- [ ] **Step 5: Commit the request/auth migration batch**

```bash
git add ImageScraperTests/src/tests/AuthRequestHelpersTests.cpp ImageScraperTests/src/tests/OAuthClientTests.cpp ImageScraperTests/src/tests/RequestTests.cpp
git commit -m "test: port request and auth tests to CppUnitTest"
```

### Task 5: Port Concurrency, UI, IO, And Runtime Behavior Tests

**Files:**
- Modify: `ImageScraperTests/src/tests/DownloadUtilsTests.cpp`
- Modify: `ImageScraperTests/src/tests/GifFrameAnimatorTests.cpp`
- Modify: `ImageScraperTests/src/tests/HistoryEntrySorterTests.cpp`
- Modify: `ImageScraperTests/src/tests/JsonFileTests.cpp`
- Modify: `ImageScraperTests/src/tests/LogPanelTests.cpp`
- Modify: `ImageScraperTests/src/tests/RateLimitedHttpClientTests.cpp`
- Modify: `ImageScraperTests/src/tests/ThreadPoolTests.cpp`
- Modify: `ImageScraperTests/src/tests/UiStateTests.cpp`
- Test: `ImageScraperTests/src/tests/ThreadPoolTests.cpp`

- [ ] **Step 1: Port the pure state-transition tests first**

For `HistoryEntrySorterTests.cpp`, `UiStateTests.cpp`, and `DownloadUtilsTests.cpp`, convert directly to:

```cpp
TEST_METHOD(DownloadDeleteController_FallsBackToEntryCountsWhenTotalBytesUnknown)
{
    DownloadDeleteController controller;
    controller.BeginDelete( 5, std::nullopt );

    Assert::AreEqual( 0.0, controller.GetDeleteProgress(), 0.0001 );
}
```

- [ ] **Step 2: Port IO and parsing files without changing fixtures**

For `JsonFileTests.cpp` and `LogPanelTests.cpp`, keep the existing temporary-file or string-fixture logic and convert only the framework macros and assertions.

- [ ] **Step 3: Port the concurrency-sensitive tests conservatively**

For `ThreadPoolTests.cpp`, `RateLimitedHttpClientTests.cpp`, and `GifFrameAnimatorTests.cpp`, preserve the existing wait/future ordering semantics and convert exception/bool assertions literally:

```cpp
TEST_METHOD(ThreadPool_Submit_FutureResolvesToCorrectReturnValue)
{
    ThreadPool pool;
    pool.Start( 1 );
    auto future = pool.Submit( [] { return 42; } );

    Assert::AreEqual( 42, future.get() );
    pool.Stop();
}
```

- [ ] **Step 4: Build and run the full converted suite in Debug**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo

$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
& $vstest 'bin\ImageScraperTests\x64\Debug\ImageScraperTests.exe' /Platform:x64 /Logger:trx
```

Expected: all migrated test files are discovered and passing under `CppUnitTest`.

- [ ] **Step 5: Commit the runtime-behavior migration batch**

```bash
git add ImageScraperTests/src/tests/DownloadUtilsTests.cpp ImageScraperTests/src/tests/GifFrameAnimatorTests.cpp ImageScraperTests/src/tests/HistoryEntrySorterTests.cpp ImageScraperTests/src/tests/JsonFileTests.cpp ImageScraperTests/src/tests/LogPanelTests.cpp ImageScraperTests/src/tests/RateLimitedHttpClientTests.cpp ImageScraperTests/src/tests/ThreadPoolTests.cpp ImageScraperTests/src/tests/UiStateTests.cpp
git commit -m "test: port runtime behavior tests to CppUnitTest"
```

### Task 6: CI, Release Workflow, Documentation, And Catch2 Removal

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `.github/workflows/release.yml`
- Modify: `README.MD`
- Modify or Delete: `ImageScraper.runsettings`
- Delete: `ImageScraperTests/include/catch2/catch_amalgamated.hpp`
- Delete: `ImageScraperTests/src/catch2/catch_amalgamated.cpp`
- Test: `.github/workflows/ci.yml`

- [ ] **Step 1: Switch CI from Catch2 executable arguments to `vstest.console.exe`**

Update `.github/workflows/ci.yml` so the test step follows this shape:

```yaml
      - name: Run native Visual Studio tests
        shell: pwsh
        run: |
          $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
          $vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
          & $vstest 'bin\ImageScraperTests\x64\Debug\ImageScraperTests.exe' /Platform:x64 /Logger:trx

      - name: Publish test results
        uses: dorny/test-reporter@v1
        if: always()
        with:
          name: Native CppUnitTest Tests
          path: TestResults/*.trx
          reporter: dotnet-trx
```

- [ ] **Step 2: Update the release workflow to use the same native test platform**

Apply the same pattern in `.github/workflows/release.yml`, pointing at the Release build output:

```yaml
      - name: Run tests
        shell: pwsh
        run: |
          $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
          $vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
          & $vstest 'bin\ImageScraperTests\x64\Release\ImageScraperTests.exe' /Platform:x64 /Logger:trx
```

- [ ] **Step 3: Rewrite the README test instructions for built-in Test Explorer**

Replace the Catch2-adapter section with native guidance such as:

```md
The recommended local workflow is Visual Studio Test Explorer with the built-in Microsoft Native Unit Test Framework for C++ support included in the Visual Studio C++ test tooling.

### Command-line test run

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo

$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
& $vstest '.\bin\ImageScraperTests\x64\Debug\ImageScraperTests.exe' /Platform:x64 /Logger:trx
```
```

- [ ] **Step 4: Reevaluate `ImageScraper.runsettings`**

Either delete it if it only exists for the old Catch2 adapter path, or reduce it to settings still used by `vstest.console.exe`. Do not leave adapter-specific settings behind.

- [ ] **Step 5: Remove vendored Catch2 after the suite passes natively**

Delete:

```text
ImageScraperTests/include/catch2/catch_amalgamated.hpp
ImageScraperTests/src/catch2/catch_amalgamated.cpp
```

- [ ] **Step 6: Run full verification in Debug and Release**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo

& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ImageScraper.sln' `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /t:ImageScraperTests `
  /m /nologo

$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$vstest = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe'
& $vstest 'bin\ImageScraperTests\x64\Debug\ImageScraperTests.exe' /Platform:x64 /Logger:trx
& $vstest 'bin\ImageScraperTests\x64\Release\ImageScraperTests.exe' /Platform:x64 /Logger:trx
```

Expected: all tests pass, TRX files are produced, and no Catch2-specific invocation remains.

- [ ] **Step 7: Commit workflow, docs, and cleanup**

```bash
git add .github/workflows/ci.yml .github/workflows/release.yml README.MD ImageScraper.runsettings ImageScraperTests/ImageScraperTests.vcxproj ImageScraperTests/include/helpers/TestStringAssert.h ImageScraperTests/src/tests
git rm ImageScraperTests/include/catch2/catch_amalgamated.hpp ImageScraperTests/src/catch2/catch_amalgamated.cpp
git commit -m "test: finish CppUnitTest migration"
```
