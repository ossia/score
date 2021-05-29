#pragma once

/*! \mainpage
 *
 * Welcome to the score code documentation.
 * <br><br>
 * Here is the documentation of core concepts used throughout the score code
 * base. <br> All the following concepts are sowewhat interdependents, hence
 * reading everything twice may be useful to get a clear mental picture. <br>
 * * \ref Models
 * * \ref PluginsFactoriesAndInterfaces
 * * \ref ScoreInterfaces
 * * \ref Contexts
 * * \ref Commands
 * * \ref Serialization
 * * \ref ModelViewPresenter
 * * \ref StateMachines
 * * \ref CodingStyle
 * <br><br>
 * Documentation of specific plug-ins and modules of the software:
 * <br>
 * * \ref State
 * * \ref Process
 * * \ref Device
 * * \ref Execution
 * * \ref LocalTree
 * * \ref Scenario
 * * \ref Curve
 * * \ref Automation
 * * \ref Gfx
 * <br><br>
 * Guides for writing custom plug-ins:
 * <br>
 * * \ref GfxPlugins
 * <br>
 * To contribute to score, it can also be useful to have a look at the
 * tutorial plug-in : https://github.com/ossia/score-addon-tutorial
 */

/*! \namespace score
 * \brief Base toolkit upon which the software is built.
 *
 * This namespace contains only non-domain specific classes
 * and utilities : serialization, model-view, documents, etc.
 *
 * It is split in two folders :
 *
 * * `core` is the internal mechanic to set-up the software : the actual widget
 * classes, the plug-in loading code, etc.
 * * `score` is the "public" part of the score API : this code can be used by
 * plug-ins.
 */
