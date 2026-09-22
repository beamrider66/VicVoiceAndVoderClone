$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$siteRepository = 'https://github.com/beamrider66/VVVC-Installer.git'
$checkout = Join-Path $projectRoot 'build\pages-publish'

Push-Location $projectRoot
try {
    python tools/build_web.py
    if ($LASTEXITCODE -ne 0) { throw 'Installer build failed.' }
    if (-not (Test-Path -LiteralPath (Join-Path $checkout '.git'))) {
        git clone $siteRepository $checkout
        if ($LASTEXITCODE -ne 0) { throw 'Could not clone installer repository.' }
    }
    $actualRemote = git -C $checkout remote get-url origin
    if ($LASTEXITCODE -ne 0 -or $actualRemote -ne $siteRepository) {
        throw 'Unexpected installer checkout remote.'
    }
    if (git -C $checkout status --porcelain) { throw 'Installer checkout has uncommitted changes.' }
    git -C $checkout pull --ff-only origin main
    if ($LASTEXITCODE -ne 0) { throw 'Could not update installer checkout.' }
    # Only generated public assets are copied; the source repository stays private.
    Get-ChildItem -LiteralPath (Join-Path $projectRoot 'build\web') -Force |
        Copy-Item -Destination $checkout -Recurse -Force
    git -C $checkout add --all
    git -C $checkout diff --cached --quiet
    if ($LASTEXITCODE -eq 1) {
        git -C $checkout commit -m 'Publish current VVVC browser installer'
        if ($LASTEXITCODE -ne 0) { throw 'Installer commit failed.' }
    } elseif ($LASTEXITCODE -ne 0) { throw 'Could not inspect staged installer files.' }
    git -C $checkout push origin main
    if ($LASTEXITCODE -ne 0) { throw 'Installer push failed.' }
    Write-Host 'Published to GitHub. Pages deployment may take a minute:'
    Write-Host 'https://beamrider66.github.io/VVVC-Installer/'
} finally {
    Pop-Location
}
