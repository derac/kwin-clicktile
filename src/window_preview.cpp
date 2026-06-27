#include "effect.h"

#include "effect/effectwindow.h"
#include "window.h"

namespace Tiles
{

void Effect::endNativeDragForSelection()
{
    if (!m_snapWindow || !m_snapWindow->window()) {
        return;
    }

    KWin::Window *window = m_snapWindow->window();
    if (!window->isInteractiveMove() && !window->isInteractiveResize()) {
        return;
    }

    window->endInteractiveMoveResize();
    const bool stillInteractiveAfterEnd = window->isInteractiveMove() || window->isInteractiveResize();
    if (stillInteractiveAfterEnd) {
        window->cancelInteractiveMoveResize();
    }

    if (m_dragWindow == m_snapWindow) {
        m_dragWindow.clear();
    }
}

void Effect::moveWindowToSelection()
{
    if (!m_snapActive || !m_snapWindow) {
        return;
    }

    const auto target = currentSelectionRect();
    if (!target || target->isEmpty()) {
        return;
    }

    if (!m_snapWindow->window()) {
        return;
    }

    KWin::Window *window = m_snapWindow->window();
    window->setMaximize(false, false);
    window->setQuickTileMode(KWin::QuickTileMode{KWin::QuickTileFlag::None}, target->center());
    window->moveResize(*target);
}

} // namespace Tiles
