#pragma once
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

/**
 * @brief The Selectable class
 *
 * A component that allows a class to be selected (or not).
 */
class SCORE_LIB_BASE_EXPORT Selectable final : public QObject
{
  W_OBJECT(Selectable)
public:
  Selectable();
  virtual ~Selectable();

  bool get() const noexcept;
  void set(bool b);

  void changed(bool b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, changed, b)

private:
  bool m_val{};
};

