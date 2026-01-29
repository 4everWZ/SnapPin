#include "ActionDispatcher.h"
#include "ActionRegistry.h"
#include "ArtifactStore.h"
#include "ConfigService.h"
#include "CaptureService.h"
#include "ExportService.h"
#include "KeybindingsService.h"
#include "SingleInstance.h"
#include "TrayIcon.h"
#include "OverlayWindow.h"
#include "ToolbarWindow.h"
#include "StatsService.h"
#include "SettingsWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <psapi.h>

#include <cstdio>
#include <memory>

namespace {
const wchar_t kMainWindowClass[] = L"SnapPinHiddenWindow";
const UINT kTrayCallbackMessage = WM_USER + 1;
const UINT kTrayIconId = 1;

UINT g_taskbar_created_msg = 0;
snappin::TrayIcon g_tray;

std::unique_ptr<snappin::ActionRegistry> g_action_registry;
std::unique_ptr<snappin::ActionDispatcher> g_action_dispatcher;
std::unique_ptr<snappin::ConfigService> g_config_service;
std::unique_ptr<snappin::KeybindingsService> g_keybindings_service;
std::unique_ptr<snappin::ICaptureService> g_capture_service;
std::unique_ptr<snappin::ArtifactStore> g_artifact_store;
std::unique_ptr<snappin::ExportService> g_export_service;
snappin::RuntimeState g_runtime_state;
std::unique_ptr<snappin::OverlayWindow> g_overlay;
std::unique_ptr<snappin::ToolbarWindow> g_toolbar;
std::unique_ptr<snappin::StatsService> g_stats;
std::unique_ptr<snappin::SettingsWindow> g_settings;

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == g_taskbar_created_msg) {
    g_tray.OnTaskbarCreated();
    return 0;
  }

  switch (msg) {
    case WM_COMMAND: {
      switch (LOWORD(wparam)) {
        case snappin::kTrayMenuCaptureId: {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "capture.start";
            g_action_dispatcher->Invoke(invoke);
          }
          return 0;
        }
        case snappin::kTrayMenuExitId: {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "app.exit";
            g_action_dispatcher->Invoke(invoke);
          } else {
            DestroyWindow(hwnd);
          }
          return 0;
        }
        case snappin::kTrayMenuSettingsId: {
          if (g_settings) {
            g_settings->Show();
          }
          return 0;
        }
        default:
          break;
      }
      break;
    }
    case kTrayCallbackMessage: {
      if (g_config_service && g_config_service->DebugEnabled(false)) {
        char buffer[128];
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE,
                    "tray cb lparam=0x%08X\n", static_cast<unsigned int>(lparam));
        OutputDebugStringA(buffer);
      }
      if (lparam == WM_RBUTTONUP || lparam == WM_RBUTTONDOWN ||
          lparam == WM_CONTEXTMENU) {
        POINT pt;
        if (GetCursorPos(&pt)) {
          g_tray.ShowContextMenu(pt);
        }
        return 0;
      }
      if (lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK ||
          lparam == NIN_SELECT || lparam == NIN_KEYSELECT) {
        if (g_settings) {
          g_settings->Show();
        }
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      if (g_keybindings_service) {
        g_keybindings_service->Shutdown();
      }
      g_tray.Cleanup();
      PostQuitMessage(0);
      return 0;
    case WM_HOTKEY: {
      if (g_keybindings_service && g_action_dispatcher) {
        auto action = g_keybindings_service->ActionForHotkeyId(wparam);
        if (action.has_value()) {
          snappin::ActionInvoke invoke;
          invoke.id = *action;
          g_action_dispatcher->Invoke(invoke);
        }
      }
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

HWND CreateHiddenMainWindow(HINSTANCE instance) {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = MainWndProc;
  wc.hInstance = instance;
  wc.lpszClassName = kMainWindowClass;

  if (!RegisterClassExW(&wc)) {
    return nullptr;
  }

  HWND hwnd = CreateWindowExW(
      0, kMainWindowClass, L"SnapPin", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0,
      nullptr, nullptr, instance, nullptr);
  if (!hwnd) {
    return nullptr;
  }

  ShowWindow(hwnd, SW_HIDE);
  return hwnd;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  snappin::SingleInstanceGuard guard(snappin::BuildInstanceMutexName());
  if (!guard.IsPrimary()) {
    return 0;
  }

  g_taskbar_created_msg = RegisterWindowMessageW(L"TaskbarCreated");

  HWND hwnd = CreateHiddenMainWindow(instance);
  if (!hwnd) {
    return 1;
  }

  g_action_registry = std::make_unique<snappin::ActionRegistry>();
  g_config_service = std::make_unique<snappin::ConfigService>();
  snappin::Result<void> config_init = g_config_service->Initialize();
  if (!config_init.ok) {
    OutputDebugStringA("Config init failed\n");
  }
  g_stats = std::make_unique<snappin::StatsService>();
  g_capture_service = snappin::CreateCaptureService();
  g_artifact_store = std::make_unique<snappin::ArtifactStore>();
  g_export_service = std::make_unique<snappin::ExportService>();

  g_toolbar = std::make_unique<snappin::ToolbarWindow>();
  if (!g_toolbar->Create(instance)) {
    OutputDebugStringA("Toolbar create failed\n");
  }
  g_settings = std::make_unique<snappin::SettingsWindow>();
  if (!g_settings->Create(instance)) {
    OutputDebugStringA("Settings create failed\n");
  }
  g_overlay = std::make_unique<snappin::OverlayWindow>();
  if (!g_overlay->Create(instance)) {
    OutputDebugStringA("Overlay create failed\n");
  } else {
    if (g_stats) {
      g_stats->SetOverlayShowMs(1.0);
    }
    g_overlay->SetCallbacks(
        [](const snappin::RectPX& rect) {
          g_runtime_state.overlay_visible = false;
          if (g_capture_service && g_artifact_store) {
            ULONGLONG t0 = GetTickCount64();
            snappin::CaptureTarget target;
            target.type = snappin::CaptureTargetType::REGION;
            target.region_px = rect;
            snappin::CaptureOptions options;
            snappin::Result<snappin::CaptureFrame> result =
                g_capture_service->CaptureOnce(target, options);
            if (result.ok) {
              ULONGLONG t1 = GetTickCount64();
              if (g_stats) {
                g_stats->SetCaptureOnceMs(static_cast<double>(t1 - t0));
              }
              snappin::Artifact artifact;
              artifact.artifact_id = g_artifact_store->NextId();
              artifact.kind = snappin::ArtifactKind::CAPTURE;
              artifact.screen_rect_px = result.value.screen_rect_px;
              artifact.dpi_scale = result.value.dpi_scale;
              g_artifact_store->Put(artifact);
              g_runtime_state.active_artifact_id = artifact.artifact_id;
              if (g_config_service && g_config_service->CaptureAutoShowToolbar(true) &&
                  g_toolbar) {
                g_toolbar->ShowAtRect(result.value.screen_rect_px);
              }
              char buffer[160];
              _snprintf_s(buffer, sizeof(buffer), _TRUNCATE,
                          "capture ok %dx%d artifact=%llu\n",
                          result.value.size_px.w, result.value.size_px.h,
                          static_cast<unsigned long long>(artifact.artifact_id.value));
              OutputDebugStringA(buffer);
              if (g_config_service && g_action_dispatcher &&
                  g_config_service->CaptureAutoCopyToClipboard(true)) {
                snappin::ActionInvoke invoke;
                invoke.id = "export.copy_image";
                g_action_dispatcher->Invoke(invoke);
              }
            } else {
              OutputDebugStringA("capture failed\n");
            }
            PROCESS_MEMORY_COUNTERS_EX pmc = {};
            if (GetProcessMemoryInfo(GetCurrentProcess(),
                                     reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                     sizeof(pmc))) {
              if (g_stats) {
                g_stats->SetWorkingSetBytes(pmc.WorkingSetSize);
              }
            }
          }
        },
        []() {
          g_runtime_state.overlay_visible = false;
          OutputDebugStringA("overlay cancel\n");
        });
  }

  if (g_toolbar) {
    g_toolbar->SetCallbacks(
        []() {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "export.copy_image";
            g_action_dispatcher->Invoke(invoke);
            snappin::ActionInvoke close;
            close.id = "artifact.dismiss";
            g_action_dispatcher->Invoke(close);
          }
        },
        []() {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "export.save_image";
            g_action_dispatcher->Invoke(invoke);
            snappin::ActionInvoke close;
            close.id = "artifact.dismiss";
            g_action_dispatcher->Invoke(close);
          }
        },
        []() {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "artifact.dismiss";
            g_action_dispatcher->Invoke(invoke);
          }
        });
  }

  if (g_settings) {
    g_settings->SetCallbacks(
        []() {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "capture.start";
            g_action_dispatcher->Invoke(invoke);
          }
        },
        []() {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "settings.reload";
            g_action_dispatcher->Invoke(invoke);
          }
        },
        []() {
          if (g_config_service) {
            ShellExecuteW(nullptr, L"open",
                          g_config_service->ConfigDir().c_str(),
                          nullptr, nullptr, SW_SHOWNORMAL);
          }
        },
        []() {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "app.exit";
            g_action_dispatcher->Invoke(invoke);
          }
        });
  }
  g_action_dispatcher = std::make_unique<snappin::ActionDispatcher>(
      *g_action_registry, &g_runtime_state, hwnd, g_config_service.get(),
      g_overlay.get(), g_artifact_store.get(), g_export_service.get(),
      g_toolbar.get(), g_settings.get());
  g_keybindings_service = std::make_unique<snappin::KeybindingsService>();
  snappin::Result<void> hotkeys = g_keybindings_service->Initialize(
      *g_config_service, *g_action_registry, hwnd);
  if (!hotkeys.ok) {
    OutputDebugStringA("Hotkeys init failed\n");
  }
  g_action_dispatcher->Subscribe([](const snappin::ActionEvent& ev) {
    if (g_config_service && !g_config_service->DebugEnabled(false)) {
      return;
    }
    char buffer[128];
    _snprintf_s(buffer, sizeof(buffer), _TRUNCATE,
                "action=%s type=%d correlation=%llu\n", ev.action_id.c_str(),
                static_cast<int>(ev.type),
                static_cast<unsigned long long>(ev.correlation_id.value));
    OutputDebugStringA(buffer);
  });

  if (!g_tray.Init(hwnd, kTrayCallbackMessage, kTrayIconId)) {
    // Tray is optional for now; continue running.
  }

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  g_action_dispatcher.reset();
  g_action_registry.reset();
  g_config_service.reset();
  g_keybindings_service.reset();
  g_capture_service.reset();
  g_artifact_store.reset();
  g_export_service.reset();
  g_overlay.reset();
  g_toolbar.reset();
  g_stats.reset();
  g_settings.reset();
  return static_cast<int>(msg.wParam);
}
