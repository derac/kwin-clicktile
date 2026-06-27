#include "effect.h"

#include "core/output.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/glvertexbuffer.h"

#include <QVector2D>

#include <array>
#include <algorithm>
#include <epoxy/gl.h>

namespace Tiles
{

namespace
{

QColor minimumAlpha(QColor color, qreal minimum)
{
    if (color.alpha() > 0) {
        color.setAlphaF(std::clamp(std::max<qreal>(color.alphaF(), minimum), 0.0, 1.0));
    }
    return color;
}

std::array<QVector2D, 6> verticesForRect(const KWin::RectF &rect)
{
    const float left = static_cast<float>(rect.left());
    const float top = static_cast<float>(rect.top());
    const float right = static_cast<float>(rect.right());
    const float bottom = static_cast<float>(rect.bottom());

    return {
        QVector2D(left, top),
        QVector2D(right, top),
        QVector2D(right, bottom),
        QVector2D(left, top),
        QVector2D(right, bottom),
        QVector2D(left, bottom),
    };
}

void restoreGlCapability(GLenum capability, GLboolean enabled)
{
    if (enabled) {
        glEnable(capability);
    } else {
        glDisable(capability);
    }
}

KWin::RectF mapOverlayRect(const KWin::RenderViewport &viewport, const KWin::RectF &rect)
{
    // KWin's effect projection matrix is built from the output's global scaled
    // render rect. Keep the global logical offset when scaling, otherwise
    // non-origin outputs draw at the wrong place.
    return rect.scaled(viewport.scale());
}

} // namespace

void Effect::prePaintScreen(KWin::ScreenPrePaintData &data)
{
    if (m_selection) {
        data.mask |= KWin::Effect::PAINT_SCREEN_TRANSFORMED;
    }

    KWin::effects->prePaintScreen(data);
}

void Effect::paintScreen(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::LogicalOutput *screen)
{
    KWin::effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);

    if (!m_selection || !screen || !shouldPaintOverlayForOutput(screen)) {
        return;
    }

    drawOverlay(viewport, screen);
}

void Effect::updateOverlayViews()
{
    if (m_selection) {
        KWin::effects->addRepaintFull();
    }
}

void Effect::drawOverlay(const KWin::RenderViewport &viewport, KWin::LogicalOutput *screen)
{
    if (!KWin::effects->isOpenGLCompositing()) {
        return;
    }

    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean stencilWasEnabled = glIsEnabled(GL_STENCIL_TEST);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

    GLint previousSrcRgb = GL_ONE;
    GLint previousDstRgb = GL_ZERO;
    GLint previousSrcAlpha = GL_ONE;
    GLint previousDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousDstAlpha);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);

    {
        KWin::ShaderBinder binder(KWin::ShaderTrait::UniformColor);
        binder.shader()->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
        drawGridGeometry(viewport, screen);
    }

    glBlendFuncSeparate(previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha);
    restoreGlCapability(GL_BLEND, blendWasEnabled);
    restoreGlCapability(GL_SCISSOR_TEST, scissorWasEnabled);
    restoreGlCapability(GL_DEPTH_TEST, depthWasEnabled);
    restoreGlCapability(GL_STENCIL_TEST, stencilWasEnabled);
    restoreGlCapability(GL_CULL_FACE, cullWasEnabled);
}

void Effect::drawGridGeometry(const KWin::RenderViewport &viewport, KWin::LogicalOutput *screen)
{
    const KWin::RectF area = workAreaForOutput(screen);
    if (area.isEmpty()) {
        return;
    }

    const qreal line = std::max<qreal>(1.0, 1.0 / viewport.scale());
    const QColor gridColor = minimumAlpha(m_colors.gridColor, 0.14);
    const QColor selectionColor = minimumAlpha(m_colors.selectionColor, 0.08);
    const QColor borderColor = minimumAlpha(m_colors.selectionBorderColor, 0.24);

    if (const auto rect = currentSelectionRect()) {
        drawGlRect(viewport, *rect, selectionColor);
        drawGlRect(viewport, KWin::RectF(rect->left(), rect->top(), rect->width(), line * 2.0), borderColor);
        drawGlRect(viewport, KWin::RectF(rect->left(), rect->bottom() - line * 2.0, rect->width(), line * 2.0), borderColor);
        drawGlRect(viewport, KWin::RectF(rect->left(), rect->top(), line * 2.0, rect->height()), borderColor);
        drawGlRect(viewport, KWin::RectF(rect->right() - line * 2.0, rect->top(), line * 2.0, rect->height()), borderColor);
    }

    drawGlRect(viewport, KWin::RectF(area.left(), area.top(), area.width(), line), gridColor);
    drawGlRect(viewport, KWin::RectF(area.left(), area.bottom() - line, area.width(), line), gridColor);
    drawGlRect(viewport, KWin::RectF(area.left(), area.top(), line, area.height()), gridColor);
    drawGlRect(viewport, KWin::RectF(area.right() - line, area.top(), line, area.height()), gridColor);

    OutputSettings settings = settingsForOutput(screen);
    if (m_selection) {
        if (screen == m_selection->anchor.output) {
            settings = m_selection->anchor.settings;
        } else if (screen == m_selection->focus.output) {
            settings = m_selection->focus.settings;
        }
    }
    const TileGrid grid = sanitizeGrid(settings.grid.columns, settings.grid.rows);

    for (int column = 1; column < grid.columns; ++column) {
        const qreal x = area.left() + area.width() * column / grid.columns;
        drawGlRect(viewport, KWin::RectF(x - line / 2.0, area.top(), line, area.height()), gridColor);
    }

    for (int row = 1; row < grid.rows; ++row) {
        const qreal y = area.top() + area.height() * row / grid.rows;
        drawGlRect(viewport, KWin::RectF(area.left(), y - line / 2.0, area.width(), line), gridColor);
    }
}

void Effect::drawGlRect(const KWin::RenderViewport &viewport, const KWin::RectF &rect, const QColor &color)
{
    if (rect.isEmpty() || color.alpha() == 0) {
        return;
    }

    KWin::GLShader *shader = KWin::ShaderManager::instance()->getBoundShader();
    if (!shader) {
        return;
    }

    shader->setUniform(KWin::GLShader::ColorUniform::Color, color);

    KWin::GLVertexBuffer *vbo = KWin::GLVertexBuffer::streamingBuffer();
    vbo->reset();
    const KWin::RectF mappedRect = mapOverlayRect(viewport, rect);
    if (mappedRect.isEmpty()) {
        return;
    }
    const auto vertices = verticesForRect(mappedRect);
    vbo->setVertices(vertices);
    vbo->render(GL_TRIANGLES);
}

} // namespace Tiles
