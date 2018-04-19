// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MenuManager.hpp"

#include <QMenu>
namespace score
{

Menu::Menu(QMenu* menu, StringKey<Menu> m) : m_impl{menu}, m_key{std::move(m)}
{
}

Menu::Menu(QMenu* menu, StringKey<Menu> m, Menu::is_toplevel, int column)
    : m_impl{menu}, m_key{std::move(m)}, m_col{column}, m_toplevel{true}
{
}

StringKey<Menu> Menu::key() const
{
  return m_key;
}

QMenu* Menu::menu() const
{
  return m_impl;
}

int Menu::column() const
{
  return m_col;
}

bool Menu::toplevel() const
{
  return m_toplevel;
}

StringKey<Menu> Menus::File()
{
  return StringKey<Menu>{"File"};
}

StringKey<Menu> Menus::Export()
{
  return StringKey<Menu>{"Export"};
}

StringKey<Menu> Menus::Edit()
{
  return StringKey<Menu>{"Edit"};
}

StringKey<Menu> Menus::Object()
{
  return StringKey<Menu>{"Object"};
}

StringKey<Menu> Menus::Play()
{
  return StringKey<Menu>{"Play"};
}

StringKey<Menu> Menus::View()
{
  return StringKey<Menu>{"View"};
}

StringKey<Menu> Menus::Windows()
{
  return StringKey<Menu>{"Windows"};
}

StringKey<Menu> Menus::Settings()
{
  return StringKey<Menu>{"Settings"};
}

StringKey<Menu> Menus::About()
{
  return StringKey<Menu>{"About"};
}

void MenuManager::insert(Menu val)
{
  m_container.insert(std::make_pair(val.key(), std::move(val)));
}

void MenuManager::insert(std::vector<Menu> vals)
{
  for (auto& val : vals)
  {
    insert(std::move(val));
  }
}
}
