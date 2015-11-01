#pragma once

namespace iscore
{
/**
 * @brief The MarginLess class
 *
 * A mixin that removes the margin of a layout.
 */
template<typename Layout>
class MarginLess final : public Layout
{
    public:
        MarginLess()
        {
            this->setContentsMargins(0, 0, 0, 0);
            this->setSpacing(0);
        }
};
}
