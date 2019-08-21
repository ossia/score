#include "Model.hpp"

#include <QSettings>

#include <wobjectimpl.h>
W_OBJECT_IMPL(RemoteControl::Settings::Model)
namespace RemoteControl
{
namespace Settings
{

namespace Parameters
{
SETTINGS_PARAMETER_IMPL(Enabled){QStringLiteral("RemoteControl/Enabled"),
                                 false};
static auto list()
{
  return std::tie(Enabled);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(bool, Model, Enabled)
}
}
