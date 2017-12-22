
#include <iostream>
#include <aeffect.h>
#include <aeffectx.h>
#include <QWindow>
#include <Media/Effect/VST/VSTEffectModel.hpp>
auto setup_rect(QWindow* container,  uint16_t width, uint16_t height)
{
  container->setMinimumHeight(height);
  container->setMaximumHeight(height);
  container->setHeight(height);
  container->setMinimumWidth(width);
  container->setMaximumWidth(width);
  container->setWidth(width);
  container->setBaseSize({width, height});
}

void show_vst2_editor(AEffect& effect, ERect rect)
{
  auto width = rect.right - rect.left;
  auto height = rect.bottom - rect.top;

  auto container = reinterpret_cast<QWindow*>(reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui);
  if(container)
  {
    effect.dispatcher(&effect, effEditOpen, 0, 0, (void*)container->winId(), 0);
    container->show();
    setup_rect(container, width, height);
    return;
  }
  container = new QWindow;

  effect.dispatcher(&effect, effEditOpen, 0, 0, (void*)container->winId(), 0);
  container->show();
  setup_rect(container, width, height);
  reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui = reinterpret_cast<VstIntPtr>(container);
}

void hide_vst2_editor(AEffect& effect)
{
  effect.dispatcher(&effect, effEditClose, 0, 0, nullptr, 0);
  if(effect.resvd1)
  {
    auto container = reinterpret_cast<QWindow*>(reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui);
    if(container)
    {
      container->close();
      container->deleteLater();
    }
    reinterpret_cast<Media::VST::VSTEffectModel*>(effect.resvd1)->ui = 0;
  }
}
