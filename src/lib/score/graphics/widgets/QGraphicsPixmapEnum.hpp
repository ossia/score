#pragma once
#include <score/graphics/widgets/QGraphicsEnum.hpp>
#include <score/graphics/widgets/Constants.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QObject>
#include <QGraphicsItem>
#include <verdigris>
#include <vector>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsPixmapEnum final : public QGraphicsEnum
{
public:
    std::vector<QPixmap> on_images;
    std::vector<QPixmap> off_images;

    template <std::size_t N>
    QGraphicsPixmapEnum(
            const std::array<const char*, N>& arr,
            const std::array<const char*, 2 * N>& pixmaps,
            QGraphicsItem* parent)
        : QGraphicsPixmapEnum{parent}
    {
        array.reserve(N);
        for (auto str : arr)
            array.push_back(str);

        for (std::size_t i = 0; i < pixmaps.size(); i++)
        {
            if (i % 2)
                off_images.emplace_back(score::get_pixmap(pixmaps[i]));
            else
                on_images.emplace_back(score::get_pixmap(pixmaps[i]));
        }
    }

    QGraphicsPixmapEnum(
            std::vector<QString> arr,
            const std::vector<QString>& pixmaps,
            QGraphicsItem* parent)
        : QGraphicsPixmapEnum{parent}
    {
        array = std::move(arr);

        for (std::size_t i = 0; i < pixmaps.size(); i++)
        {
            if (i % 2)
                off_images.emplace_back(score::get_pixmap(pixmaps[i]));
            else
                on_images.emplace_back(score::get_pixmap(pixmaps[i]));
        }
    }

    QGraphicsPixmapEnum(QGraphicsItem* parent);

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};
}
