#pragma once

template<typename Layout>
class MarginLess : public Layout
{
    public:
        MarginLess()
        {
            this->setContentsMargins(0, 0, 0, 0);
            this->setSpacing(0);
        }
};
