module;
#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/CpuGeneratorNode.hpp>
#include <Crousti/ExecutorPortSetup.hpp>
#include <Crousti/ExecutorUpdateControlValueInUi.hpp>
#include <Crousti/File.hpp>
#include <Crousti/GpuComputeNode.hpp>
#include <Crousti/GpuNode.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/Metadatas.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/context.hpp>

#include <ossia-qt/invoke.hpp>

#include <QGuiApplication>

#if SCORE_PLUGIN_GFX
#include <Crousti/GpuNode.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#endif

#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>

#include <Control/Layout.hpp>
#include <Crousti/Attributes.hpp>
#include <Crousti/Concepts.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/Metadata.hpp>
#include <Crousti/Metadatas.hpp>
#include <Crousti/Painter.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Crousti/ProcessModelPortInit.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Dataflow/ControlInletItem.hpp>
#include <Dataflow/ControlOutletItem.hpp>
#include <Dataflow/PortItem.hpp>
#include <Effect/EffectLayer.hpp>

#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/layouts/GraphicsGridLayout.hpp>
#include <score/graphics/layouts/GraphicsSplitLayout.hpp>
#include <score/graphics/layouts/GraphicsTabLayout.hpp>
#include <score/graphics/widgets/QGraphicsLineEdit.hpp>
#include <score/graphics/widgets/QGraphicsRangeSlider.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>
#include <score/tools/ThreadPool.hpp>

#include <ossia/detail/for_each.hpp>
#include <ossia/detail/type_if.hpp>
#include <ossia/detail/typelist.hpp>

#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/vector.hpp>
#include <boost/pfr.hpp>
#include <boost/predef.h>

#include <QTimer>

#include <avnd/binding/ossia/data_node.hpp>
#include <avnd/binding/ossia/dynamic_ports.hpp>
#include <avnd/binding/ossia/mono_audio_node.hpp>
#include <avnd/binding/ossia/node.hpp>
#include <avnd/binding/ossia/ossia_audio_node.hpp>
#include <avnd/binding/ossia/poly_audio_node.hpp>
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/enum_reflection.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/common/function_reflection.hpp>
#include <avnd/common/span_polyfill.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/layout.hpp>
#include <avnd/concepts/temporality.hpp>
#include <avnd/concepts/ui.hpp>
#include <avnd/concepts/worker.hpp>
#include <avnd/introspection/generic.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/bus_host_process_adapter.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <tuplet/tuple.hpp>

#include <cmath>
#include <score_plugin_engine.hpp>
#include <smallfun.hpp>

#include <algorithm>
#include <array>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <ratio>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <version>

export module halp;

#define HALP_MODULE_BUILD 1
#include <halp/attributes.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/controls_fmt.hpp>
#include <halp/curve.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/device.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/fft.hpp>
#include <halp/file_port.hpp>
#include <halp/geometry.hpp>
#include <halp/gradient_port.hpp>
#include <halp/inline.hpp>
#include <halp/layout.hpp>
#include <halp/log.hpp>
#include <halp/mappers.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/reactive_value.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <halp/schedule.hpp>
#include <halp/shared_instance.hpp>
#include <halp/smooth_controls.hpp>
#include <halp/smoothers.hpp>
#include <halp/soundfile_port.hpp>
#include <halp/static_string.hpp>
#include <halp/texture.hpp>
#include <halp/texture_formats.hpp>
#include <halp/value_types.hpp>

#include <halp/controls.basic.hpp>
#include <halp/controls.buttons.hpp>
#include <halp/controls.enums.hpp>
#include <halp/controls.sliders.hpp>
#include <halp/controls.typedefs.hpp>
