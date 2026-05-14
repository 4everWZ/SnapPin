param(
  [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"
$pattern = "reference\.(com|cn)|https?://[^\s)\]]*reference"

Push-Location $RepoRoot
try {
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
