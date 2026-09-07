#pragma once
#include <ossia/network/value/value.hpp>

#include <QWidget>

#include <score_lib_process_export.h>

#include <functional>

class QListWidget;

namespace Process
{
// Reuses ControlInlet's ordinary LIST value: [[stable id, text], ...].
// No transient widget state is needed to restore an edit or a preset.
class SCORE_LIB_PROCESS_EXPORT StringListEditor final : public QWidget
{
public:
  std::function<void(ossia::value)> on_edited;

  explicit StringListEditor(QWidget* parent = nullptr);

  void setValue(const ossia::value& value);

  ossia::value value() const;

private:
  QListWidget* rows{};
  void append(int key, const QString& text);
  void commit();
  void move(int row, int direction);
};
}
