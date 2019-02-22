#pragma once
#include <score/actions/Menu.hpp>
#include <score/tools/std/HashMap.hpp>

namespace score
{
/**
 * @brief The MenuManager class
 *
 * Keeps track of the \ref Menu%s registered in the software
 *
 * Accessible through an \ref score::ApplicationContext.
 */
class SCORE_LIB_BASE_EXPORT MenuManager
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
  score::hash_map<StringKey<Menu>, Menu> m_container;
};
}
