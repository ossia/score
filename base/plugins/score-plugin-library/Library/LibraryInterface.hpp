#pragma once
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score_plugin_library_export.h>
#include <score/application/GUIApplicationContext.hpp>
class QAbstractItemModel;

namespace Library
{
class ProcessesItemModel;
class SCORE_PLUGIN_LIBRARY_EXPORT LibraryInterface
    : public score::InterfaceBase
{
  SCORE_INTERFACE(LibraryInterface, "9b94d974-9f2d-4986-a62b-b69e51a4d305")
public:
  ~LibraryInterface() override;

  virtual void setup(ProcessesItemModel& model, const score::GUIApplicationContext& ctx);
};

class SCORE_PLUGIN_LIBRARY_EXPORT LibraryInterfaceList final
    : public score::InterfaceList<LibraryInterface>
{
};
}
