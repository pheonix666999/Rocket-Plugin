# GitHub Actions

## macOS builds (AU/VST3)

Workflow: `.github/workflows/macos-build.yml`

What it does:
- Builds **both** projects:
  - `The Rocket/`
  - `RocketRemake/`
- Produces both variants:
  - `ROCKET_INTERNAL_UI=OFF` (public)
  - `ROCKET_INTERNAL_UI=ON` (internal/preset designer)
- Uploads zipped AU + VST3 bundles as workflow artifacts.

Notes:
- The workflow expects JUCE sources to exist at `The Rocket/JUCE_SRC/`.
- Code-signing is **ad-hoc** (`codesign -`) only. Proper Developer ID signing/notarization is a separate step.

