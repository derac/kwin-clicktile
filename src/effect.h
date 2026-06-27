#pragma once

#include "settings.h"
#include "effect/effect.h"
#include "input.h"

#include <QPointer>
#include <memory>
#include <optional>

#include <KConfigWatcher>

namespace Tiles
{

class InputFilter;

struct Tile {
    int column = 0;
    int row = 0;

    bool operator==(const Tile &) const = default;
};

struct TileSelection {
    Tile anchor;
    Tile focus;

    bool operator==(const TileSelection &) const = default;
};

class Effect final : public KWin::Effect
{
    Q_OBJECT

public:
    Effect();
    ~Effect() override;

    static bool supported();
    static bool enabledByDefault();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(KWin::ScreenPrePaintData &data) override;
    void paintScreen(const KWin::RenderTarget &renderTarget,
                     const KWin::RenderViewport &viewport,
                     int mask,
                     const KWin::Region &deviceRegion,
                     KWin::LogicalOutput *screen) override;
    void postPaintScreen() override;
    bool isActive() const override;
    bool blocksDirectScanout() const override;

private:
    friend class InputFilter;

    void reloadConfig();

    // Input and native drag tracking.
    bool filterPointerMotion(KWin::PointerMotionEvent *event);
    bool filterPointerButton(KWin::PointerButtonEvent *event);
    void wireWindow(KWin::EffectWindow *window);
    void unwireWindow(KWin::EffectWindow *window);
    void onMoveResizeStarted(KWin::EffectWindow *window);
    void onMoveResizeStepped(KWin::EffectWindow *window, const KWin::RectF &geometry);
    void onMoveResizeFinished(KWin::EffectWindow *window);

    // Grid selection and window placement.
    bool beginSelection(const QPointF &point);
    void updateSelection(const QPointF &point);
    void finishSelection(const QPointF &point);
    void cancelSelection();
    void clearSelectionState();
    void endNativeDragForSelection();
    void moveWindowToSelection();

    // Overlay rendering.
    void updateOverlayViews();
    void drawOverlay(const KWin::RenderViewport &viewport, KWin::LogicalOutput *screen);
    void drawGlRect(const KWin::RenderViewport &viewport, const KWin::RectF &rect, const QColor &color);
    void drawGridGeometry(const KWin::RenderViewport &viewport, KWin::LogicalOutput *screen);

    // Output/grid helpers.
    KWin::LogicalOutput *outputForPoint(const QPointF &point) const;
    KWin::RectF workAreaForOutput(KWin::LogicalOutput *output) const;
    OutputSettings settingsForOutput(KWin::LogicalOutput *output) const;
    Tile cellAt(KWin::LogicalOutput *output, const OutputSettings &settings, const QPointF &point) const;
    KWin::RectF cellRectForOutput(KWin::LogicalOutput *output, const OutputSettings &settings, const Tile &cell) const;
    bool shouldPaintOverlayForOutput(KWin::LogicalOutput *output) const;
    std::optional<KWin::RectF> currentSelectionRect() const;

    QPointer<KWin::EffectWindow> m_dragWindow;
    QPointer<KWin::EffectWindow> m_snapWindow;
    QPointer<KWin::LogicalOutput> m_anchorOutput;
    QPointer<KWin::LogicalOutput> m_activeOutput;
    std::unique_ptr<InputFilter> m_inputFilter;
    KSharedConfigPtr m_config;
    KConfigWatcher::Ptr m_configWatcher;
    OverlayColors m_colors = defaultOverlayColors();
    OutputSettings m_anchorSettings;
    OutputSettings m_activeSettings;
    std::optional<TileSelection> m_selection;
    bool m_snapActive = false;
    bool m_suppressNextRightRelease = false;
};

} // namespace Tiles
