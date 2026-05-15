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
}
"@

$WM_CLOSE = 0x0010
$WM_COMMAND = 0x0111
$WM_KEYDOWN = 0x0100
$VK_ESCAPE = 0x1B
$CaptureCommand = 1000

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

    $classText = [System.Text.StringBuilder]::new(256)
    [void][SnapPinSmokeNative]::GetClassName($hWnd, $classText, $classText.Capacity)
    if ($classText.ToString() -ne $script:SmokeClassName) {
      return $true
    }

    [uint32]$windowProcessId = 0
    [void][SnapPinSmokeNative]::GetWindowThreadProcessId($hWnd, [ref]$windowProcessId)
    if ($windowProcessId -eq $script:SmokeProcessId) {
      $script:SmokeFoundWindow = $hWnd
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

$resolvedAppPath = (Resolve-Path -LiteralPath $AppPath).Path
$process = $null
$mainWindow = [IntPtr]::Zero

try {
  $process = Start-Process -FilePath $resolvedAppPath -PassThru -WindowStyle Hidden

  $mainWindow = Wait-WindowForProcess -ClassName "SnapPinHiddenWindow" -Process $process
  if (-not [SnapPinSmokeNative]::PostMessage($mainWindow, $WM_COMMAND, [IntPtr]$CaptureCommand, [IntPtr]::Zero)) {
    throw "Failed to post capture command."
  }

  $overlayWindow = Wait-WindowForProcess -ClassName "SnapPinOverlay" -Process $process -Visible
  $rect = [SnapPinSmokeNative+RECT]::new()
  if (-not [SnapPinSmokeNative]::GetWindowRect($overlayWindow, [ref]$rect)) {
    throw "Failed to read overlay bounds."
  }

  $width = $rect.Right - $rect.Left
  $height = $rect.Bottom - $rect.Top
  if ($width -le 0 -or $height -le 0) {
    throw "Overlay bounds are invalid: ${width}x${height}."
  }

  if (-not [SnapPinSmokeNative]::PostMessage($overlayWindow, $WM_KEYDOWN, [IntPtr]$VK_ESCAPE, [IntPtr]::Zero)) {
    throw "Failed to post Escape to overlay."
  }

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  do {
    if ($process.HasExited) {
      break
    }
    if (-not [SnapPinSmokeNative]::IsWindowVisible($overlayWindow)) {
      break
    }
    Start-Sleep -Milliseconds 100
  } while ((Get-Date) -lt $deadline)

  if (-not $process.HasExited -and [SnapPinSmokeNative]::IsWindowVisible($overlayWindow)) {
    throw "Overlay remained visible after Escape."
  }
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
}
