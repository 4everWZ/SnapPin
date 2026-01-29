#include "Types.h"

int main() {
  snappin::RectPX r{};
  return (r.w == 0 && r.h == 0) ? 0 : 1;
}
