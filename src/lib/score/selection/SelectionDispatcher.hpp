#pragma once
#include "Selection.hpp"

namespace score
{
class SelectionStack;

/**
 * @brief The SelectionDispatcher class
 *
 * Sends new selections to the document so that they
 * can be shown in the inspector and other places that work with selections.
 */
class SCORE_LIB_BASE_EXPORT SelectionDispatcher
{
public:
  explicit SelectionDispatcher(SelectionStack& s) : m_stack{s} { }

  void deselect();
  void select(const Selection&);
  void select(const IdentifiedObjectAbstract& obj);

  score::SelectionStack& stack() const;

private:
  score::SelectionStack& m_stack;
};
}
