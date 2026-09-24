$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$siteRepository = 'https://github.com/beamrider66/VicVoiceAndVoderClone.git'
$checkout = Join-Path $projectRoot 'build\project-pages-publish'

Push-Location $projectRoot
try {
    python tools/build_user_guide.py
    if ($LASTEXITCODE -ne 0) { throw 'Guide build failed.' }
    python tools/build_web.py
    if ($LASTEXITCODE -ne 0) { throw 'Installer build failed.' }
    if (-not (Test-Path -LiteralPath (Join-Path $checkout '.git'))) {
        git clone --single-branch --branch gh-pages $siteRepository $checkout
        if ($LASTEXITCODE -ne 0) { throw 'Could not clone installer repository.' }
    }
    $actualRemote = git -C $checkout remote get-url origin
    if ($LASTEXITCODE -ne 0 -or $actualRemote -ne $siteRepository) {
        throw 'Unexpected installer checkout remote.'
    }
    if (git -C $checkout status --porcelain) { throw 'Installer checkout has uncommitted changes.' }
    $actualBranch = git -C $checkout branch --show-current
    if ($LASTEXITCODE -ne 0 -or $actualBranch -ne 'gh-pages') { throw 'Expected gh-pages branch.' }
    git -C $checkout pull --ff-only origin gh-pages
    if ($LASTEXITCODE -ne 0) { throw 'Could not update installer checkout.' }
    # Only generated site assets are copied to the gh-pages branch.
    Get-ChildItem -LiteralPath (Join-Path $projectRoot 'build\web') -Force |
        Copy-Item -Destination $checkout -Recurse -Force
    git -C $checkout add --all
    git -C $checkout diff --cached --quiet
    if ($LASTEXITCODE -eq 1) {
        git -C $checkout commit -m 'Publish current VVVC browser installer'
        if ($LASTEXITCODE -ne 0) { throw 'Installer commit failed.' }
    } elseif ($LASTEXITCODE -ne 0) { throw 'Could not inspect staged installer files.' }
    git -C $checkout push origin gh-pages
    if ($LASTEXITCODE -ne 0) { throw 'Installer push failed.' }
    Write-Host 'Published to GitHub. Pages deployment may take a minute:'
    Write-Host 'https://beamrider66.github.io/VicVoiceAndVoderClone/'
} finally {
    Pop-Location
}
