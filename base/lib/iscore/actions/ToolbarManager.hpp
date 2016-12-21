#pragma once
#include <iscore/actions/Toolbar.hpp>
#include <iscore/tools/std/HashMap.hpp>

namespace iscore
{
/**
 * @brief The ToolbarManager class
 *
 * Keeps track of the \ref Toolbar%s registered in the software.
 *
 * Accessible through an \ref iscore::ApplicationContext.
 */
class ISCORE_LIB_BASE_EXPORT ToolbarManager
{
public:
  void insert(Toolbar val);

  void insert(std::vector<Toolbar> vals);

  auto& get()
  {
    return m_container;
  }
  auto& get() const
  {
    return m_container;
  }

private:
  iscore::hash_map<StringKey<Toolbar>, Toolbar> m_container;
};

}
