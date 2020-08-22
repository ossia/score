#pragma once
#include <score/selection/Selection.hpp>

#include <QObject>
#include <QStack>

#include <tsl/hopscotch_map.h>

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

  void pushNewSelection(const Selection& s) E_SIGNAL(SCORE_LIB_BASE_EXPORT, pushNewSelection, s)

  void currentSelectionChanged(const Selection& old, const Selection& current)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, currentSelectionChanged, old, current)

  void prune(IdentifiedObjectAbstract* p);
  W_INVOKABLE(prune)

private:
  // Select new objects
  void push(const Selection& s);
  void pruneConnections();

  // m_unselectable always contains the empty set at the beginning
  QStack<Selection> m_unselectable;
  QStack<Selection> m_reselectable;

  tsl::hopscotch_map<const IdentifiedObjectAbstract*, QMetaObject::Connection> m_connections;
};
}
