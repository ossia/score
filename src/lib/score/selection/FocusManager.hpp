#pragma once
#include <score/model/IdentifiedObjectAbstract.hpp>

#include <QPointer>

namespace score
{
struct SCORE_LIB_BASE_EXPORT FocusManager : public QObject
{
  W_OBJECT(FocusManager)
public:
  const IdentifiedObjectAbstract* get() { return m_obj; }

  template <typename T>
  void set(QPointer<T> obj)
  {
    m_obj = obj.data();
    changed();
  }

  void set(std::nullptr_t)
  {
    m_obj.clear();
    changed();
  }

  void changed() E_SIGNAL(SCORE_LIB_BASE_EXPORT, changed)

private:
  QPointer<IdentifiedObjectAbstract> m_obj{};
};

struct SCORE_LIB_BASE_EXPORT FocusFacade
{
private:
  FocusManager& m_mgr;

public:
  FocusFacade(FocusManager& mgr) : m_mgr{mgr} { }

  const IdentifiedObjectAbstract* get() const { return m_mgr.get(); }
};
}
