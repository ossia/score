#pragma once
#include <Process/Dataflow/DataflowObjects.hpp>
#include <QPointer>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QDialog>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQmlEngine>
namespace Dataflow
{


class DataflowWindow: public QObject
{
  Q_OBJECT
public:
  DataflowWindow()
  {
    window.setMinimumSize(600, 600);
    layout.addWidget(&menu);
    menu.setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    menu.setMaximumHeight(30);
    auto widg = QWidget::createWindowContainer(&view, &window);
    layout.addWidget(widg);
    view.setColor(QColor::fromHslF(0.65, 0.1, 0.3));
    widg->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // ImmediateGlutton, ImmediateStrict, DelayedGlutton, DelayedStrict
    a1->setCheckable(true);
    a2->setCheckable(true);
    a3->setCheckable(true);
    a4->setCheckable(true);

    auto acts = new QActionGroup{&menu};
    acts->addAction(a1);
    acts->addAction(a2);
    acts->addAction(a3);
    acts->addAction(a4);
    acts->setExclusive(true);
    menu.addAction(a1);
    menu.addAction(a2);
    menu.addAction(a3);
    menu.addAction(a4);

    connect(a1, &QAction::toggled, this, &DataflowWindow::on_typeChanged);
    connect(a2, &QAction::toggled, this, &DataflowWindow::on_typeChanged);
    connect(a3, &QAction::toggled, this, &DataflowWindow::on_typeChanged);
    connect(a4, &QAction::toggled, this, &DataflowWindow::on_typeChanged);
  }

  void cableSelected(Id<Process::Cable> con)
  {/*
    selected.push_back(&con);
    auto go = static_cast<CustomConnection*>(&con.getConnectionGraphicsObject());
    if(selected.size() == 1)
    {
      switch(go->cableType) {
        case Process::CableType::ImmediateGlutton: a1->setChecked(true); break;
        case Process::CableType::ImmediateStrict: a2->setChecked(true); break;
        case Process::CableType::DelayedGlutton: a3->setChecked(true); break;
        case Process::CableType::DelayedStrict: a4->setChecked(true); break;
        default: break;
      }
    }
    else
    {
      // TODO Set "half" check state for everyone
    }*/
  }



  void on_typeChanged()
  {
    if(a1->isChecked()) emit typeChanged(selected, Process::CableType::ImmediateGlutton);
    else if (a2->isChecked()) emit typeChanged(selected, Process::CableType::ImmediateStrict);
    else if (a3->isChecked()) emit typeChanged(selected, Process::CableType::DelayedGlutton);
    else if (a4->isChecked()) emit typeChanged(selected, Process::CableType::DelayedStrict);

  }

  QAction* a1 = new QAction{"Immediate Glutton"};
  QAction* a2 = new QAction{"Immediate Strict"};
  QAction* a3 = new QAction{"Delayed Glutton"};
  QAction* a4 = new QAction{"Delayed Strict"};

  QWidget window;
  QMenuBar menu;
  QQuickWindow view;

  QVBoxLayout layout{&window};
  QList<QQuickItem*> selected;

signals:
  void typeChanged(QList<QQuickItem*>, Process::CableType);

private:

};
}
