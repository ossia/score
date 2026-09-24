#pragma once
#include <score/plugins/StringFactoryKey.hpp>

#include <QPointer>

#include <score_lib_base_export.h>

class QToolBar;
namespace score
{
/**
 * @brief A toolbar provided by an application plug-in.
 *
 * Owned by \ref ToolbarManager once registered. The main window may reparent
 * and destroy it, so toolbar() returns nullptr once the QToolBar is gone.
 */
class SCORE_LIB_BASE_EXPORT Toolbar
{
public:
  Toolbar(QToolBar* tb, StringKey<Toolbar> key, int defaultRow, int defaultCol);

  QToolBar* toolbar() const;

  StringKey<Toolbar> key() const;

  int row() const;
  int column() const;

private:
  QPointer<QToolBar> m_impl{};
  StringKey<Toolbar> m_key;

  // If a row is used, it goes next
  // Maybe it should be a list instead ?
  int m_row = 0;
  int m_col = 0;
};
}
