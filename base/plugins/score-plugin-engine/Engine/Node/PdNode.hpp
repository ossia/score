#pragma once
#include <Engine/Node/Widgets.hpp>
#include <Engine/Node/Node.hpp>
#include <Engine/Node/Process.hpp>
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/Inspector.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <Process/GenericProcessFactory.hpp>
#define make_uuid(text) score::uuids::string_generator::compute((text))

namespace Process
{
template<typename Node>
struct Factories
{
    using Info = decltype(Node::info);
    using process = Process::ControlProcess<Node>;
    using process_factory = Process::GenericProcessModelFactory<process>;

    using executor = Process::Executor<Node>;
    using executor_factory = Engine::Execution::ProcessComponentFactory_T<executor>;

    using inspector = Process::InspectorWidget<Node>;
    using inspector_factory = Process::InspectorFactory<Node>;

    using layer_factory = ControlLayerFactory<Node>;
};
}

