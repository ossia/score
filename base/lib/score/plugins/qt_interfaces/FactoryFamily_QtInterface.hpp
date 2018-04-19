#pragma once
#include <QObject>
#include <memory>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <vector>

namespace score
{

// Reimplement in plug-in if the plug-in offers a NEW ABSTRACT TYPE of data.
// Example : the Inspector plug-in provides an interface for an inspector
// widget factory.
class SCORE_LIB_BASE_EXPORT FactoryList_QtInterface
{
public:
  virtual ~FactoryList_QtInterface();
  virtual std::vector<std::unique_ptr<InterfaceListBase>> factoryFamilies()
      = 0;
};
}

#define FactoryList_QtInterface_iid \
  "org.ossia.score.plugins.FactoryList_QtInterface"

Q_DECLARE_INTERFACE(
    score::FactoryList_QtInterface, FactoryList_QtInterface_iid)
