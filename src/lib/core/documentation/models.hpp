#pragma once

/*! \page Models
 *
 * score provides ways to create hierarchical data models,
 * based on an entity-component paradigm similar to game engines.
 *
 * \section Organization
 * The objects in score are organized in a hierarchical tree :
 *
 * \code
 * Application -> score::DocumentModel -> score::DocumentPlugin -> [... objects
 * of the document plugin ... ]
 *                            \
 *                        score::DocumentDelegateModel
 *                              \
 *                         [... objects of the document ...]
 * \endcode
 *
 * This tree is based on Qt's \ref QObject. See
 * http://doc.qt.io/qt-5/object.html for more information. <br> This applies to
 * all the "meaningful" objects in the software, i.e. the objects of the score
 * domain (Process::ProcessModel, Scenario::IntervalModel,
 * Automation::ProcessModel, etc). <br> We call these the "model" objects. <br>
 * Model objects are all identified uniquely across the children of their
 * parents. <br> E.g. the following case is possible :
 *
 * \code
 * Parent.1             Parent.2
 *   |    \                |
 *  Obj.1  Obj.2         Obj.1
 * \endcode
 *
 * The following case is not possible :
 * \code
 * Parent.1             Parent.2
 *   |    \                |
 *  Obj.1  Obj.1         Obj.1
 * \endcode
 *
 * \section IdentificationObjects Identification of objects
 * \subsection Identifiers
 * Objects are identified by the couple `Name.Identifier` (see
 * ObjectIdentifier). The name comes from the object's `objectName()` property.
 * The identifier comes from the object's `id()` property.
 * <br>
 * The numeric identifier of an object is a template class
 * parametrized by the object's type (see id_base_t and \ref Id).
 * <br>
 * This approach then allows us to have paths to objects, by chaining
 * ObjectIdentifier%s together. This is necessary for the \ref Commands system,
 * for instance.
 *
 * \subsection Paths
 * Paths are a list of identifiers that lead from the root of the
 * score::Document, to the actual object we are looking for. <br> There are two
 * variants :
 * * ObjectPath is weakly typed.
 * * Path is a strongly typed wrapper over ObjectPath, like Id. It is the one
 * to be used 90% of the time.
 *
 * Paths allow to get a serializable reference to a specific entity in the
 * object hierarchy.
 *
 * This is necessary for undo-redo commands.
 * Take the following case :
 * * An object is created by the user by drag'n'drop.
 * * A Command is instantiated for this action and applied with
 * score::Command::redo.
 * * During `redo()`, the object is allocated, inserted in the hierarchy, etc.
 * * Then, the object is moved : a new Command is created, with a Path to the
 * moved object.
 * * Then, the user undoes everything : the move is undone, as well as the
 * creation of the object.
 * * At this point, the object has been `delete`d : its memory has been freed
 * and it is not available anymore. If any Command or other object had a
 * pointer on this object, doing anything with this pointer would crash.
 * * Then, the user decides to redo everything. A **new** object is created
 * during the redoing of the first command.
 * * The second command is redone : since it had a Path to the object and not a
 * pointer, it is able to find it instead of crashing, even though memory-wise,
 * it is not the **same** object that was created initially.
 *
 * \section CreatingModels Creating models
 *
 * Base classes for custom model objects are provided :
 * * IdentifiedObject : provides identification.
 * * Entity<> : provides identification, \ref Components and \ref
 * score::ModelMetadata.
 *
 * \section ItemModel Relationship to Qt's item models
 * The previous information is separate from Qt's model-view paradigm which is
 * more useful when one wants to see a tree of objects in a tree widget. <br>
 * This means that it does not apply to the "small" objects in the various \ref
 * QAbstractItemModel child classes, such as the nodes in the tree
 * (Device::Node). <br> This is done mostly for performance & memory usage
 * reasons, and because it would not be really useful for these cases. <br> For
 * these objects, paths can still be saved with the TreePath class which is a
 * simpler list of integers. <br> A base "Tree" QAbstractItemModel
 * implementation is provided with TreeNodeItemModel, with the nodes of the
 * tree are based on TreeNode.
 *
 *
 * \see Serialization
 */
