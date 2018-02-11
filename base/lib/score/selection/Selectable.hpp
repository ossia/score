#pragma once
#include <QObject>
#include <score_lib_base_export.h>

/**
 * @brief The Selectable class
 *
 * A component that allows a class to be selected (or not).
 */
class SCORE_LIB_BASE_EXPORT Selectable final : public QObject
{
  Q_OBJECT
public:
  Selectable()
  {
    connect(this, &Selectable::set, this, &Selectable::set_impl);
  }

  virtual ~Selectable()
  {
    set(false);
  }

  bool get() const
  {
    return m_val;
  }

  void set_impl(bool v)
  {
    if (m_val != v)
    {
      m_val = v;
      changed(v);
    }
  }

Q_SIGNALS:
  void set(bool) const;
  void changed(bool);

private:
  bool m_val{};
};
