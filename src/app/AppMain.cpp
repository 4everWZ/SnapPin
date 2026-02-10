#include "ActionDispatcher.h"
#include "ActionRegistry.h"
#include "ArtifactStore.h"
#include "ConfigService.h"
#include "CaptureService.h"
#include "CaptureFreeze.h"
#include "ExportService.h"
#include "KeybindingsService.h"
#include "PinManager.h"
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
#include <cstring>
#include <memory>
#include <vector>

namespace {
const wchar_t kMainWindowClass[] = L"SnapPinHiddenWindow";
const UINT kTrayCallbackMessage = WM_USER + 1;
const UINT kTrayIconId = 1;
const int kSessionCopyHotkeyId = 0x51C0;

UINT g_taskbar_created_msg = 0;
snappin::TrayIcon g_tray;
HWND g_main_hwnd = nullptr;
bool g_session_copy_hotkey_registered = false;

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
std::unique_ptr<snappin::PinManager> g_pin_manager;

void SetSessionCopyHotkey(bool enabled) {
  if (!g_main_hwnd) {
    return;
  }
  if (enabled && !g_session_copy_hotkey_registered) {
    if (RegisterHotKey(g_main_hwnd, kSessionCopyHotkeyId,
                       MOD_CONTROL | MOD_NOREPEAT, 'C')) {
      g_session_copy_hotkey_registered = true;
    }
    return;
  }
  if (!enabled && g_session_copy_hotkey_registered) {
    UnregisterHotKey(g_main_hwnd, kSessionCopyHotkeyId);
    g_session_copy_hotkey_registered = false;
  }
}

std::optional<snappin::CpuBitmap> CropFrozenFrame(
    const snappin::FrozenFrame& frozen, const snappin::RectPX& selection,
    std::shared_ptr<std::vector<uint8_t>>* storage_out,
    snappin::RectPX* out_rect) {
  if (!storage_out) {
    return std::nullopt;
  }
  if (!frozen.pixels || frozen.pixels->empty()) {
    return std::nullopt;
  }

  int32_t rel_x = selection.x - frozen.screen_rect_px.x;
  int32_t rel_y = selection.y - frozen.screen_rect_px.y;
  int32_t w = selection.w;
  int32_t h = selection.h;

  if (rel_x < 0) {
    w += rel_x;
    rel_x = 0;
  }
  if (rel_y < 0) {
    h += rel_y;
    rel_y = 0;
  }

  if (rel_x + w > frozen.size_px.w) {
    w = frozen.size_px.w - rel_x;
  }
  if (rel_y + h > frozen.size_px.h) {
    h = frozen.size_px.h - rel_y;
  }

  if (w <= 0 || h <= 0) {
    return std::nullopt;
  }

  const int32_t src_stride = frozen.stride_bytes;
  const int32_t dst_stride = w * 4;
  const size_t row_bytes = static_cast<size_t>(dst_stride);
  const size_t total = row_bytes * static_cast<size_t>(h);

  auto storage = std::make_shared<std::vector<uint8_t>>();
  storage->resize(total);

  const uint8_t* src_base =
      reinterpret_cast<const uint8_t*>(frozen.pixels->data());
  uint8_t* dst_base = storage->data();

  const size_t src_row_offset = static_cast<size_t>(rel_x) * 4;
  for (int32_t y = 0; y < h; ++y) {
    const uint8_t* src =
        src_base + static_cast<size_t>(rel_y + y) * src_stride + src_row_offset;
    uint8_t* dst = dst_base + static_cast<size_t>(y) * dst_stride;
    std::memcpy(dst, src, row_bytes);
  }

  if (out_rect) {
    out_rect->x = frozen.screen_rect_px.x + rel_x;
    out_rect->y = frozen.screen_rect_px.y + rel_y;
    out_rect->w = w;
    out_rect->h = h;
  }

  snappin::CpuBitmap bmp;
  bmp.format = snappin::PixelFormat::BGRA8;
  bmp.size_px = snappin::SizePX{w, h};
  bmp.stride_bytes = dst_stride;
  bmp.data.p = storage->data();
  *storage_out = std::move(storage);
  return bmp;
}

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
    case snappin::PinManager::kWindowCommandMessage:
      if (g_pin_manager) {
        g_pin_manager->HandleWindowCommand(wparam, lparam);
      }
      return 0;
    case kTrayCallbackMessage: {
      const UINT tray_msg = static_cast<UINT>(LOWORD(lparam));
      if (g_config_service && g_config_service->DebugEnabled(false)) {
        char buffer[128];
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE,
                    "tray cb wparam=0x%08X lparam=0x%08X msg=0x%04X\n",
                    static_cast<unsigned int>(wparam),
                    static_cast<unsigned int>(lparam),
                    static_cast<unsigned int>(tray_msg));
        OutputDebugStringA(buffer);
      }
      if (tray_msg == WM_RBUTTONUP || tray_msg == WM_RBUTTONDOWN ||
          tray_msg == WM_CONTEXTMENU) {
        POINT pt;
        if (GetCursorPos(&pt)) {
          g_tray.ShowContextMenu(pt);
        }
        return 0;
      }
      if (tray_msg == WM_LBUTTONUP || tray_msg == WM_LBUTTONDBLCLK ||
          tray_msg == NIN_SELECT || tray_msg == NIN_KEYSELECT) {
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
      SetSessionCopyHotkey(false);
      if (g_pin_manager) {
        g_pin_manager->Shutdown();
      }
      if (g_keybindings_service) {
        g_keybindings_service->Shutdown();
      }
      g_tray.Cleanup();
      PostQuitMessage(0);
      return 0;
    case WM_HOTKEY: {
      if (wparam == kSessionCopyHotkeyId) {
        if (g_action_dispatcher && g_runtime_state.active_artifact_id.has_value()) {
          snappin::ActionInvoke copy;
          copy.id = "export.copy_image";
          g_action_dispatcher->Invoke(copy);
          snappin::ActionInvoke close;
          close.id = "artifact.dismiss";
          g_action_dispatcher->Invoke(close);
        }
        return 0;
      }
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
  g_main_hwnd = hwnd;

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
  g_pin_manager = std::make_unique<snappin::PinManager>();
  if (!g_pin_manager->Initialize(instance, hwnd, &g_runtime_state, g_config_service.get(), g_export_service.get())) {
    OutputDebugStringA("Pin manager init failed\n");
  }

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
          ULONGLONG t0 = GetTickCount64();
          bool captured = false;

          std::optional<snappin::FrozenFrame> frozen =
              snappin::ConsumeFrozenFrame();
          if (frozen.has_value()) {
            std::shared_ptr<std::vector<uint8_t>> storage;
            snappin::RectPX actual_rect = rect;
            std::optional<snappin::CpuBitmap> bmp =
                CropFrozenFrame(*frozen, rect, &storage, &actual_rect);
            if (bmp.has_value() && g_artifact_store) {
              ULONGLONG t1 = GetTickCount64();
              if (g_stats) {
                g_stats->SetCaptureOnceMs(static_cast<double>(t1 - t0));
              }
              snappin::Artifact artifact;
              artifact.artifact_id = g_artifact_store->NextId();
              artifact.kind = snappin::ArtifactKind::CAPTURE;
              artifact.base_cpu = *bmp;
              artifact.base_cpu_storage = std::move(storage);
              artifact.screen_rect_px = actual_rect;
              artifact.dpi_scale = 1.0f;
              g_artifact_store->Put(artifact);
              g_runtime_state.active_artifact_id = artifact.artifact_id;
              SetSessionCopyHotkey(true);
              if (g_config_service && g_config_service->CaptureAutoShowToolbar(true) &&
                  g_toolbar) {
                g_toolbar->ShowAtRect(actual_rect);
              }
              char buffer[160];
              _snprintf_s(buffer, sizeof(buffer), _TRUNCATE,
                          "capture ok %dx%d artifact=%llu\n", bmp->size_px.w,
                          bmp->size_px.h,
                          static_cast<unsigned long long>(artifact.artifact_id.value));
              OutputDebugStringA(buffer);
              if (g_config_service && g_action_dispatcher &&
                  g_config_service->CaptureAutoCopyToClipboard(true)) {
                snappin::ActionInvoke invoke;
                invoke.id = "export.copy_image";
                g_action_dispatcher->Invoke(invoke);
              }
              captured = true;
            } else {
              OutputDebugStringA("capture freeze crop failed\n");
            }
          }
          if (!captured && g_capture_service && g_artifact_store) {
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
              SetSessionCopyHotkey(true);
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
              captured = true;
            } else {
              OutputDebugStringA("capture failed\n");
            }
          }

          PROCESS_MEMORY_COUNTERS_EX pmc = {};
          if (GetProcessMemoryInfo(GetCurrentProcess(),
                                   reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                   sizeof(pmc))) {
            if (g_stats) {
              g_stats->SetWorkingSetBytes(pmc.WorkingSetSize);
            }
          }
        },
        []() {
          g_runtime_state.overlay_visible = false;
          snappin::ClearFrozenFrame();
          if (g_overlay) {
            g_overlay->ClearFrozenFrame();
          }
          if (g_toolbar) {
            g_toolbar->Hide();
          }
          if (g_artifact_store) {
            g_artifact_store->ClearActive();
          }
          g_runtime_state.active_artifact_id.reset();
          SetSessionCopyHotkey(false);
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
            invoke.id = "pin.create_from_artifact";
            g_action_dispatcher->Invoke(invoke);
          }
        },
        []() {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "annotate.open";
            g_action_dispatcher->Invoke(invoke);
          }
        },
        []() {
          if (g_action_dispatcher) {
            snappin::ActionInvoke invoke;
            invoke.id = "ocr.start";
            g_action_dispatcher->Invoke(invoke);
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
      g_toolbar.get(), g_settings.get(), g_pin_manager.get());
  g_keybindings_service = std::make_unique<snappin::KeybindingsService>();
  snappin::Result<void> hotkeys = g_keybindings_service->Initialize(
      *g_config_service, *g_action_registry, hwnd);
  if (!hotkeys.ok) {
    OutputDebugStringA("Hotkeys init failed\n");
  }
  g_action_dispatcher->Subscribe([](const snappin::ActionEvent& ev) {
    if (ev.action_id == "capture.start" &&
        ev.type == snappin::ActionEvent::Type::Started) {
      SetSessionCopyHotkey(false);
    }
    if (ev.action_id == "artifact.dismiss" &&
        ev.type == snappin::ActionEvent::Type::Succeeded) {
      SetSessionCopyHotkey(false);
    }
    if (ev.action_id == "pin.create_from_artifact" &&
        ev.type == snappin::ActionEvent::Type::Succeeded) {
      SetSessionCopyHotkey(false);
    }
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
  g_pin_manager.reset();
  return static_cast<int>(msg.wParam);
}


