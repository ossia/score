#pragma once
#include <QFileIconProvider>

#include <score_lib_base_export.h>

namespace score
{

class SCORE_LIB_BASE_EXPORT IconProvider : public QFileIconProvider
{
public:
  ~IconProvider() override;

  static IconProvider& instance() noexcept;
  static const QIcon& folderIcon() noexcept;

  QIcon icon(IconType type) const override;
  QIcon icon(const QFileInfo& info) const override;
  QString type(const QFileInfo&) const override;
};

}
