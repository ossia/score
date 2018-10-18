#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/selection/Selection.hpp>

namespace score
{
struct ObjectRemover
    : public score::InterfaceBase
{
  SCORE_INTERFACE(ObjectRemover, "12951ea1-ffb0-4f77-8a3a-bf28ccb60a2e")
  virtual ~ObjectRemover() override
  {

  }

  virtual bool remove(const Selection& s, QObject* focus) = 0;
};

class SCORE_LIB_BASE_EXPORT ObjectRemoverList final
    : public score::InterfaceList<ObjectRemover>
{
};

}
