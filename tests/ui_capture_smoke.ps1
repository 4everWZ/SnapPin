param(
  [Parameter(Mandatory = $true)]
  [string]$AppPath,
  [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $AppPath)) {
  throw "AppPath does not exist: $AppPath"
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class SnapPinSmokeNative {
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

  [StructLayout(LayoutKind.Sequential)]
  public struct RECT {
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
  }

  [DllImport("user32.dll", SetLastError = true)]
  public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

  [DllImport("user32.dll", SetLastError = true)]
  public static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

  [DllImport("user32.dll", SetLastError = true)]
  public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

  [DllImport("user32.dll", SetLastError = true)]
  public static extern bool IsWindowVisible(IntPtr hWnd);

  [DllImport("user32.dll", SetLastError = true)]
  public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

  [DllImport("user32.dll", SetLastError = true)]
  public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll", SetLastError = true)]
  public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll", SetLastError = true)]
  public static extern bool SetCursorPos(int X, int Y);
}
"@

$WM_CLOSE = 0x0010
$WM_COMMAND = 0x0111
$WM_KEYDOWN = 0x0100
$WM_MOUSEMOVE = 0x0200
$WM_LBUTTONDOWN = 0x0201
$WM_LBUTTONUP = 0x0202
$VK_ESCAPE = 0x1B
$MK_LBUTTON = 0x0001
$CaptureCommand = 1000
$PinCommand = 2003
$MarkCommand = 2004
$MaxCompactToolbarWidth = 320

function Find-WindowByClassForProcess {
  param(
    [string]$ClassName,
    [int]$ProcessId
  )

  $script:SmokeFoundWindow = [IntPtr]::Zero
  $script:SmokeClassName = $ClassName
  $script:SmokeProcessId = [uint32]$ProcessId

  $callback = [SnapPinSmokeNative+EnumWindowsProc]{
    param([IntPtr]$hWnd, [IntPtr]$lParam)

    [uint32]$topProcessId = 0
    [void][SnapPinSmokeNative]::GetWindowThreadProcessId($hWnd, [ref]$topProcessId)
    if ($topProcessId -ne $script:SmokeProcessId) {
      return $true
    }

    $classText = [System.Text.StringBuilder]::new(256)
    [void][SnapPinSmokeNative]::GetClassName($hWnd, $classText, $classText.Capacity)
    if ($classText.ToString() -eq $script:SmokeClassName) {
      $script:SmokeFoundWindow = $hWnd
      return $false
    }

    $childCallback = [SnapPinSmokeNative+EnumWindowsProc]{
      param([IntPtr]$childHwnd, [IntPtr]$childParam)

      [uint32]$childProcessId = 0
      [void][SnapPinSmokeNative]::GetWindowThreadProcessId(
          $childHwnd, [ref]$childProcessId)
      if ($childProcessId -ne $script:SmokeProcessId) {
        return $true
      }

      $childClassText = [System.Text.StringBuilder]::new(256)
      [void][SnapPinSmokeNative]::GetClassName(
          $childHwnd, $childClassText, $childClassText.Capacity)
      if ($childClassText.ToString() -eq $script:SmokeClassName) {
        $script:SmokeFoundWindow = $childHwnd
        return $false
      }
      return $true
    }
    [void][SnapPinSmokeNative]::EnumChildWindows($hWnd, $childCallback, [IntPtr]::Zero)
    if ($script:SmokeFoundWindow -ne [IntPtr]::Zero) {
      return $false
    }
    return $true
  }

  [void][SnapPinSmokeNative]::EnumWindows($callback, [IntPtr]::Zero)
  return $script:SmokeFoundWindow
}

function Wait-WindowForProcess {
  param(
    [string]$ClassName,
    [System.Diagnostics.Process]$Process,
    [switch]$Visible
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  do {
    if ($Process.HasExited) {
      throw "Process exited before $ClassName appeared."
    }

    $hwnd = Find-WindowByClassForProcess -ClassName $ClassName -ProcessId $Process.Id
    if ($hwnd -ne [IntPtr]::Zero) {
      if (-not $Visible -or [SnapPinSmokeNative]::IsWindowVisible($hwnd)) {
        return $hwnd
      }
    }
    Start-Sleep -Milliseconds 100
  } while ((Get-Date) -lt $deadline)

  throw "Timed out waiting for $ClassName."
}

function New-LParam {
  param(
    [int]$X,
    [int]$Y
  )

  $packed = (($Y -band 0xffff) -shl 16) -bor ($X -band 0xffff)
  return [IntPtr]$packed
}

function Read-WindowRect {
  param([IntPtr]$Window)

  $rect = [SnapPinSmokeNative+RECT]::new()
  if (-not [SnapPinSmokeNative]::GetWindowRect($Window, [ref]$rect)) {
    throw "Failed to read window bounds."
  }
  return $rect
}

function Assert-PositiveBounds {
  param(
    [SnapPinSmokeNative+RECT]$Rect,
    [string]$Name
  )

  $width = $Rect.Right - $Rect.Left
  $height = $Rect.Bottom - $Rect.Top
  if ($width -le 0 -or $height -le 0) {
    throw "$Name bounds are invalid: ${width}x${height}."
  }
}

function Initialize-SmokeConfig {
  param([string]$ExeDir)

  $portableFlag = Join-Path $ExeDir "portable.flag"
  $dataDir = Join-Path $ExeDir "SnapPinData"
  $markerPath = Join-Path $dataDir ".ui-smoke-owned"
  $createdPortableFlag = -not (Test-Path -LiteralPath $portableFlag)
  $dataDirExists = Test-Path -LiteralPath $dataDir

  if ($dataDirExists -and -not (Test-Path -LiteralPath $markerPath)) {
    throw "Refusing to use existing portable data directory without smoke marker: $dataDir"
  }

  if ($createdPortableFlag) {
    [void](New-Item -ItemType File -Path $portableFlag -Force)
  }

  $configDir = Join-Path $dataDir "config"
  [void](New-Item -ItemType Directory -Path $configDir -Force)
  [void](New-Item -ItemType File -Path $markerPath -Force)

  $configPath = Join-Path $configDir "config.json"
  $configJson = @'
{
  "hotkeys": {
    "enabled": false
  },
  "capture": {
    "auto_copy_to_clipboard": false,
    "auto_show_toolbar": true
  },
  "export": {
    "open_folder_after_save": false
  }
}
'@
  Set-Content -LiteralPath $configPath -Value $configJson -Encoding UTF8 -NoNewline

  return @{
    PortableFlag = $portableFlag
    DataDir = $dataDir
    MarkerPath = $markerPath
    CreatedPortableFlag = $createdPortableFlag
  }
}

function Remove-SmokeConfig {
  param([hashtable]$ConfigState)

  if (-not $ConfigState) {
    return
  }

  $dataDir = [string]$ConfigState.DataDir
  $markerPath = [string]$ConfigState.MarkerPath
  if ((Test-Path -LiteralPath $markerPath) -and (Test-Path -LiteralPath $dataDir)) {
    $resolvedDataDir = (Resolve-Path -LiteralPath $dataDir).Path
    $resolvedExeDir = (Resolve-Path -LiteralPath (Split-Path -Path $dataDir -Parent)).Path
    $prefix = $resolvedExeDir.TrimEnd('\') + '\'
    if (-not $resolvedDataDir.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
      throw "Refusing to remove smoke data outside executable directory: $resolvedDataDir"
    }
    Remove-Item -LiteralPath $resolvedDataDir -Recurse -Force
  }

  if ([bool]$ConfigState.CreatedPortableFlag -and
      (Test-Path -LiteralPath ([string]$ConfigState.PortableFlag))) {
    Remove-Item -LiteralPath ([string]$ConfigState.PortableFlag) -Force
  }
}

function Invoke-CaptureDragSelection {
  param(
    [System.Diagnostics.Process]$Process,
    [IntPtr]$MainWindow
  )

  if (-not [SnapPinSmokeNative]::PostMessage($MainWindow, $WM_COMMAND, [IntPtr]$CaptureCommand, [IntPtr]::Zero)) {
    throw "Failed to post capture command."
  }

  $overlayWindow = Wait-WindowForProcess -ClassName "SnapPinOverlay" -Process $Process -Visible
  $overlayRect = Read-WindowRect -Window $overlayWindow
  Assert-PositiveBounds -Rect $overlayRect -Name "Overlay"

  $overlayWidth = $overlayRect.Right - $overlayRect.Left
  $overlayHeight = $overlayRect.Bottom - $overlayRect.Top
  $startClientX = [Math]::Min(80, [Math]::Max(10, [int]($overlayWidth / 4)))
  $startClientY = [Math]::Min(80, [Math]::Max(10, [int]($overlayHeight / 4)))
  $endClientX = [Math]::Min($overlayWidth - 20, $startClientX + 160)
  $endClientY = [Math]::Min($overlayHeight - 20, $startClientY + 120)
  if ($endClientX -le $startClientX -or $endClientY -le $startClientY) {
    throw "Overlay is too small for drag selection."
  }

  [void][SnapPinSmokeNative]::SetCursorPos($overlayRect.Left + $startClientX,
                                           $overlayRect.Top + $startClientY)
  [void][SnapPinSmokeNative]::SendMessage(
      $overlayWindow, $WM_LBUTTONDOWN, [IntPtr]$MK_LBUTTON,
      (New-LParam -X $startClientX -Y $startClientY))
  [void][SnapPinSmokeNative]::SetCursorPos($overlayRect.Left + $endClientX,
                                           $overlayRect.Top + $endClientY)
  [void][SnapPinSmokeNative]::SendMessage(
      $overlayWindow, $WM_MOUSEMOVE, [IntPtr]$MK_LBUTTON,
      (New-LParam -X $endClientX -Y $endClientY))
  [void][SnapPinSmokeNative]::SendMessage(
      $overlayWindow, $WM_LBUTTONUP, [IntPtr]::Zero,
      (New-LParam -X $endClientX -Y $endClientY))

  $toolbarWindow = Wait-WindowForProcess -ClassName "SnapPinToolbar" -Process $Process -Visible
  $toolbarRect = Read-WindowRect -Window $toolbarWindow
  Assert-PositiveBounds -Rect $toolbarRect -Name "Toolbar"
  $toolbarWidth = $toolbarRect.Right - $toolbarRect.Left
  if ($toolbarWidth -gt $MaxCompactToolbarWidth) {
    throw "Toolbar is wider than the compact budget: $toolbarWidth."
  }

  return @{
    OverlayWindow = $overlayWindow
    ToolbarWindow = $toolbarWindow
  }
}

function Wait-WindowsHidden {
  param(
    [System.Diagnostics.Process]$Process,
    [IntPtr[]]$Windows,
    [string]$Name
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  do {
    if ($Process.HasExited) {
      break
    }

    $anyVisible = $false
    foreach ($window in $Windows) {
      if ($window -ne [IntPtr]::Zero -and [SnapPinSmokeNative]::IsWindowVisible($window)) {
        $anyVisible = $true
        break
      }
    }
    if (-not $anyVisible) {
      return
    }
    Start-Sleep -Milliseconds 100
  } while ((Get-Date) -lt $deadline)

  foreach ($window in $Windows) {
    if ($window -ne [IntPtr]::Zero -and [SnapPinSmokeNative]::IsWindowVisible($window)) {
      throw "$Name remained visible."
    }
  }
}

$resolvedAppPath = (Resolve-Path -LiteralPath $AppPath).Path
$appDir = Split-Path -Path $resolvedAppPath -Parent
$process = $null
$mainWindow = [IntPtr]::Zero
$smokeConfig = $null

try {
  $smokeConfig = Initialize-SmokeConfig -ExeDir $appDir
  $process = Start-Process -FilePath $resolvedAppPath -PassThru -WindowStyle Hidden

  $mainWindow = Wait-WindowForProcess -ClassName "SnapPinHiddenWindow" -Process $process

  $pinFlow = Invoke-CaptureDragSelection -Process $process -MainWindow $mainWindow
  [void][SnapPinSmokeNative]::SendMessage(
      $pinFlow.ToolbarWindow, $WM_COMMAND, [IntPtr]$PinCommand, [IntPtr]::Zero)
  $pinWindow = Wait-WindowForProcess -ClassName "SnapPinPinWindow" -Process $process -Visible
  $pinRect = Read-WindowRect -Window $pinWindow
  Assert-PositiveBounds -Rect $pinRect -Name "Pin"

  Wait-WindowsHidden -Process $process `
                     -Windows @($pinFlow.ToolbarWindow, $pinFlow.OverlayWindow) `
                     -Name "Capture artifact UI after pin creation"

  if (-not [SnapPinSmokeNative]::PostMessage($pinWindow, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)) {
    throw "Failed to post close to pin window."
  }
  Wait-WindowsHidden -Process $process -Windows @($pinWindow) -Name "Pin window after close"

  $markFlow = Invoke-CaptureDragSelection -Process $process -MainWindow $mainWindow
  [void][SnapPinSmokeNative]::SendMessage(
      $markFlow.ToolbarWindow, $WM_COMMAND, [IntPtr]$MarkCommand, [IntPtr]::Zero)
  $annotateWindow =
      Wait-WindowForProcess -ClassName "SnapPinAnnotateWindow" -Process $process -Visible
  $annotateRect = Read-WindowRect -Window $annotateWindow
  Assert-PositiveBounds -Rect $annotateRect -Name "Annotate"

  if (-not [SnapPinSmokeNative]::IsWindowVisible($markFlow.OverlayWindow)) {
    throw "Overlay should remain visible behind active mark session."
  }
  Wait-WindowsHidden -Process $process `
                     -Windows @($markFlow.ToolbarWindow) `
                     -Name "Toolbar after mark open"

  if (-not [SnapPinSmokeNative]::PostMessage($annotateWindow, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)) {
    throw "Failed to post close to annotate window."
  }
  Wait-WindowsHidden -Process $process `
                     -Windows @($annotateWindow, $markFlow.OverlayWindow) `
                     -Name "Mark artifact UI after close"
} finally {
  if ($mainWindow -ne [IntPtr]::Zero) {
    [void][SnapPinSmokeNative]::PostMessage($mainWindow, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
  }
  if ($null -ne $process -and -not $process.HasExited) {
    if (-not $process.WaitForExit(3000)) {
      Stop-Process -Id $process.Id -Force
      $process.WaitForExit()
    }
  }
  Remove-SmokeConfig -ConfigState $smokeConfig
}
