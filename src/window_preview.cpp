#include "effect.h"

#include "effect/effectwindow.h"
#include "window.h"

namespace Tiles
{

bool Effect::endNativeDragForSelection(KWin::EffectWindow *effectWindow)
{
    QPointer<KWin::EffectWindow> trackedWindow = effectWindow;
    auto nativeWindow = [&trackedWindow]() -> KWin::Window * {
        return trackedWindow ? trackedWindow->window() : nullptr;
    };
    auto nativeDragFinished = [](KWin::Window *window) {
        return window && !window->isInteractiveMove() && !window->isInteractiveResize();
    };

    KWin::Window *window = nativeWindow();
    if (!window || !window->isInteractiveMove() || window->isInteractiveResize()) {
        return false;
    }

    window->endInteractiveMoveResize();
    window = nativeWindow();
    if (!window) {
        return false;
    }

    if (!nativeDragFinished(window)) {
        window->cancelInteractiveMoveResize();
        window = nativeWindow();
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
