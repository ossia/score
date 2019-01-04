#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/TimeValue.hpp>
#include <Sequence/SequenceModel.hpp>
#include <Sequence/SequencePresenter.hpp>
#include <Sequence/SequenceView.hpp>

#include <QByteArray>
#include <QString>
namespace Process
{
class LayerPresenter;
class LayerView;
class ProcessModel;
}
class QGraphicsItem;
class QObject;
struct VisitorVariant;

namespace Sequence
{
class EditionSettings;

using SequenceFactory = Process::ProcessFactory_T<Sequence::ProcessModel>;
using SequenceLayerFactory = Process::LayerFactory_T<
Sequence::ProcessModel, Sequence::SequencePresenter, Sequence::SequenceView>;
}
