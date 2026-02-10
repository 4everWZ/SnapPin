#pragma once
#include "Types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace snappin {

class AnnotateWindow {
public:
  enum class Command {
    Copy = 1,
    Save = 2,
    Close = 3,
    Reselect = 4,
  };

  using CommandCallback =
      std::function<void(Command, std::shared_ptr<std::vector<uint8_t>>,
                         const SizePX&, int32_t)>;

  AnnotateWindow() = default;
  ~AnnotateWindow();

  bool Create(HINSTANCE instance);
  void Destroy();

  bool BeginSession(const RectPX& screen_rect,
                    std::shared_ptr<std::vector<uint8_t>> source_pixels,
                    const SizePX& size_px, int32_t stride_bytes);
  void EndSession();
  bool IsVisible() const;

  void SetCommandCallback(CommandCallback on_command);

private:
  enum class Tool {
    Select,
    Rect,
    Line,
    Arrow,
    Pencil,
    Text,
  };

  enum class AnnotationType {
    Rect,
    Line,
    Arrow,
    Pencil,
    Text,
  };

  enum class DragMode {
    None,
    CreateRect,
    CreateLine,
    CreateArrow,
    CreatePencil,
    MoveRect,
    ResizeRectTL,
    ResizeRectTR,
    ResizeRectBL,
    ResizeRectBR,
    MoveLine,
    MoveLineStart,
    MoveLineEnd,
    MoveText,
  };

  struct Annotation {
    AnnotationType type = AnnotationType::Rect;
    COLORREF color = RGB(255, 80, 64);
    int thickness = 2;
    POINT p1{};
    POINT p2{};
    std::vector<POINT> points; // Pencil path.
    std::wstring text;         // Text payload.
    int text_size = 20;
  };

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void EnsureControls();
  void LayoutControls();
  void UpdateToolButtons();
  void SetTool(Tool tool);
  void Invalidate();

  RECT CanvasRectClient() const;
  bool ToCanvasPoint(POINT client_pt, POINT* out_canvas) const;
  POINT ClampToCanvas(POINT pt) const;
  void BeginDrag(POINT canvas_pt);
  void UpdateDrag(POINT canvas_pt);
  void EndDrag(POINT canvas_pt);

  int HitTestAnnotation(POINT canvas_pt, DragMode* mode_out) const;
  bool AnnotationTypeAllowedByTool(AnnotationType type) const;
  bool AnnotationEditable(AnnotationType type) const;
  RectPX RectFromPoints(POINT a, POINT b) const;
  RectPX RectBoundsForAnnotation(const Annotation& ann) const;
  RectPX NormalizeRect(RectPX rect) const;

  void DrawOverlay(HDC hdc) const;
  void DrawAnnotation(HDC hdc, const Annotation& ann, bool selected) const;
  void DrawArrowHead(HDC hdc, POINT start, POINT end, COLORREF color,
                     int thickness) const;
  void DrawSelectionHandles(HDC hdc, const Annotation& ann) const;
  bool BuildComposedPixels(std::shared_ptr<std::vector<uint8_t>>* out_pixels,
                           SizePX* out_size, int32_t* out_stride) const;

  void PushHistory();
  bool Undo();
  bool Redo();
  void DeleteSelection();
  void ShowContextMenu(POINT screen_pt);
  void EmitCommand(Command cmd);

  HWND hwnd_ = nullptr;
  HINSTANCE instance_ = nullptr;
  bool visible_ = false;

  RectPX screen_rect_px_{};
  SizePX bitmap_size_px_{};
  int32_t stride_bytes_ = 0;
  std::shared_ptr<std::vector<uint8_t>> source_pixels_;

  Tool tool_ = Tool::Rect;
  COLORREF color_ = RGB(255, 80, 64);
  int thickness_ = 2;

  DragMode drag_mode_ = DragMode::None;
  bool dragging_ = false;
  POINT drag_start_{};
  POINT drag_current_{};
  Annotation drag_seed_{};
  int selected_index_ = -1;
  int drag_index_ = -1;
  POINT drag_offset_{};

  bool text_editing_ = false;
  int text_edit_index_ = -1;

  std::vector<Annotation> annotations_;
  std::vector<std::vector<Annotation>> history_;
  size_t history_index_ = 0;

  HWND btn_select_ = nullptr;
  HWND btn_rect_ = nullptr;
  HWND btn_line_ = nullptr;
  HWND btn_arrow_ = nullptr;
  HWND btn_pencil_ = nullptr;
  HWND btn_text_ = nullptr;
  HWND btn_reselect_ = nullptr;
  HWND btn_undo_ = nullptr;
  HWND btn_redo_ = nullptr;
  HWND btn_copy_ = nullptr;
  HWND btn_save_ = nullptr;
  HWND btn_close_ = nullptr;

  CommandCallback on_command_;
};

} // namespace snappin
