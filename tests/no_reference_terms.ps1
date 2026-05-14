param(
  [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
  [switch]$AllHistory
)

$ErrorActionPreference = "Stop"
$pattern = "reference\.(com|cn)|https?://[^\s)\]]*reference"

Push-Location $RepoRoot
try {
  if ($AllHistory) {
    $revs = & git rev-list --all
    if ($LASTEXITCODE -ne 0) {
      Write-Error "git rev-list failed while preparing reference URL history scan."
      exit $LASTEXITCODE
    }

    $matches = @()
    foreach ($rev in $revs) {
      $found = & git grep -n -i -E $pattern $rev -- .
      $code = $LASTEXITCODE
      if ($code -eq 0) {
        $matches += $found
        continue
      }
      if ($code -eq 1) {
        continue
      }
      Write-Error "git grep failed while scanning revision $rev for reference URLs."
      exit $code
    }

    if ($matches.Count -eq 0) {
      exit 0
    }

    Write-Error ("Direct reference URL references are not allowed in repository history." +
                 [Environment]::NewLine + ($matches -join [Environment]::NewLine))
    exit 1
  }

  $matches = & git grep -n -i -E $pattern -- .
  $code = $LASTEXITCODE
  if ($code -eq 1) {
    exit 0
  }
  if ($code -ne 0) {
    Write-Error "git grep failed while scanning tracked files for reference URLs."
    exit $code
  }

  Write-Error ("Direct reference URL references are not allowed in tracked files." +
               [Environment]::NewLine + ($matches -join [Environment]::NewLine))
  exit 1
} finally {
  Pop-Location
}
