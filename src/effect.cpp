#include "effect.h"

#include "input_filter.h"

#include "effect/effecthandler.h"
#include "effect/effectwindow.h"

#include <KConfigGroup>

namespace Tiles
{

namespace
{

[[gnu::used]] const char BuildTag[] = "kwin-clicktile_build=1.0";

} // namespace

Effect::Effect()
{
    m_config = openKWinConfig();
    m_configWatcher = KConfigWatcher::create(m_config);
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group, const QByteArrayList &) {
        if (group.name() == effectConfigGroupName()) {
            reconfigure(ReconfigureAll);
        }
    });
    reloadConfig();

    m_inputFilter = std::make_unique<InputFilter>(this);
    KWin::input()->installInputEventFilter(m_inputFilter.get());

    connect(KWin::effects, &KWin::EffectsHandler::windowAdded, this, &Effect::wireWindow);
    connect(KWin::effects, &KWin::EffectsHandler::windowDeleted, this, &Effect::unwireWindow);
    connect(KWin::effects, &KWin::EffectsHandler::windowClosed, this, &Effect::unwireWindow);
    connect(KWin::effects, &KWin::EffectsHandler::screenRemoved, this, [this](KWin::LogicalOutput *screen) {
        if (m_selection && (m_selection->anchor.output == screen || m_selection->focus.output == screen)) {
            cancelSelection();
        }
    });

    const auto windows = KWin::effects->stackingOrder();
    for (KWin::EffectWindow *window : windows) {
        wireWindow(window);
    }
}

Effect::~Effect()
{
    cancelSelection();
    if (m_inputFilter) {
        KWin::input()->uninstallInputEventFilter(m_inputFilter.get());
        m_inputFilter.reset();
    }
}

bool Effect::supported()
{
    return true;
}

bool Effect::enabledByDefault()
{
    return false;
}

void Effect::reconfigure(ReconfigureFlags flags)
{
    KWin::Effect::reconfigure(flags);
    reloadConfig();
}

void Effect::reloadConfig()
{
    if (!m_config) {
        m_config = openKWinConfig();
    }
    m_config->reparseConfiguration();
    m_colors = readOverlayColors(m_config);
    if (m_selection) {
        if (m_selection->anchor.output) {
            m_selection->anchor.settings = settingsForOutput(m_selection->anchor.output);
        }
        if (m_selection->focus.output) {
            m_selection->focus.settings = settingsForOutput(m_selection->focus.output);
        }
    }
    updateOverlayViews();
}

bool Effect::isActive() const
{
    return m_selection.has_value();
}

bool Effect::blocksDirectScanout() const
{
    return m_selection.has_value();
}

} // namespace Tiles
