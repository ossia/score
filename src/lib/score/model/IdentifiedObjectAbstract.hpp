#pragma once
#include <QObject>

#include <score_lib_base_export.h>

#include <cinttypes>
#include <verdigris>

/**
 * @brief Base class for IdentifiedObject.
 *
 * Is only necessary because one cannot have signals in template classes, but
 * we cannot switch to woboq/verdigris yet due to MSVC2015...
 */
class SCORE_LIB_BASE_EXPORT IdentifiedObjectAbstract : public QObject
{
  W_OBJECT(IdentifiedObjectAbstract)
public:
  virtual int32_t id_val() const noexcept = 0;
  ~IdentifiedObjectAbstract() override;

  //! To be called by subclasses
  void identified_object_destroying(IdentifiedObjectAbstract* o)
  E_SIGNAL(SCORE_LIB_BASE_EXPORT, identified_object_destroying, o)

  //! Will be called in the IdentifiedObjectAbstract destructor.
  void identified_object_destroyed(IdentifiedObjectAbstract* o)
  E_SIGNAL(SCORE_LIB_BASE_EXPORT, identified_object_destroyed, o)

  virtual void resetCache() const noexcept = 0;

protected:
  using QObject::QObject;
  IdentifiedObjectAbstract(const QString& name, QObject* parent) noexcept
  {
    QObject::setObjectName(name);
    QObject::setParent(parent);
  }
};

W_REGISTER_ARGTYPE(IdentifiedObjectAbstract*)
