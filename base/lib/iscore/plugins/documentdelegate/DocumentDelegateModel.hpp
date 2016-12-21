#pragma once
#include <iscore/model/IdentifiedObject.hpp>

#include <iscore/selection/Selection.hpp>

struct VisitorVariant;

namespace iscore
{
class ISCORE_LIB_BASE_EXPORT DocumentDelegateModel
    : public IdentifiedObject<DocumentDelegateModel>
{
  Q_OBJECT
public:
  using IdentifiedObject<DocumentDelegateModel>::IdentifiedObject;
  virtual ~DocumentDelegateModel();

  virtual void serialize(const VisitorVariant&) const = 0;

public slots:
  virtual void setNewSelection(const Selection& s) = 0;
};
}
