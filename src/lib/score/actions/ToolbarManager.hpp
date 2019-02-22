#pragma once
#include <score/actions/Toolbar.hpp>
#include <score/tools/std/HashMap.hpp>

namespace score
{
/**
 * @brief The ToolbarManager class
 *
 * Keeps track of the \ref Toolbar%s registered in the software.
 *
 * Accessible through an \ref score::ApplicationContext.
 */
class SCORE_LIB_BASE_EXPORT ToolbarManager
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
  score::hash_map<StringKey<Toolbar>, Toolbar> m_container;
};
}
