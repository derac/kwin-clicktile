#pragma once

#include "input.h"

namespace Tiles
{

class Effect;

class InputFilter final : public KWin::InputEventFilter
{
public:
    explicit InputFilter(Effect *effect);

    bool pointerMotion(KWin::PointerMotionEvent *event) override;
    bool pointerButton(KWin::PointerButtonEvent *event) override;

private:
    Effect *m_effect = nullptr;
};

} // namespace Tiles
