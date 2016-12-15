#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_lib_base_export.h>

class QToolBar;
namespace iscore
{
class ISCORE_LIB_BASE_EXPORT Toolbar
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

  int m_defaultRow = 0;
  int m_defaultCol = 0;
};

}
