#pragma once
#include <score/actions/Toolbar.hpp>
#include <score/tools/std/HashMap.hpp>

namespace score
{
/**
 * @brief The ToolbarManager class
 *
 * Keeps track of the \ref Toolbar%s registered in the software, and owns
 * them: they are created without a parent, and without a main window
 * (MinimalApplication) nothing else deletes them.
 *
 * Accessible through an \ref score::ApplicationContext.
 */
class SCORE_LIB_BASE_EXPORT ToolbarManager
{
public:
  ToolbarManager() = default;
  ToolbarManager(const ToolbarManager&) = delete;
  ToolbarManager& operator=(const ToolbarManager&) = delete;
  ~ToolbarManager();

  void insert(Toolbar val);

  void insert(std::vector<Toolbar> vals);

  auto& get() { return m_container; }
  auto& get() const { return m_container; }

private:
  score::hash_map<StringKey<Toolbar>, Toolbar> m_container;
};
}
