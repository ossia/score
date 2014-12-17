#pragma once

struct EventData
{
    int eventClickedId{0};  // id of the clicked event
    int x{0};  // use for : position of the mouse realeased point in event coordinate
    int y{0};  // idem
    double relativeY{0.0}; // y scaled with current scenario
};
