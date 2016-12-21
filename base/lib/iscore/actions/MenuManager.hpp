#pragma once
#include <iscore/actions/Menu.hpp>
#include <iscore/tools/std/HashMap.hpp>

namespace iscore
{
/**
 * @brief The MenuManager class
 *
 * Keeps track of the \ref Menu%s registered in the software
 *
 * Accessible through an \ref iscore::ApplicationContext.
 */
class ISCORE_LIB_BASE_EXPORT MenuManager
{
public:
  void insert(Menu val);

  void insert(std::vector<Menu> vals);

  auto& get()
  {
    return m_container;
  }
  auto& get() const
  {
    return m_container;
  }

private:
  iscore::hash_map<StringKey<Menu>, Menu> m_container;
};


}
