#include "effect.h"

#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "input_event.h"

namespace Tiles
{

bool Effect::filterPointerMotion(KWin::PointerMotionEvent *event)
{
    if (!event) {
        return false;
    }

    if (m_selection) {
        updateSelection(event->position);
        return true;
    }

    return false;
}

bool Effect::filterPointerButton(KWin::PointerButtonEvent *event)
{
    if (!event) {
        return false;
    }

    if (event->button == Qt::RightButton && event->state == KWin::PointerButtonState::Released && m_suppressNextRightRelease) {
        m_suppressNextRightRelease = false;
        return true;
    }

    if (m_selection) {
        if (event->button == Qt::LeftButton && event->state == KWin::PointerButtonState::Released) {
            finishSelection(event->position);
            return false;
        }

        if (event->button == Qt::RightButton && event->state == KWin::PointerButtonState::Pressed) {
            finishSelection(event->position);
            m_suppressNextRightRelease = true;
            return true;
        }

        return false;
    }

    if (event->button == Qt::RightButton && event->state == KWin::PointerButtonState::Pressed) {
        if (m_dragWindow && beginSelection(event->position)) {
            m_suppressNextRightRelease = true;
            return true;
        }
    }

    return false;
}

void Effect::wireWindow(KWin::EffectWindow *window)
{
    if (!window) {
        return;
    }

    connect(window, &KWin::EffectWindow::windowStartUserMovedResized, this, &Effect::onMoveResizeStarted, Qt::UniqueConnection);
    connect(window, &KWin::EffectWindow::windowStepUserMovedResized, this, &Effect::onMoveResizeStepped, Qt::UniqueConnection);
    connect(window, &KWin::EffectWindow::windowFinishUserMovedResized, this, &Effect::onMoveResizeFinished, Qt::UniqueConnection);
}

void Effect::unwireWindow(KWin::EffectWindow *window)
{
    if (!window) {
        return;
    }

    if (m_selection && m_selection->window == window) {
        cancelSelection();
    }

    if (m_dragWindow == window) {
        m_dragWindow.clear();
    }

    disconnect(window, nullptr, this, nullptr);
}

void Effect::onMoveResizeStarted(KWin::EffectWindow *window)
{
    m_dragWindow = window;
}

void Effect::onMoveResizeStepped(KWin::EffectWindow *window, const KWin::RectF &)
{
    if (m_selection && m_selection->window == window) {
        moveWindowToSelection();
        KWin::effects->addRepaintFull();
    }
}

void Effect::onMoveResizeFinished(KWin::EffectWindow *window)
{
    if (window == m_dragWindow) {
        m_dragWindow.clear();
    }
}

} // namespace Tiles
