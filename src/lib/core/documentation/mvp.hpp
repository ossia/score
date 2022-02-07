#pragma once

/*! \page ModelViewPresenter Model-View-Presenter separation
 *
 * For "complex" objects that are not in QAbstractItemModel's scope (generally what lies in the QGraphicsScene), the pattern is as follows :
 *
 * http://www.planetgeek.ch/2009/04/08/passive-view-command-pvc-pattern/
 *
 * We try to build score in such a way that the Model and the View of each element are both entirely unaware of any other class than themselves and the elements they respectively contain.
 * They only send signals, and the Presenter's task is to connect the signals to relevant slots.
 *
 * More precisely :
 *  * When the user realizes a *semantic* action, like creating a new event+interval, the View sends a signal with the relevant information (where will the new event be created).
 *  * The element Presenter catches the signal, creates a relevant Command object and submits it.
 *  * The Command object is forwarded to the network via the network plug-in.
 *  * The Command object's redo() method is called. The Command *must* find the object on which it applies by itself, using ModelPath and identifiers, but not pointers (else it would not work through the network).
 *  * The Model is modified and sends relevant signals.
 *  * The Presenter catches the signals and updates the View accordingly.
 *
 */
