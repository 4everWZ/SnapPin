#include "Types.h"
#include "OverlayWindow.h"

int main() {
  snappin::RectPX r{};
  if (!(r.w == 0 && r.h == 0)) {
    return 1;
  }

  if (!snappin::OverlayWindow::ShouldUseSelectionHole(
          true, true, false, false)) {
    return 2;
  }

  if (snappin::OverlayWindow::ShouldUseSelectionHole(
          false, false, true, false)) {
    return 3;
  }

  if (snappin::OverlayWindow::ShouldUseSelectionHole(
          true, false, true, true)) {
    return 4;
  }

  return 0;
}
