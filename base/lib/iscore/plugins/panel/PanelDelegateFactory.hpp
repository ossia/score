#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>

namespace iscore
{
//! Reimplement this interface to register new panels.
class ISCORE_LIB_BASE_EXPORT PanelDelegateFactory
    : public iscore::Interface<PanelDelegateFactory>
{
  ISCORE_INTERFACE("8d6211f7-5244-44f9-94dd-f3e32255c43e")
public:
  virtual ~PanelDelegateFactory();

  //! Create an instance of a PanelDelegate. Will only be called once.
  virtual std::unique_ptr<PanelDelegate>
  make(const iscore::GUIApplicationContext& ctx) = 0;
};

//! All the panels are registerd in this interface list.
class ISCORE_LIB_BASE_EXPORT PanelDelegateFactoryList final
    : public InterfaceList<iscore::PanelDelegateFactory>
{
public:
  using object_type = PanelDelegate;
};
}
