#pragma once

/*! \page ScoreInterfaces Useful score interfaces
 *
 * This page lists useful score interfaces to use when building a custom plug-in.
 *
 * Basic interfaces :
 * * score::InterfaceBase : custom polymorphic interfaces, the base class of all interfaces mentioned here.
 * * score::GUIApplicationPlugin : used to store app-level objects and data.
 *  * Notable example: Curve::ApplicationPlugin handles curve edition global keyboard shortcuts.
 *  * Notable example: Audio::ApplicationPlugin creates and manages the audio engine.
 * * score::DocumentPlugin : used to store document-level objects and data. For global per-document data that needs to be saved, for instance.
 *   * Notable example: RemoteControl::DocumentPlugin : exposes a document over WebSockets.
 * * score::PanelDelegate : used to add custom panels to the software.
 *   * Notable example: JS::PanelDelegate : adds a JavaScript scripting console.
 * * score::Command : custom undo-redo commands.
 *   * Simple example: Process::ChangePortAddress.
 *   * Average example: Midi::ChangeNotesVelocity.
 *   * Advanced example: Scenario::Command::RemoveProcessFromInterval.
 *   * See also score::PropertyCommand and the PROPERTY_COMMAND_T macro which makes that automatic for simple properties.
 *
 * Interfaces used to implement processes (like automations, etc.) :
 * * Process::ProcessModel : data model for processes.
 *  * Simple example: Media::Step::Model
 *  * Average example: Midi::ProcessModel
 *  * Advanced example: JS::ProcessModel ; if you want to embed a new programming language in score, this is the one to check.
 *  * See also Process::ProcessModelFactory
 * * Process::LayerPresenter and Process::LayerView : custom user interface for processes.
 *  * Simple example: Media::Step::Presenter / Media::Step::View
 *  * Average example: Midi::Presenter / Midi::View
 *  * Advanced example: Scenario::ScenarioPresenter / Scenario::ScenarioView
 *  * See also Process::LayerFactory
 * * Execution::ProcessComponent : going from the score data model to the libossia execution engine.
 *  * Example: Execution::StepComponent
 * * LocalTree::ProcessComponent : exposing processes properties as OSC keys that can be remote-controlled.
 *  * Example: LocalTree::AutomationComponent
 * * RemoteControl::ProcessComponent : exposing processes properties as a custom WebSocket API.
 *  * Example: ControlSurface::Remote
 *
 * Note that if you want to make a simple process with only fixed controls, inputs and outputs,
 * a much simpler API exists, in score-plugin-fx.
 *  * Check out Nodes::Gain::Node for a simple gain processor.
 *  * Check out Nodes::LFO::Node for a more advanced example with a custom UI.
 *
 * Interfaces used to implement video processors (like shader filter, video, image) :
 * * score::gfx::Node : data model for graphics renderers
 *  * Example: TexgenNode
 * * score::gfx::NodeRenderer : actual renderer
 *  * Example: TexgenNode::Rendered
 *
 * Interfaces used to implement protocols and devices (like OSC, Art-Net...) :
 * * Device::DeviceInterface : the actual device implementation.
 *  * Example: Protocols::ArtnetDevice / Protocols::ArtnetProtocolFactory
 *  * See also Device::ProtocolFactory : the factory, which will also create widgets, etc.
 *
 * Interfaces relative to media processing:
 * * Video::VideoInterface : generic interface for decoding video frame by frame.
 *  * Example: Video::CameraInput ; Video::VideoDecoder
 * * GPUVideoDecoder : code specific for decoding a kind of video frame on the GPU, e.g. YUV420, HAP...
 *  * Example: RGB0Decoder
 *
 * Other useful interfaces:
 * * Library::LibraryInterface : used to add handling of a custom data type to the user library.
 *   For instance for making a custom file type visible in the user library, adding a new format of audio plug-in...
 *   * Example: Faust::LibraryHandler
 *   * Example: Media::Sound::LibraryHandler
 *
 * * Process::ProcessDropHandler : used to handle custom data types when a drag'n'drop occurs.
 *   For instance for adding support to dropping a custom file type in a score.
 *   * Example: Faust::DropHandler
 *   * Example: Midi::DropHandler
 *
 * * Inspector::InspectorWidgetBase : creating custom inspectors.
 */
