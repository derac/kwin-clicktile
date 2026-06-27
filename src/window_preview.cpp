#include "effect.h"

#include "effect/effectwindow.h"
#include "window.h"

namespace Tiles
{

bool Effect::endNativeDragForSelection(KWin::EffectWindow *effectWindow)
{
    QPointer<KWin::EffectWindow> trackedWindow = effectWindow;
    if (!trackedWindow || !trackedWindow->window()) {
        return false;
    }

    KWin::Window *window = trackedWindow->window();
    if (!window->isInteractiveMove() || window->isInteractiveResize()) {
        return false;
    }

    window->endInteractiveMoveResize();
    if (!trackedWindow || !trackedWindow->window()) {
        return false;
    }

    window = trackedWindow->window();
    if (window->isInteractiveMove() || window->isInteractiveResize()) {
        window->cancelInteractiveMoveResize();
    }

    if (!trackedWindow || !trackedWindow->window()) {
        return false;
    }

    window = trackedWindow->window();
    if (window->isInteractiveMove() || window->isInteractiveResize()) {
        return false;
    }

    if (m_dragWindow == trackedWindow) {
        m_dragWindow.clear();
    }

    return true;
}

void Effect::moveWindowToSelection()
{
    if (!m_selection || !m_selection->window) {
        return;
    }

    const auto target = currentSelectionRect();
    if (!target || target->isEmpty()) {
        return;
    }

    if (!m_selection->window->window()) {
        return;
    }

    KWin::Window *window = m_selection->window->window();
    window->setMaximize(false, false);
    window->setQuickTileMode(KWin::QuickTileMode{KWin::QuickTileFlag::None}, target->center());
    window->moveResize(*target);
}

} // namespace Tiles
