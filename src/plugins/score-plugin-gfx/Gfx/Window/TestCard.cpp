#include "TestCard.hpp"

#include <QPainter>

namespace Gfx
{
QImage renderTestCard(int w, int h)
{
  if(w < 1 || h < 1)
    return {};

  QImage img(w, h, QImage::Format_RGB32);
  img.fill(Qt::black);
  QPainter p(&img);

  const double cx = w * 0.5;
  const double cy = h * 0.5;
  const int gridStep = qMax(20, qMin(w, h) / 20);
  const double dim = qMin(w, h);

  // Layer 1: Checkerboard background
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    QColor light(60, 60, 60);
    QColor dark(30, 30, 30);
    
    QFont font = p.font();
    font.setPixelSize(qMax(9, gridStep / 4));
    p.setFont(font);

    for(int y = 0; y < h; y += gridStep)
      for(int x = 0; x < w; x += gridStep)
      {
        bool even = ((x / gridStep) + (y / gridStep)) % 2 == 0;
        p.fillRect(x, y, gridStep, gridStep, even ? dark : light);
        
        int col = x / gridStep;
        int row = y / gridStep;
        char letter = 'A' + (col % 26);
        QString text = QString("%1%2").arg(letter).arg(row);
        
        QRect rect(x, y, gridStep, gridStep);
        
        p.setRenderHint(QPainter::Antialiasing, true);
        
        p.setPen(QColor(0, 0, 0, 40)); 
        p.drawText(rect.translated(1, 1), Qt::AlignCenter, text);
        
        p.setPen(QColor(255, 255, 255, 40)); 
        p.drawText(rect, Qt::AlignCenter, text);
        
        p.setRenderHint(QPainter::Antialiasing, false);
      }
  }

  // Layer 2: Gradient bars — symmetric, centered
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    // Horizontal gradient bar (centered, 60% width, at ~70% height)
    int barH = qMax(8, (int)(h * 0.03));
    int barW = (int)(w * 0.6);
    int barX = (w - barW) / 2;
    int barY = (int)(h * 0.70);
    // Black-to-white-to-black
    for(int x = 0; x < barW; ++x)
    {
      double t = (double)x / qMax(1, barW - 1); // 0..1
      double v = t < 0.5 ? t * 2.0 : (1.0 - t) * 2.0;
      int gray = (int)(v * 255);
      p.setPen(QColor(gray, gray, gray));
      p.drawLine(barX + x, barY, barX + x, barY + barH - 1);
    }

    // Vertical gradient bar (centered, 60% height, at ~70% width from left, mirrored at 30%)
    int vBarW = qMax(8, (int)(w * 0.02));
    int vBarH = (int)(h * 0.6);
    int vBarY = (h - vBarH) / 2;
    int vBarX1 = (int)(w * 0.30) - vBarW / 2;
    int vBarX2 = (int)(w * 0.70) - vBarW / 2;
    for(int y = 0; y < vBarH; ++y)
    {
      double t = (double)y / qMax(1, vBarH - 1);
      double v = t < 0.5 ? t * 2.0 : (1.0 - t) * 2.0;
      int gray = (int)(v * 255);
      p.setPen(QColor(gray, gray, gray));
      p.drawLine(vBarX1, vBarY + y, vBarX1 + vBarW - 1, vBarY + y);
      p.drawLine(vBarX2, vBarY + y, vBarX2 + vBarW - 1, vBarY + y);
    }
  }

  // Layer 3: Color bars — rainbow strip centered horizontally, below center
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    const QColor colors[]
        = {QColor(255, 0, 0),   QColor(255, 128, 0), QColor(255, 255, 0),
           QColor(128, 255, 0), QColor(0, 255, 0),   QColor(0, 255, 128),
           QColor(0, 255, 255), QColor(0, 128, 255), QColor(0, 0, 255),
           QColor(128, 0, 255), QColor(255, 0, 255), QColor(255, 0, 128)};
    int nColors = 12;
    int stripW = (int)(w * 0.5);
    int stripH = qMax(8, (int)(h * 0.04));
    int stripX = (w - stripW) / 2;
    int stripY = (int)(h * 0.62);
    int cellW = stripW / nColors;
    for(int i = 0; i < nColors; i++)
    {
      int x0 = stripX + i * cellW;
      int x1 = (i == nColors - 1) ? stripX + stripW : stripX + (i + 1) * cellW;
      p.fillRect(x0, stripY, x1 - x0, stripH, colors[i]);
    }

    // Vertical color bars on left and right sides (mirrored)
    int vStripH = (int)(h * 0.5);
    int vStripW = qMax(8, (int)(w * 0.025));
    int vStripY = (h - vStripH) / 2;
    int vStripXL = (int)(w * 0.10);
    int vStripXR = (int)(w * 0.90) - vStripW;
    int cellH = vStripH / nColors;
    for(int i = 0; i < nColors; i++)
    {
      int y0 = vStripY + i * cellH;
      int y1 = (i == nColors - 1) ? vStripY + vStripH : vStripY + (i + 1) * cellH;
      p.fillRect(vStripXL, y0, vStripW, y1 - y0, colors[i]);
      p.fillRect(vStripXR, y0, vStripW, y1 - y0, colors[i]);
    }
  }

  p.setRenderHint(QPainter::Antialiasing, true);

  // Layer 4: Circles centered on the image
  {
    p.setBrush(Qt::NoBrush);
    QPen circlePen(QColor(200, 200, 200, 120), qMax(1.0, dim * 0.002));
    p.setPen(circlePen);
    // Large circle (80% of min dimension)
    double r1 = dim * 0.40;
    p.drawEllipse(QPointF(cx, cy), r1, r1);
    // Medium circle (50%)
    double r2 = dim * 0.25;
    p.drawEllipse(QPointF(cx, cy), r2, r2);
    // Small circle (20%)
    double r3 = dim * 0.10;
    p.drawEllipse(QPointF(cx, cy), r3, r3);
  }

  // Layer 5: Diagonals (corner to corner) + center cross
  {
    QPen diagPen(QColor(120, 120, 120, 100), qMax(1.0, dim * 0.001));
    p.setPen(diagPen);
    p.drawLine(0, 0, w - 1, h - 1);
    p.drawLine(w - 1, 0, 0, h - 1);

    // Dashed center cross
    QPen crossPen(QColor(200, 200, 200, 160), qMax(1.0, dim * 0.001));
    crossPen.setStyle(Qt::DashLine);
    p.setPen(crossPen);
    p.drawLine(0, h / 2, w - 1, h / 2);
    p.drawLine(w / 2, 0, w / 2, h - 1);
  }

  // Layer 6: Grid lines
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen gridPen(QColor(100, 100, 100, 80), 1);
    p.setPen(gridPen);
    for(int x = 0; x <= w; x += gridStep)
      p.drawLine(x, 0, x, h - 1);
    for(int y = 0; y <= h; y += gridStep)
      p.drawLine(0, y, w - 1, y);
  }

  // Layer 7: Crosshairs every 200px
  {
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen tickPen(QColor(180, 180, 180), 1);
    p.setPen(tickPen);
    int tickLen = qMax(4, (int)(dim * 0.008));
    for(int y = 0; y < h; y += gridStep * 5)
      for(int x = 0; x < w; x += gridStep * 5)
      {
        p.drawLine(x - tickLen, y, x + tickLen, y);
        p.drawLine(x, y - tickLen, x, y + tickLen);
      }
  }

  p.setRenderHint(QPainter::Antialiasing, true);

  // Layer 8: "ossia score" branding — above center, balanced around center line
  {
    QFont f(QStringLiteral("Ubuntu"), 1);
    f.setPixelSize(qMax(16, (int)(dim * 0.06)));
    f.setWeight(QFont::Bold);
    p.setFont(f);
    p.setPen(QColor(0x03, 0xc3, 0xdd));

    QFontMetrics fm(f);
    double gap = fm.horizontalAdvance(QChar(' ')) * 0.6;
    double wOssia = fm.horizontalAdvance(QStringLiteral("ossia"));
    double wScore = fm.horizontalAdvance(QStringLiteral("score"));
    int textY = (int)(cy - dim * 0.40);
    int textH = (int)(dim * 0.10);

    // Draw "ossia" to the left of center, "score" to the right
    double xOssia = cx - gap * 0.5 - wOssia;
    double xScore = cx + gap * 0.5;
    QRectF rOssia(xOssia, textY, wOssia, textH);
    QRectF rScore(xScore, textY, wScore, textH);
    p.drawText(rOssia, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("ossia"));
    p.drawText(rScore, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("score"));
  }

  // Layer 9: Resolution label — below center
  {
    QFont f(QStringLiteral("Ubuntu"), 1);
    f.setPixelSize(qMax(10, (int)(dim * 0.025)));
    p.setFont(f);
    p.setPen(QColor(200, 200, 200));
    QString label = QStringLiteral("%1 \u00d7 %2").arg(w).arg(h);
    QRect textRect(0, (int)(cy - dim * 0.31), w, (int)(dim * 0.06));
    p.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, label);
  }

  return img;
}
}
