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

#pragma once
// Qt header
#include <QMenu>
#include <QAbstractItemModel>

#include "qmenuview.h"

//! \private
class QMenuViewPrivate : public QObject
{
	Q_OBJECT
public:
	QMenuViewPrivate(QMenuView * menu);
	virtual ~QMenuViewPrivate() = default;

	QAction *makeAction(const QModelIndex &index);

	QMenuView * _menu{};
	QAbstractItemModel * m_model{};
	QPersistentModelIndex m_root;
public slots:
	void aboutToShow();
	void triggered(QAction *action);
	void hovered(QAction *action);
};
