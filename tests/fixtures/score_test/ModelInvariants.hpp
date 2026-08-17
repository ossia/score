#pragma once
// Catch2-native replacements for the two QtTest classes the suite used to reach
// for. The suite standardizes on Catch2 and links no QtTest anywhere; QtTest is
// also simply absent from some Qt builds (the macOS ossia-sdk static Qt ships
// 192 Qt6 modules without it), which made a QtTest dependency fail the whole
// configure rather than one target.

#include <score/tools/Debug.hpp>

#include <QAbstractItemModel>
#include <QObject>
#include <QPersistentModelIndex>

#include <catch2/catch_test_macros.hpp>

#include <set>

namespace score::test
{

//! Counts emissions of a signal. Replaces QSignalSpy for the count()-only uses.
class SignalCounter
{
public:
  template <typename Obj, typename Signal>
  SignalCounter(Obj* obj, Signal sig)
  {
    m_conn = QObject::connect(obj, sig, [this] { ++m_count; });
  }
  ~SignalCounter() { QObject::disconnect(m_conn); }

  SignalCounter(const SignalCounter&) = delete;
  SignalCounter& operator=(const SignalCounter&) = delete;

  int count() const noexcept { return m_count; }
  void clear() noexcept { m_count = 0; }

private:
  QMetaObject::Connection m_conn;
  int m_count{};
};

//! Walks the whole tree and checks the QAbstractItemModel contract, in place of
//! QAbstractItemModelTester. Covers what a tree model can realistically get
//! wrong: a child whose parent() does not round-trip, an index handed out for a
//! row/column outside the parent's counts, hasChildren() disagreeing with
//! rowCount(), a reused internal pointer aliasing two distinct indices, and
//! sibling/child accessors that contradict index().
inline void
checkModelInvariants(const QAbstractItemModel& m, const QModelIndex& parent = {})
{
  const int rows = m.rowCount(parent);
  const int cols = m.columnCount(parent);
  REQUIRE(rows >= 0);
  REQUIRE(cols >= 0);
  CHECK(m.hasChildren(parent) == (rows > 0 && cols > 0));

  // Out-of-range requests must produce invalid indices, not crashes or
  // fabricated ones.
  CHECK(!m.index(rows, 0, parent).isValid());
  CHECK(!m.index(-1, 0, parent).isValid());
  CHECK(!m.index(0, cols, parent).isValid());

  std::set<void*> seen;
  for(int r = 0; r < rows; ++r)
  {
    for(int c = 0; c < cols; ++c)
    {
      const QModelIndex idx = m.index(r, c, parent);
      REQUIRE(idx.isValid());
      CHECK(idx.row() == r);
      CHECK(idx.column() == c);
      CHECK(idx.model() == &m);
      CHECK(m.parent(idx) == parent);
      CHECK(m.sibling(r, c, idx) == idx);
    }

    const QModelIndex first = m.index(r, 0, parent);
    // Column 0 owns the children, so its internal pointer identifies the node.
    if(void* p = first.internalPointer(); p != nullptr)
    {
      CHECK(seen.insert(p).second);
    }
    checkModelInvariants(m, first);
  }
}
}
