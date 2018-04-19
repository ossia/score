#pragma once
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score_lib_base_export.h>
class QMenu;
namespace score
{
/**
 * @brief Represents a menu in the software.
 */
class SCORE_LIB_BASE_EXPORT Menu
{
public:
  struct is_toplevel
  {
  };
  Menu(QMenu* menu, StringKey<Menu> m);

  Menu(
      QMenu* menu,
      StringKey<Menu> m,
      is_toplevel,
      int c = std::numeric_limits<int>::max() - 1);

  StringKey<Menu> key() const;

  QMenu* menu() const;

  int column() const;

  //! True if the menu is shown in the top menu bar
  bool toplevel() const;

private:
  QMenu* m_impl{};
  StringKey<Menu> m_key;
  int m_col = std::numeric_limits<int>::max() - 1;
  bool m_toplevel{};
};

struct SCORE_LIB_BASE_EXPORT Menus
{
  static StringKey<Menu> File();
  static StringKey<Menu> Export();
  static StringKey<Menu> Edit();
  static StringKey<Menu> Object();
  static StringKey<Menu> Play();
  static StringKey<Menu> View();
  static StringKey<Menu> Windows();
  static StringKey<Menu> Settings();
  static StringKey<Menu> About();
};
}
