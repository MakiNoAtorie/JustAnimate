#ifndef MOVEMODE_H
#define MOVEMODE_H


enum class MoveMode {
    MIDDLE,
    TOPLEFT,
    TOPRIGHT,
    BOTTOMLEFT,
    BOTTOMRIGHT,
    ROTATION,
    ROTATIONLEFT,
    ROTATIONRIGHT,
    SYMMETRY,
    REPOSITION,
    PERSP_SINGLE,
    PERSP_LEFT,
    PERSP_RIGHT,
    PERSP_MIDDLE,
    CENTER,
    SKEW_HORIZONTAL, // top or bottom edge handle
    SKEW_VERTICAL,   // left or right edge handle
    NONE
};

#endif // MOVEMODE_H
