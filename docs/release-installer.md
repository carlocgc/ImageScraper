# Release Installer

The release workflow builds a portable zip and an Inno Setup installer.
Release versions are derived from the pushed `vX.X.X` tag.

## Release Artifacts

The workflow uploads:

- `ImageScraper-vX.X.X-x64.zip`
- `ImageScraper-vX.X.X-x64-setup.exe`
- `SHA256SUMS.txt`

The installer is per-user and installs to:

```text
%LOCALAPPDATA%\Programs\ImageScraper
```

The installer intentionally packages only runtime files. It excludes local files such
as `config.json`, `imgui.ini`, debug symbols, and download output.

## Signing Status

The installer is currently unsigned. This is enough to provide a normal Windows setup
experience, Start Menu shortcut, uninstall entry, and predictable install location.
It does not establish publisher trust with Windows Defender or SmartScreen.

## Update Notifications

ImageScraper can check GitHub Releases for the latest stable `vX.X.X` release and
open the release page in the user's default browser. The app intentionally does not
download or run the setup executable itself while the installer remains unsigned.

If Microsoft Defender quarantines a release, submit the exact flagged artifact to
Microsoft as a software developer false positive:

https://www.microsoft.com/wdsi/filesubmission

Record the submission ID in the release issue or PR so follow-up is traceable.
