#pragma once
#include <iscore/tools/Metadata.hpp>
#include <QObject>

namespace Process
{
class ProcessFactory;
class StateProcessFactory;
}

#define PROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
 OBJECTKEY_METADATA(Export, Model, ObjectKey) \
 UUID_METADATA(Export, Process::ProcessFactory, Model, Uuid) \
 TR_TEXT_METADATA(Export, Model, PrettyName_k, PrettyName)

#define STATEPROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
 OBJECTKEY_METADATA(Export, Model, ObjectKey) \
 UUID_METADATA(Export, Process::StateProcessFactory, Model, Uuid) \
 TR_TEXT_METADATA(Export, Model, PrettyName_k, PrettyName)
