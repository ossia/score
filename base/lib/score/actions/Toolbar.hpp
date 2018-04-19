#pragma once
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score_lib_base_export.h>

class QToolBar;
namespace score
{
class SCORE_LIB_BASE_EXPORT Toolbar
{
public:
  Toolbar(
      QToolBar* tb, StringKey<Toolbar> key, int defaultRow, int defaultCol);

  QToolBar* toolbar() const;

  StringKey<Toolbar> key() const;

  int row() const;
  int column() const;

private:
  QToolBar* m_impl{};
  StringKey<Toolbar> m_key;

  // If a row is used, it goes next
  // Maybe it should be a list instead ?
  int m_row = 0;
  int m_col = 0;
};
}
