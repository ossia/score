/*
XINX
Copyright Ulrich Van Den Hekke, (2007-2011)
xinx@shadoware.org

Ce logiciel est un programme informatique servant à éditer les feuilles
de styles.

Ce logiciel est régi par la licence CeCILL soumise au droit français et
respectant les principes de diffusion des logiciels libres. Vous pouvez
utiliser, modifier et/ou redistribuer ce programme sous les conditions
de la licence CeCILL telle que diffusée par le CEA, le CNRS et l'INRIA
sur le site "http://www.cecill.info".

En contrepartie de l'accessibilité au code source et des droits de copie,
de modification et de redistribution accordés par cette licence, il n'est
offert aux utilisateurs qu'une garantie limitée.  Pour les mêmes raisons,
seule une responsabilité restreinte pèse sur l'auteur du programme,  le
titulaire des droits patrimoniaux et les concédants successifs.

A cet égard  l'attention de l'utilisateur est attirée sur les risques
associés au chargement,  à l'utilisation,  à la modification et/ou au
développement et à la reproduction du logiciel par l'utilisateur étant
donné sa spécificité de logiciel libre, qui peut le rendre complexe à
manipuler et qui le réserve donc à des développeurs et des professionnels
avertis possédant  des  connaissances  informatiques approfondies.  Les
utilisateurs sont donc invités à charger  et  tester  l'adéquation  du
logiciel à leurs besoins dans des conditions permettant d'assurer la
sécurité de leurs systèmes et ou de leurs données et, plus généralement,
à l'utiliser et l'exploiter dans les mêmes conditions de sécurité.

Le fait que vous puissiez accéder à cet en-tête signifie que vous avez
pris connaissance de la licence CeCILL, et que vous en avez accepté les
termes.

== Note JM : cette license couvre les trois fichiers
qmenuview.h qmenuview.cpp, qmenuview_p.h.
J'ai apporté quelques modifications en plus.
*/

#include <QAbstractItemModel>
#include <QAction>
#include <QEvent>
#include <QIcon>
#include <QMenu>
#include <qnamespace.h>
#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QVariant>
#include <QMouseEvent>

#include <Device/QMenuView/qmenuview.h>
#include "qmenuview_p.h"

class QWidget;

Q_DECLARE_METATYPE(QModelIndex)


void ClickableMenu::mouseReleaseEvent(QMouseEvent* event)
{

    QAction* const actionAtEvent = actionAt(event->pos());

    if(actionAtEvent)
    {
        actionAtEvent->trigger();
    }

    QMenu::mouseReleaseEvent(event);
}



/* QMenuViewPrivate */

QMenuViewPrivate::QMenuViewPrivate(QMenuView* menu) : _menu(menu)
{
}

QAction* QMenuViewPrivate::makeAction(const QModelIndex& index)
{
    QIcon icon = qvariant_cast<QIcon> (index.data(Qt::DecorationRole));
    QAction* action = new QAction(icon, index.data().toString(), this);
    action->setEnabled(index.flags().testFlag(Qt::ItemIsEnabled));
    QVariant v;
    v.setValue(index);
    action->setData(v);

    return action;
}

void QMenuViewPrivate::triggered(QAction* action)
{
    QVariant v = action->data();

    if(v.canConvert<QModelIndex>())
    {
        QModelIndex idx = qvariant_cast<QModelIndex> (v);
        emit _menu->triggered(idx);
    }
}

void QMenuViewPrivate::hovered(QAction* action)
{
    QVariant v = action->data();

    if(v.canConvert<QModelIndex>())
    {
        QModelIndex idx = qvariant_cast<QModelIndex> (v);
        QString hoveredString = idx.data(Qt::StatusTipRole).toString();

        if(!hoveredString.isEmpty())
        {
            emit _menu->hovered(hoveredString);
        }
    }
}

void QMenuViewPrivate::aboutToShow()
{
    QMenu* menu = qobject_cast<QMenu*> (sender());

    if(menu)
    {
        QVariant v = menu->menuAction()->data();

        if(v.canConvert<QModelIndex>())
        {
            QModelIndex idx = qvariant_cast<QModelIndex> (v);
            _menu->createMenu(idx, *menu, menu);
            disconnect(menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShow()));
            return;
        }
    }

    _menu->clear();

    if(_menu->prePopulated())
    {
        _menu->addSeparator();
    }

    _menu->createMenu(m_root, *_menu, _menu);

    _menu->postPopulated();
}

/* QMenuView */

/*!
 * \ingroup Components
 * \class QMenuView
 * \since 0.9.0.0
 *
 * \brief The QMenuView provides a menu based view on a QAbstractItemModel class.
 *
 * \bc 0.10.0.0
 *
 * This class is used to transform a hierarchical model based on the class
 * QAbstractItemModel into a menu. It can be used to create an action menu, history,
 * or snipets menu.
 *
 * \image html qmenuview.png
 * \image latex qmenuview.png
 *
 * When the model is defined, the structure of the menu is automatically generated. This
 * class ignores call to QAbstractItemModel::beginInsertRows() and QAbstractItemModel::endInsertRows().
 * Menu is generated when the user opens it.
 */

/*!
 * \brief Creates the new menu view based on a QMenu object.
 * \param parent The parent object of the menu.
 */
QMenuView::QMenuView(QWidget* parent) :
    ClickableMenu(parent),
    d(new QMenuViewPrivate(this))
{
    connect(this, SIGNAL(triggered(QAction*)), d.data(), SLOT(triggered(QAction*)));
    connect(this, SIGNAL(hovered(QAction*)), d.data(), SLOT(hovered(QAction*)));
    connect(this, SIGNAL(aboutToShow()), d.data(), SLOT(aboutToShow()));
}

//! Destroy the menu.
QMenuView::~QMenuView()
{
    setModel(nullptr);
}

/*!
 * \fn void QMenuView::hovered(const QString &text) const
 * \brief The signal when a menu action is highlighted.
 *
 * \p text is the Qt::StatusTipRole of the index that caused the signal to be emitted.
 *
 * Often this is used to update status information.
 *
 * \sa triggered()
 */

/*!
 * \fn void QMenuView::triggered(const QModelIndex & index) const
 * \brief This signal is emitted when an action in this menu is triggered.
 *
 * \p index is the index's action that caused the signal to be emitted.
 *
 * \sa hovered()
 */

//! Add any actions before the tree, return true if any actions are added.
bool QMenuView::prePopulated()
{
    return false;
}

//! Add any actions after the tree
void QMenuView::postPopulated()
{
}

/*!
 * \brief Set the new model to \p model.
 * \param model The new model to use for the creation of menus.
 */
void QMenuView::setModel(QAbstractItemModel* model)
{
    d->m_model = model;
}

/*!
 * \brief Return the current model of the menu.
 */
QAbstractItemModel* QMenuView::model() const
{
    return d->m_model;
}

/*!
 * \brief Change the root index to \p index.
 *
 * This can be used to show only a part of the QAbstractItemModel.
 * \param index The index to use to show the menu. if QModelIndex(), all the model is show.
 */
void QMenuView::setRootIndex(const QModelIndex& index)
{
    d->m_root = index;
}

/*!
 * \brief Returns the current root index.
 *
 * Default root index is QModelIndex()
 */
QModelIndex QMenuView::rootIndex() const
{
    return d->m_root;
}

//! Puts all of the children of parent into menu
void QMenuView::createMenu(const QModelIndex& parent, QMenu& parentMenu, QMenu* menu)
{
    if(! menu)
    {
        QIcon icon = qvariant_cast<QIcon> (parent.data(Qt::DecorationRole));

        QVariant v;
        v.setValue(parent);

        menu = new ClickableMenu(parent.data().toString(), this);
        menu->setIcon(icon);
        parentMenu.addMenu(menu);
        menu->menuAction()->setData(v);

        auto act = d->makeAction(parent);
        act->setParent(menu);
        connect(menu->menuAction(), &QAction::triggered,
                [ = ]()
        {
            act->trigger();
        });

        menu->setEnabled(true);
        //menu->setEnabled(parent.flags().testFlag(Qt::ItemIsEnabled));

        connect(menu, SIGNAL(aboutToShow()), d.data(), SLOT(aboutToShow()));

        return;
    }

    int end = d->m_model->rowCount(parent);

    for(int i = 0; i < end; ++i)
    {
        QModelIndex idx = d->m_model->index(i, 0, parent);

        if(d->m_model->hasChildren(idx))
        {
            createMenu(idx, *menu);
        }
        else
        {
            menu->addAction(d->makeAction(idx));
        }
    }
}

