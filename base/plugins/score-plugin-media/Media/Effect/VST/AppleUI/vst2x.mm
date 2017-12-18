#include <AppKit/AppKit.h>
//#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CFBundle.h>
#include <iostream>
#include <QMacCocoaViewContainer>
#include <aeffect.h>
#include <QDialog>
#include "vstwindow.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QDebug>
#include <QMainWindow>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <QTimer>

QSize sizeHint(NSView* m_view) {

  QSize ret;

  if (m_view == 0) {
    ret = QSize(0, 0);
  }

  NSRect frame = [m_view frame];
  ret = QSize(frame.size.width, frame.size.height);

  return ret;

}


struct VSTDialog: public QDialog
{
    VSTDialog(QWidget* parent):
      QDialog{parent}
    {
    }

    void addWidg(QWidget* w)
    {
      auto lay = new QHBoxLayout(this);
      lay->setContentsMargins(0, 0, 0, 0);
      lay->addWidget(w);
    }
};
// TODO have a look at https://github.com/alex-weej/Evilnote/blob/master/vsteditorwidget.mm
void show_vst2_editor(AEffect& effect, ERect rect)
{
  auto container = reinterpret_cast<QMacCocoaViewContainer*>(reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui);
  if(container)
  {
    effect.dispatcher(&effect, effEditOpen, 0, 0, (void*)container->cocoaView(), 0);
    container->show();
    return;
  }
  //NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  //  NSView *view = [[NSView alloc] initWithFrame: NSMakeRect(0, 0, width, height)] ;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  qDebug() << rect.top << rect.left;
  auto diag = new VSTDialog{nullptr};
  diag->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
  diag->setWindowFlag(Qt::WindowMinimizeButtonHint, false);
  diag->setWindowFlag(Qt::WindowMaximizeButtonHint, false);
  id superview = [[::NSView alloc] initWithFrame: NSMakeRect(rect.top, rect.left, rect.right, rect.bottom)];

  effect.dispatcher(&effect, effEditOpen, 0, 0, (void*)superview, 0);

  ERect* vstRect{};

  effect.dispatcher(&effect, effEditGetRect, 0, 0, &vstRect, 0.0f);
  if(vstRect)
  {
    rect = *vstRect;
  }

  auto width = rect.right - rect.left;
  auto height = rect.bottom - rect.top;

  container = new QMacCocoaViewContainer{superview, diag};

  NSRect frame = NSMakeRect(rect.left, rect.top,
                            rect.right, rect.bottom);

  [superview setFrame:frame];
  container->resize(width, height);

  NSArray* subviews;
  subviews = [superview subviews];
  auto m_view = [[subviews objectAtIndex:0] retain];

  [[NSNotificationCenter defaultCenter] addObserverForName:@"NSViewFrameDidChangeNotification" object:m_view queue:nil usingBlock:^(NSNotification* notification) {
      Q_UNUSED(notification);

      //qDebug() << "adjust editor size to" << sizeHint();
      // need to adjust the superview frame to be the same as the view frame
      [superview setFrame:[m_view frame]];
      diag->setFixedSize(sizeHint(m_view));
      // adjust the size of the window to fit.
      // FIXME: this is indeed a bit dodgy ;)
      QApplication::processEvents();
      diag->window()->adjustSize();

    qDebug() << "got a notification" << sizeHint(m_view);
  }];

  //[superview setFrame:NSMakeRect(0, 0, width, height)];

  qDebug() << sizeHint(m_view);
  diag->setFixedSize(QSize(width, height));

  diag->show();
  container->setVisible(true);
  container->update();
  effect.dispatcher(&effect, __effEditTopDeprecated, 0, 0, 0, 0);

  reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui = reinterpret_cast<VstIntPtr>(container);
  [superview release];
  [pool release];
}

void hide_vst2_editor(AEffect& effect)
{
  if(effect.resvd1)
  {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    auto container = reinterpret_cast<QMacCocoaViewContainer*>(reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui);
    ((QDialog*)container->parent())->close();
    delete container->parent();
    reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui = 0;
    [pool release];
  }
  effect.dispatcher(&effect, effEditClose, 0, 0, nullptr, 0);
}
