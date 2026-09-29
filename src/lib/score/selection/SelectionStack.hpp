#pragma once
#include <score/selection/Selection.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QObject>
#include <QStack>

#include <verdigris>

class IdentifiedObjectAbstract;

namespace score
{
/**
 * @brief The SelectionStack class
 *
 * A stack of selected elements.
 * Each time a selection of objects is done in the software,
 * it should be added to this stack using SelectionDispatcher.
 * This way, the user will be able to browse through his previous selections.
 */
class SCORE_LIB_BASE_EXPORT SelectionStack final : public QObject
{
  W_OBJECT(SelectionStack)
public:
  SelectionStack();
  ~SelectionStack();

  bool canUnselect() const;
  bool canReselect() const;
  void clear();
  void clearAllButLast();

  // Go to the previous set of selections
  void unselect();

  // Go to the next set of selections
  void reselect();

  // Push a new set of empty selection.
  void deselect();

  // Push a new selection without these objects
  void deselectObjects(const Selection& toDeselect);

  Selection currentSelection() const;

  void pushNewSelection(const Selection& s)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, pushNewSelection, s)

  void currentSelectionChanged(const Selection& old, const Selection& current)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, currentSelectionChanged, old, current)

  void prune(IdentifiedObjectAbstract* p);
  W_INVOKABLE(prune)

  void pruneRecursively(IdentifiedObjectAbstract* p);
  W_INVOKABLE(pruneRecursively)

  /**
   * While a batch is open, prune() only records the destroyed object.
   * The stack is swept once when the outermost batch ends, with a single
   * currentSelectionChanged: destroying k selected objects out of n costs
   * O(n) instead of O(k * n).
   *
   * Inside a batch, the stack may still hold the recorded objects until the
   * next mutation (push, unselect, ...), which sweeps first;
   * currentSelection() already leaves them out.
   */
  class Batch
  {
  public:
    explicit Batch(SelectionStack& s) noexcept
        : m_stack{s}
    {
      ++m_stack.m_batchDepth;
    }
    ~Batch()
    {
      if(--m_stack.m_batchDepth == 0)
        m_stack.sweepPruned();
    }
    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;

  private:
    SelectionStack& m_stack;
  };

private:
  // Select new objects
  void push(const Selection& s);
  void pruneConnections();
  void sweepPruned();

  // m_unselectable always contains the empty set at the beginning
  QStack<Selection> m_unselectable;
  QStack<Selection> m_reselectable;

  ossia::hash_map<const IdentifiedObjectAbstract*, QMetaObject::Connection>
      m_connections;

  // Destroyed during the open batch and not swept yet
  ossia::hash_set<const IdentifiedObjectAbstract*> m_pruned;
  int m_batchDepth{};
};
}
