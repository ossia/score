#pragma once
#include <QObject>
#include <cinttypes>
#include <iscore_lib_base_export.h>

/**
 * @brief Base class for IdentifiedObject.
 *
 * Is only necessary because one cannot have signals in template classes, but
 * we cannot switch to woboq/verdigris yet due to MSVC2015...
 */
class ISCORE_LIB_BASE_EXPORT IdentifiedObjectAbstract : public QObject
{
  Q_OBJECT
public:
  virtual int32_t id_val() const = 0;
  virtual ~IdentifiedObjectAbstract();

signals:
  //! To be called by subclasses
  void identified_object_destroying(IdentifiedObjectAbstract*);

  //! Will be called in the IdentifiedObjectAbstract destructor.
  void identified_object_destroyed(IdentifiedObjectAbstract*);

protected:
  using QObject::QObject;
  IdentifiedObjectAbstract(const QString& name, QObject* parent)
  {
    QObject::setObjectName(name);
    QObject::setParent(parent);
  }
};

