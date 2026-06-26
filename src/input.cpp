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

    if (m_snapActive) {
        updateSelection(event->position);
    }

    return false;
}

bool Effect::filterPointerButton(KWin::PointerButtonEvent *event)
{
    if (!event) {
        return false;
    }

    if (event->button == Qt::LeftButton || event->button == Qt::RightButton) {
        log(QStringLiteral("input_filter_pointer_button button=%1 state=%2 native=%3 pos=%4,%5 buttons=%6 drag_active=%7 snap_active=%8 drag_window=%9 snap_window=%10")
                .arg(describeButton(event->button),
                     describeButtonState(event->state))
                .arg(event->nativeButton)
                .arg(event->position.x(), 0, 'f', 1)
                .arg(event->position.y(), 0, 'f', 1)
                .arg(describeButtons(event->buttons),
                     m_dragWindow ? QStringLiteral("true") : QStringLiteral("false"),
                     m_snapActive ? QStringLiteral("true") : QStringLiteral("false"),
                     describeWindow(m_dragWindow),
                     describeWindow(m_snapWindow)));
    }

    if (event->button == Qt::RightButton && event->state == KWin::PointerButtonState::Released && m_suppressNextRightRelease) {
        m_suppressNextRightRelease = false;
        log(QStringLiteral("right_release_consumed_after_snap_trigger"));
        return true;
    }

    if (m_snapActive) {
        if (event->button == Qt::LeftButton && event->state == KWin::PointerButtonState::Released) {
            finishSelection(event->position, QStringLiteral("left_release"));
            return false;
        }

        if (event->button == Qt::RightButton && event->state == KWin::PointerButtonState::Pressed) {
            finishSelection(event->position, QStringLiteral("right_press"));
            m_suppressNextRightRelease = true;
            return true;
        }

        return false;
    }

    if (event->button == Qt::RightButton && event->state == KWin::PointerButtonState::Pressed) {
        m_sawRightPress = true;
        const bool leftReported = event->buttons.testFlag(Qt::LeftButton);
        log(QStringLiteral("selection_trigger button=right drag_window=%1 left_reported=%2 buttons=%3")
                .arg(describeWindow(m_dragWindow),
                     leftReported ? QStringLiteral("true") : QStringLiteral("false"),
                     describeButtons(event->buttons)));
        if (m_dragWindow && beginSelection(event->position)) {
            m_suppressNextRightRelease = true;
            return true;
        }
        if (!m_dragWindow) {
            log(QStringLiteral("selection_trigger_ignored reason=no_drag_window"));
        }
    }

    return false;
}

void Effect::wireWindow(KWin::EffectWindow *window)
{
    if (!window || m_wiredWindows.contains(window)) {
        return;
    }

    m_wiredWindows.insert(window);
    connect(window, &KWin::EffectWindow::windowStartUserMovedResized, this, &Effect::onMoveResizeStarted);
    connect(window, &KWin::EffectWindow::windowFinishUserMovedResized, this, &Effect::onMoveResizeFinished);
    log(QStringLiteral("wire_window window=%1").arg(describeWindow(window)));
}

void Effect::unwireWindow(KWin::EffectWindow *window)
{
    if (!window || !m_wiredWindows.remove(window)) {
        return;
    }

    if (m_snapWindow == window) {
        cancelSelection(QStringLiteral("snap_window_removed"));
    }

    if (m_dragWindow == window) {
        log(QStringLiteral("drag_window_removed window=%1").arg(describeWindow(window)));
        m_dragWindow.clear();
    }

    disconnect(window, nullptr, this, nullptr);
    log(QStringLiteral("unwire_window window=%1").arg(describeWindow(window)));
}

void Effect::onMoveResizeStarted(KWin::EffectWindow *window)
{
    m_dragWindow = window;
    m_sawRightPress = false;

    const QString mode = window && window->isUserResize()
        ? QStringLiteral("resize")
        : QStringLiteral("move");

    log(QStringLiteral("drag_start mode=%1 window=%2 geometry=%3 cursor=%4,%5")
            .arg(mode,
                 describeWindow(window),
                 window ? describeRect(window->frameGeometry()) : QStringLiteral("<none>"))
            .arg(KWin::effects->cursorPos().x(), 0, 'f', 1)
            .arg(KWin::effects->cursorPos().y(), 0, 'f', 1));
}

void Effect::onMoveResizeFinished(KWin::EffectWindow *window)
{
    log(QStringLiteral("drag_finish window=%1 geometry=%2 saw_right=%3 snap_active=%4")
            .arg(describeWindow(window),
                 window ? describeRect(window->frameGeometry()) : QStringLiteral("<none>"),
                 m_sawRightPress ? QStringLiteral("true") : QStringLiteral("false"),
                 m_snapActive ? QStringLiteral("true") : QStringLiteral("false")));

    if (window == m_dragWindow) {
        m_dragWindow.clear();
    }

    if (window == m_pendingSnapWindow) {
        schedulePendingSnap(QStringLiteral("native_drag_finish"));
    }
}

} // namespace Tiles
