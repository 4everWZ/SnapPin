#pragma once
#include "Artifact.h"
#include "Types.h"

#include <string>

namespace snappin {

enum class ImageFormat { PNG, JPEG, WEBP };

struct SaveImageOptions {
  ImageFormat format = ImageFormat::PNG;
  int32_t quality_0_100 = 90;
  std::wstring path;
  bool open_folder = false;
};

class IExportService {
public:
  virtual ~IExportService() = default;
  virtual Result<void> CopyImageToClipboard(const Artifact& art) = 0;
  virtual Result<std::wstring> SaveImage(const Artifact& art, const SaveImageOptions&) = 0;
  virtual Result<void> CopyTextToClipboard(const std::wstring& text) = 0;
};

class ExportService final : public IExportService {
public:
  Result<void> CopyImageToClipboard(const Artifact& art) override;
  Result<std::wstring> SaveImage(const Artifact& art,
                                 const SaveImageOptions&) override;
  Result<void> CopyTextToClipboard(const std::wstring& text) override;
};

} // namespace snappin
