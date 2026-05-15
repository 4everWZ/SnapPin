param(
  [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
  [switch]$AllHistory
)

$ErrorActionPreference = "Stop"
$forbiddenTerms = @(
  (-join ([char[]](112, 105, 120, 112, 105, 110)))
)

function Add-Matches {
  param(
    [System.Collections.Generic.List[string]]$Target,
    [object[]]$Items
  )

  foreach ($item in $Items) {
    if ($null -ne $item -and "$item" -ne "") {
      $Target.Add("$item")
    }
  }
}

function Test-ForbiddenText {
  param(
    [string]$Text,
    [string[]]$Terms
  )

  foreach ($term in $Terms) {
    if ($Text.IndexOf($term, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
      return $true
    }
  }
  return $false
}

function Scan-CurrentTree {
  param(
    [string[]]$Terms
  )

  $matches = [System.Collections.Generic.List[string]]::new()
  foreach ($term in $Terms) {
    $found = & git grep -n -i -F -- $term -- .
    $code = $LASTEXITCODE
    if ($code -eq 0) {
      Add-Matches -Target $matches -Items $found
    } elseif ($code -ne 1) {
      Write-Error "git grep failed while scanning tracked files for forbidden reference terms."
      exit $code
    }
  }

  $names = & git ls-files
  if ($LASTEXITCODE -ne 0) {
    Write-Error "git ls-files failed while scanning tracked file names for forbidden reference terms."
    exit $LASTEXITCODE
  }
  foreach ($name in $names) {
    if (Test-ForbiddenText -Text $name -Terms $Terms) {
      $matches.Add("file-name:$name")
    }
  }

  return $matches
}

function Scan-History {
  param(
    [string[]]$Terms
  )

  $matches = [System.Collections.Generic.List[string]]::new()
  $revs = & git rev-list --all
  if ($LASTEXITCODE -ne 0) {
    Write-Error "git rev-list failed while preparing forbidden reference term history scan."
    exit $LASTEXITCODE
  }

  foreach ($rev in $revs) {
    foreach ($term in $Terms) {
      $found = & git grep -n -i -F -- $term $rev -- .
      $code = $LASTEXITCODE
      if ($code -eq 0) {
        Add-Matches -Target $matches -Items $found
      } elseif ($code -ne 1) {
        Write-Error "git grep failed while scanning revision $rev for forbidden reference terms."
        exit $code
      }
    }

    $names = & git ls-tree -r --name-only $rev
    if ($LASTEXITCODE -ne 0) {
      Write-Error "git ls-tree failed while scanning revision $rev file names for forbidden reference terms."
      exit $LASTEXITCODE
    }
    foreach ($name in $names) {
      if (Test-ForbiddenText -Text $name -Terms $Terms) {
        $matches.Add("$rev:file-name:$name")
      }
    }
  }

  $messages = & git log --all --format="%H%x09%s%x09%b"
  if ($LASTEXITCODE -ne 0) {
    Write-Error "git log failed while scanning commit messages for forbidden reference terms."
    exit $LASTEXITCODE
  }
  foreach ($line in $messages) {
    if (Test-ForbiddenText -Text $line -Terms $Terms) {
      $matches.Add("commit-message:$line")
    }
  }

  $refs = & git for-each-ref --format="%(refname)"
  if ($LASTEXITCODE -ne 0) {
    Write-Error "git for-each-ref failed while scanning ref names for forbidden reference terms."
    exit $LASTEXITCODE
  }
  foreach ($ref in $refs) {
    if (Test-ForbiddenText -Text $ref -Terms $Terms) {
      $matches.Add("ref-name:$ref")
    }
  }

  return $matches
}

Push-Location $RepoRoot
try {
  if ($AllHistory) {
    $matches = Scan-History -Terms $forbiddenTerms
    if ($matches.Count -eq 0) {
      exit 0
    }

    Write-Error ("Forbidden reference terms are not allowed in repository history." +
                 [Environment]::NewLine + ($matches -join [Environment]::NewLine))
    exit 1
  }

  $matches = Scan-CurrentTree -Terms $forbiddenTerms
  if ($matches.Count -eq 0) {
    exit 0
  }

  Write-Error ("Forbidden reference terms are not allowed in tracked files." +
               [Environment]::NewLine + ($matches -join [Environment]::NewLine))
  exit 1
} finally {
  Pop-Location
}
