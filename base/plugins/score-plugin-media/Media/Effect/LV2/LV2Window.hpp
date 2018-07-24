#pragma once
#include <QDialog>
#include <Media/Effect/LV2/LV2EffectModel.hpp>
#include <wobjectdefs.h>

namespace Media::LV2
{

class Window final : public QDialog
{
  W_OBJECT(Window)
public:
  Window(
      const LV2EffectModel& e,
      const score::DocumentContext& ctx,
      QWidget* parent);

  ~Window() override;
  //void resize(int w, int h);

  void uiClosing() W_SIGNAL(uiClosing);

  static bool is_resizable(LilvWorld* world, const LilvUI& ui);
private:
//  static void setup_rect(QWidget* container, int width, int height);

  void resizeEvent(QResizeEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  const LV2EffectModel& effect;
  QWidget* m_widget{};
};

using LayerFactory = Process::
    EffectLayerFactory_T<LV2EffectModel, Media::Effect::DefaultEffectItem, Window>;
}
