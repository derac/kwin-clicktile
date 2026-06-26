#include "input_filter.h"

#include "effect.h"

namespace Tiles
{

InputFilter::InputFilter(Effect *effect)
    : KWin::InputEventFilter(KWin::InputFilterOrder::Effects)
    , m_effect(effect)
{
}

bool InputFilter::pointerMotion(KWin::PointerMotionEvent *event)
{
    return m_effect ? m_effect->filterPointerMotion(event) : false;
}

bool InputFilter::pointerButton(KWin::PointerButtonEvent *event)
{
    return m_effect ? m_effect->filterPointerButton(event) : false;
}

} // namespace Tiles
