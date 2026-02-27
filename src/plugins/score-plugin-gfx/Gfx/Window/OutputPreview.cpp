#include "OutputPreview.hpp"

#include <Gfx/Window/TestCard.hpp>

#include <QPainter>
#include <QScreen>
#include <QWindow>

namespace Gfx
{

static QColor previewColorForIndex(int index)
{
  static constexpr QColor colors[] = {
      QColor(30, 60, 180),  QColor(180, 30, 30),  QColor(30, 150, 30),
      QColor(180, 150, 30), QColor(150, 30, 150), QColor(30, 150, 150),
      QColor(200, 100, 30), QColor(100, 30, 200),
  };
  return colors[index % 8];
}

PreviewWidget::PreviewWidget(int index, PreviewContent content, QWidget* parent)
    : QWidget{parent, Qt::Window | Qt::WindowStaysOnTopHint}
    , m_index{index}
    , m_content{content}
{
  setWindowTitle(QStringLiteral("Output %1").arg(index));
}

void PreviewWidget::setOutputIndex(int idx)
{
  m_index = idx;
  setWindowTitle(QStringLiteral("Output %1").arg(idx));
  update();
}

void PreviewWidget::setPreviewContent(PreviewContent mode)
{
  m_content = mode;
  update();
}

void PreviewWidget::setOutputResolution(QSize sz)
{
  m_resolution = sz;
  update();
}

void PreviewWidget::setBlend(
    EdgeBlend left, EdgeBlend right, EdgeBlend top, EdgeBlend bottom)
{
  m_blendLeft = left;
  m_blendRight = right;
  m_blendTop = top;
  m_blendBottom = bottom;
  update();
}

void PreviewWidget::setSourceRect(QRectF rect)
{
  m_sourceRect = rect;
  update();
}

void PreviewWidget::setCornerWarp(const CornerWarp& warp)
{
  m_cornerWarp = warp;
  update();
}

void PreviewWidget::setGlobalTestCard(const QImage& img)
{
  m_globalTestCard = img;
  update();
}

void PreviewWidget::mouseDoubleClickEvent(QMouseEvent*)
{
  if(isFullScreen())
    showNormal();
  else
    showFullScreen();

  if(onFullscreenToggled)
    onFullscreenToggled(m_index, isFullScreen());
}

// Build gradient stops approximating pow(1-t, gamma) opacity curve
static void setGammaGradientStops(QLinearGradient& grad, float gamma)
{
  constexpr int steps = 16;
  constexpr int maxAlpha = 180;
  for(int i = 0; i <= steps; i++)
  {
    double t = (double)i / steps;            // 0 = edge, 1 = interior
    double alpha = std::pow(1.0 - t, gamma); // 1 at edge, 0 at interior
    grad.setColorAt(t, QColor(0, 0, 0, (int)(alpha * maxAlpha)));
  }
}

static void paintBlendGradients(
    QPainter& p, const QRect& r, const EdgeBlend& left, const EdgeBlend& right,
    const EdgeBlend& top, const EdgeBlend& bottom)
{
  p.setPen(Qt::NoPen);

  if(left.width > 0.f)
  {
    double w = left.width * r.width();
    QLinearGradient grad(r.left(), r.center().y(), r.left() + w, r.center().y());
    setGammaGradientStops(grad, left.gamma);
    p.setBrush(QBrush(grad));
    p.drawRect(QRectF(r.left(), r.top(), w, r.height()));
  }

  if(right.width > 0.f)
  {
    double w = right.width * r.width();
    QLinearGradient grad(r.right(), r.center().y(), r.right() - w, r.center().y());
    setGammaGradientStops(grad, right.gamma);
    p.setBrush(QBrush(grad));
    p.drawRect(QRectF(r.right() - w, r.top(), w, r.height()));
  }

  if(top.width > 0.f)
  {
    double h = top.width * r.height();
    QLinearGradient grad(r.center().x(), r.top(), r.center().x(), r.top() + h);
    setGammaGradientStops(grad, top.gamma);
    p.setBrush(QBrush(grad));
    p.drawRect(QRectF(r.left(), r.top(), r.width(), h));
  }

  if(bottom.width > 0.f)
  {
    double h = bottom.width * r.height();
    QLinearGradient grad(r.center().x(), r.bottom(), r.center().x(), r.bottom() - h);
    setGammaGradientStops(grad, bottom.gamma);
    p.setBrush(QBrush(grad));
    p.drawRect(QRectF(r.left(), r.bottom() - h, r.width(), h));
  }
}

void PreviewWidget::paintEvent(QPaintEvent*)
{
  QPainter p(this);
  p.fillRect(rect(), Qt::black);

  // Apply corner warp transform if non-identity
  if(!m_cornerWarp.isIdentity())
  {
    double w = width(), h = height();
    QPolygonF src;
    src << QPointF(0, 0) << QPointF(w, 0) << QPointF(w, h) << QPointF(0, h);
    QPolygonF dst;
    dst << QPointF(m_cornerWarp.topLeft.x() * w, m_cornerWarp.topLeft.y() * h)
        << QPointF(m_cornerWarp.topRight.x() * w, m_cornerWarp.topRight.y() * h)
        << QPointF(m_cornerWarp.bottomRight.x() * w, m_cornerWarp.bottomRight.y() * h)
        << QPointF(m_cornerWarp.bottomLeft.x() * w, m_cornerWarp.bottomLeft.y() * h);

    QTransform t;
    if(QTransform::quadToQuad(src, dst, t))
      p.setTransform(t);
  }

  switch(m_content)
  {
    case PreviewContent::Black:
      p.fillRect(rect(), Qt::black);
      return;

    case PreviewContent::PerOutputTestCard: {
      QImage card = renderTestCard(m_resolution.width(), m_resolution.height());
      p.drawImage(rect(), card);
      paintBlendGradients(
          p, rect(), m_blendLeft, m_blendRight, m_blendTop, m_blendBottom);
      return;
    }

    case PreviewContent::GlobalTestCard: {
      if(!m_globalTestCard.isNull())
      {
        QRectF srcPixels(
            m_sourceRect.x() * m_globalTestCard.width(),
            m_sourceRect.y() * m_globalTestCard.height(),
            m_sourceRect.width() * m_globalTestCard.width(),
            m_sourceRect.height() * m_globalTestCard.height());
        p.drawImage(QRectF(rect()), m_globalTestCard, srcPixels);
      }
      else
      {
        p.fillRect(rect(), Qt::black);
      }
      paintBlendGradients(
          p, rect(), m_blendLeft, m_blendRight, m_blendTop, m_blendBottom);
      return;
    }

    case PreviewContent::OutputIdentification: {
      p.fillRect(rect(), previewColorForIndex(m_index));
      paintBlendGradients(
          p, rect(), m_blendLeft, m_blendRight, m_blendTop, m_blendBottom);

      p.setPen(Qt::white);

      {
        QFont f = p.font();
        f.setPixelSize(qMax(32, qMin(width(), height()) / 3));
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, QString::number(m_index));
      }

      {
        QFont f = p.font();
        f.setPixelSize(qMax(14, qMin(width(), height()) / 12));
        f.setBold(false);
        p.setFont(f);
        auto text = QStringLiteral("%1 x %2")
                        .arg(m_resolution.width())
                        .arg(m_resolution.height());
        QRect lower = rect();
        lower.setTop(rect().center().y() + qMin(width(), height()) / 5);
        p.drawText(lower, Qt::AlignHCenter | Qt::AlignTop, text);
      }
      return;
    }
  }
}

OutputPreviewWindows::OutputPreviewWindows(QObject* parent)
    : QObject{parent}
{
}

OutputPreviewWindows::~OutputPreviewWindows()
{
  closeAll();
}

void OutputPreviewWindows::closeAll()
{
  qDeleteAll(m_windows);
  m_windows.clear();
}

void OutputPreviewWindows::syncToMappings(const std::vector<OutputMapping>& mappings)
{
  // Ensure global test card exists
  if(m_globalTestCard.isNull())
    rebuildGlobalTestCard();

  const int newCount = (int)mappings.size();
  const int oldCount = (int)m_windows.size();

  // Remove excess windows
  while((int)m_windows.size() > newCount)
  {
    delete m_windows.back();
    m_windows.pop_back();
  }

  // Add new windows
  for(int i = oldCount; i < newCount; ++i)
  {
    auto* pw = new PreviewWidget(i, m_content);
    pw->onFullscreenToggled = [this](int idx, bool fs) {
      if(onFullscreenToggled)
        onFullscreenToggled(idx, fs);
    };
    m_windows.push_back(pw);
  }

  // Update positions/sizes/screens for all windows
  const auto& screens = qApp->screens();
  for(int i = 0; i < newCount; ++i)
  {
    auto* pw = m_windows[i];
    const auto& m = mappings[i];

    pw->setOutputResolution(m.windowSize);
    pw->setSourceRect(m.sourceRect);
    pw->setBlend(m.blendLeft, m.blendRight, m.blendTop, m.blendBottom);
    pw->setCornerWarp(m.cornerWarp);
    if(!m_globalTestCard.isNull())
      pw->setGlobalTestCard(m_globalTestCard);

    if(m_syncPositions)
    {
      // Determine target screen
      QScreen* targetScreen = nullptr;
      if(m.screenIndex >= 0 && m.screenIndex < screens.size())
        targetScreen = screens[m.screenIndex];

      if(m.fullscreen)
      {
        // On Windows, showFullScreen() can default to the primary monitor
        // unless the widget is already positioned within the target screen.
        if(targetScreen)
        {
          if(auto* wh = pw->windowHandle())
          {
            if(wh->screen() != targetScreen)
              wh->setScreen(targetScreen);
          }
          pw->setGeometry(targetScreen->geometry());
        }
        if(!pw->isFullScreen())
          pw->showFullScreen();
      }
      else
      {
        if(pw->isFullScreen())
          pw->showNormal();

        if(targetScreen)
        {
          if(auto* wh = pw->windowHandle())
          {
            if(wh->screen() != targetScreen)
              wh->setScreen(targetScreen);
          }
        }

        // Only reposition/resize if actually different to avoid jitter
        if(pw->pos() != m.windowPosition)
          pw->move(m.windowPosition);
        if(pw->size() != m.windowSize)
          pw->resize(m.windowSize);
      }
    }

    if(!pw->isVisible())
    {
      pw->show();
      pw->raise();
    }
  }
}

void OutputPreviewWindows::setSyncPositions(bool sync)
{
  m_syncPositions = sync;
}

void OutputPreviewWindows::setPreviewContent(PreviewContent mode)
{
  m_content = mode;
  for(auto* pw : m_windows)
    pw->setPreviewContent(mode);
}

void OutputPreviewWindows::setInputResolution(QSize sz)
{
  if(m_inputResolution != sz)
  {
    m_inputResolution = sz;
    rebuildGlobalTestCard();
  }
}

void OutputPreviewWindows::rebuildGlobalTestCard()
{
  m_globalTestCard
      = renderTestCard(m_inputResolution.width(), m_inputResolution.height());
  for(auto* pw : m_windows)
    pw->setGlobalTestCard(m_globalTestCard);
}

}
