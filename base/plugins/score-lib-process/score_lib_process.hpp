#pragma once

/**
 * \namespace Process
 * \brief Base classes and tools to implement processes and layers
 *
 * This library provides the base building blocks for the processes in score.
 * Examples of processes are Automation::ProcessModel, Scenario::ProcessModel,
 * Mapping::ProcessModel, etc.
 *
 * The corresponding class in the OSSIA API is ossia::time_process.
 * It is only used for execution.
 *
 * The main class is Process::ProcessModel : it contains all the data necessary
 * to create, save, reload
 * a process, as well as signals and slots to notify of changes.
 *
 * All processes in score are contained in a Scenario::IntervalModel.
 *
 * When modifying a process (or any other model class) from the UI, it is
 * **MANDATORY** to use a Command for elements that are part of the data model,
 * except for very specific cases : for instance, the execution percentage,
 * since
 * it is not part of the save.
 *
 * See for instance the addon tutorial
 * (<https://github.com/OSSIA/score-addon-tutorial>).
 *
 * The UI works as follows :
 * * Process::ProcessModel is the base class for processes.
 * * Process::LayerPresenter / Process::LayerView do the actual displaying,
 * currently with Qt's Graphics View framework.
 * * A process can have an inspector widget.
 *
 * Two different implementations of layers are provided :
 * * One that only displays the name of the process, useful when starting an
 * implementation.
 * * One that uses a QWidget for showing information (Process::WidgetLayer).
 *
 * This plug-in also contains :
 * * Styling information for the central view of score, with the
 * Process::ScenarioStyle class.
 * * Utility classes to display a process in the process panel (provided with
 * Scenario::PanelDelegate).
 * * The implementation of a tree of messages, with Process::MessageNode, to be
 * used in Scenario::StateModel.
 *
 * score provides extensions for processes through \ref score::Component%s.
 * Available extensions are :
 * * Execution of the process, given through
 * Engine::Execution::ProcessComponent.
 * * Local tree, given through Engine::LocalTree::ProcessComponent.
 *
 */
