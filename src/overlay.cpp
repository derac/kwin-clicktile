#include "effect.h"

#include "core/output.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/glvertexbuffer.h"

#include <QVector2D>
#include <QStringList>

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

QString glEnabled(GLboolean enabled)
{
    return enabled ? QStringLiteral("true") : QStringLiteral("false");
}

QString glErrorName(GLenum error)
{
    switch (error) {
    case GL_NO_ERROR:
        return QStringLiteral("none");
    case GL_INVALID_ENUM:
        return QStringLiteral("invalid_enum");
    case GL_INVALID_VALUE:
        return QStringLiteral("invalid_value");
    case GL_INVALID_OPERATION:
        return QStringLiteral("invalid_operation");
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return QStringLiteral("invalid_framebuffer_operation");
    case GL_OUT_OF_MEMORY:
        return QStringLiteral("out_of_memory");
    default:
        return QStringLiteral("0x%1").arg(static_cast<uint>(error), 0, 16);
    }
}

QString drainGlErrors()
{
    QStringList errors;
    for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
        errors << glErrorName(error);
    }
    return errors.isEmpty() ? QStringLiteral("none") : errors.join(QLatin1Char('|'));
}

QString glIntRect(GLenum name)
{
    GLint values[4] = {0, 0, 0, 0};
    glGetIntegerv(name, values);
    return QStringLiteral("%1,%2 %3x%4")
        .arg(values[0])
        .arg(values[1])
        .arg(values[2])
        .arg(values[3]);
}

GLint glInteger(GLenum name)
{
    GLint value = 0;
    glGetIntegerv(name, &value);
    return value;
}

void restoreGlCapability(GLenum capability, GLboolean enabled)
{
    if (enabled) {
        glEnable(capability);
    } else {
        glDisable(capability);
    }
}

QString transformName(KWin::OutputTransform transform)
{
    switch (transform.kind()) {
    case KWin::OutputTransform::Normal:
        return QStringLiteral("normal");
    case KWin::OutputTransform::Rotate90:
        return QStringLiteral("rotate90");
    case KWin::OutputTransform::Rotate180:
        return QStringLiteral("rotate180");
    case KWin::OutputTransform::Rotate270:
        return QStringLiteral("rotate270");
    case KWin::OutputTransform::FlipX:
        return QStringLiteral("flipx");
    case KWin::OutputTransform::FlipX90:
        return QStringLiteral("flipx90");
    case KWin::OutputTransform::FlipX180:
        return QStringLiteral("flipx180");
    case KWin::OutputTransform::FlipX270:
        return QStringLiteral("flipx270");
    }
    return QString::number(static_cast<int>(transform.kind()));
}

QString overlayMappingName()
{
    return QStringLiteral("texture_projection");
}

KWin::RectF mapOverlayRect(const KWin::RenderViewport &viewport, const KWin::RectF &rect)
{
    // RenderViewport::projectionMatrix() applies the render target transform.
    // Feed untransformed render-target texture coordinates, matching KWin's
    // built-in GL effects.
    return viewport.mapToRenderTargetTexture(rect);
}

} // namespace

void Effect::paintScreen(const KWin::RenderTarget &renderTarget,
                         const KWin::RenderViewport &viewport,
                         int mask,
                         const KWin::Region &deviceRegion,
                         KWin::LogicalOutput *screen)
{
    KWin::effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);

    if (!m_snapActive || !screen || screen != m_activeOutput || !m_selection) {
        return;
    }

    drawOverlay(renderTarget, viewport, screen, mask, deviceRegion);
}

void Effect::updateOverlayViews()
{
    if (m_snapActive) {
        KWin::effects->addRepaintFull();
    }
}

void Effect::drawOverlay(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport, KWin::LogicalOutput *screen, int mask, const KWin::Region &deviceRegion)
{
    if (!KWin::effects->isOpenGLCompositing()) {
        if (!m_loggedNoOverlayRenderer) {
            m_loggedNoOverlayRenderer = true;
            log(QStringLiteral("overlay_render_skip reason=non_opengl_compositor"));
        }
        return;
    }

    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean stencilWasEnabled = glIsEnabled(GL_STENCIL_TEST);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const bool logThisPaint = !m_loggedOverlayPaintForSelection;

    if (logThisPaint) {
        m_loggedOverlayPaintForSelection = true;
        const auto selection = currentSelectionRect();
        const QString selectionDescription = selection ? describeRect(*selection) : QStringLiteral("<none>");
        const QString drawSelectionDescription = selection ? describeRect(mapOverlayRect(viewport, *selection)) : QStringLiteral("<none>");
        const QString textureSelectionDescription = selection ? describeRect(viewport.mapToRenderTargetTexture(*selection)) : QStringLiteral("<none>");
        const QString targetSelectionDescription = selection ? describeRect(viewport.mapToRenderTarget(*selection)) : QStringLiteral("<none>");
        const QString deviceSelectionDescription = selection ? describeRect(viewport.mapToDeviceCoordinates(*selection)) : QStringLiteral("<none>");
        const QString damageDescription = deviceRegion.isEmpty() ? QStringLiteral("empty") : describeRect(KWin::RectF(deviceRegion.boundingRect()));
        log(QStringLiteral("overlay_paint_sample renderer=opengl draw_mapping=%1 screen=%2 work_area=%3 selection=%4 map_draw=%5 map_texture=%6 map_target=%7 map_device=%8 scale=%9 device=%10 render=%11 damage_device=%12 mask=0x%13 transform=%14 blend=%15 scissor=%16 depth=%17 stencil=%18 cull=%19 gl_viewport=%20 gl_scissor_box=%21 draw_fbo=%22 read_fbo=%23 program=%24")
                .arg(overlayMappingName(),
                     describeOutput(screen),
                     describeRect(workAreaForOutput(screen)),
                     selectionDescription,
                     drawSelectionDescription,
                     textureSelectionDescription,
                     targetSelectionDescription,
                     deviceSelectionDescription)
                .arg(viewport.scale(), 0, 'f', 2)
                .arg(describeRect(viewport.deviceRect()),
                     describeRect(viewport.renderRect()),
                     damageDescription,
                     QString::number(mask, 16),
                     transformName(viewport.transform()),
                     glEnabled(blendWasEnabled),
                     glEnabled(scissorWasEnabled),
                     glEnabled(depthWasEnabled),
                     glEnabled(stencilWasEnabled),
                     glEnabled(cullWasEnabled),
                     glIntRect(GL_VIEWPORT),
                     glIntRect(GL_SCISSOR_BOX))
                .arg(glInteger(GL_DRAW_FRAMEBUFFER_BINDING))
                .arg(glInteger(GL_READ_FRAMEBUFFER_BINDING))
                .arg(glInteger(GL_CURRENT_PROGRAM)));
    }

    const QString errorsBefore = drainGlErrors();
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

    int rectanglesDrawn = 0;
    bool shaderBound = false;
    GLint programDuringDraw = 0;
    {
        KWin::ShaderBinder binder(KWin::ShaderTrait::UniformColor | KWin::ShaderTrait::TransformColorspace);
        binder.shader()->setUniform(KWin::GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
        binder.shader()->setColorspaceUniforms(KWin::ColorDescription::sRGB, renderTarget.colorDescription(), KWin::RenderingIntent::Perceptual);
        shaderBound = KWin::ShaderManager::instance()->getBoundShader() != nullptr;
        programDuringDraw = glInteger(GL_CURRENT_PROGRAM);
        rectanglesDrawn = drawGridGeometry(viewport, screen);
    }

    glBlendFuncSeparate(previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha);
    restoreGlCapability(GL_BLEND, blendWasEnabled);
    restoreGlCapability(GL_SCISSOR_TEST, scissorWasEnabled);
    restoreGlCapability(GL_DEPTH_TEST, depthWasEnabled);
    restoreGlCapability(GL_STENCIL_TEST, stencilWasEnabled);
    restoreGlCapability(GL_CULL_FACE, cullWasEnabled);

    const QString errorsAfter = drainGlErrors();
    if (logThisPaint) {
        log(QStringLiteral("overlay_draw_result rectangles=%1 shader_bound=%2 program_during=%3 program_after=%4 errors_before=%5 errors_after=%6 gl_viewport_after=%7 gl_scissor_box_after=%8")
                .arg(rectanglesDrawn)
                .arg(shaderBound ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(programDuringDraw)
                .arg(glInteger(GL_CURRENT_PROGRAM))
                .arg(errorsBefore,
                     errorsAfter,
                     glIntRect(GL_VIEWPORT),
                     glIntRect(GL_SCISSOR_BOX)));
    }
}

int Effect::drawGridGeometry(const KWin::RenderViewport &viewport, KWin::LogicalOutput *screen)
{
    const KWin::RectF area = workAreaForOutput(screen);
    if (area.isEmpty()) {
        return 0;
    }

    int rectanglesDrawn = 0;
    const qreal line = std::max<qreal>(1.0, 1.0 / viewport.scale());
    const QColor gridColor = minimumAlpha(m_colors.gridColor, 0.28);
    const QColor selectionColor = minimumAlpha(m_colors.selectionColor, 0.18);
    const QColor borderColor = minimumAlpha(m_colors.selectionBorderColor, 0.45);

    if (const auto rect = currentSelectionRect()) {
        rectanglesDrawn += drawGlRect(viewport, *rect, selectionColor) ? 1 : 0;
        rectanglesDrawn += drawGlRect(viewport, KWin::RectF(rect->left(), rect->top(), rect->width(), line * 2.0), borderColor) ? 1 : 0;
        rectanglesDrawn += drawGlRect(viewport, KWin::RectF(rect->left(), rect->bottom() - line * 2.0, rect->width(), line * 2.0), borderColor) ? 1 : 0;
        rectanglesDrawn += drawGlRect(viewport, KWin::RectF(rect->left(), rect->top(), line * 2.0, rect->height()), borderColor) ? 1 : 0;
        rectanglesDrawn += drawGlRect(viewport, KWin::RectF(rect->right() - line * 2.0, rect->top(), line * 2.0, rect->height()), borderColor) ? 1 : 0;
    }

    rectanglesDrawn += drawGlRect(viewport, KWin::RectF(area.left(), area.top(), area.width(), line), gridColor) ? 1 : 0;
    rectanglesDrawn += drawGlRect(viewport, KWin::RectF(area.left(), area.bottom() - line, area.width(), line), gridColor) ? 1 : 0;
    rectanglesDrawn += drawGlRect(viewport, KWin::RectF(area.left(), area.top(), line, area.height()), gridColor) ? 1 : 0;
    rectanglesDrawn += drawGlRect(viewport, KWin::RectF(area.right() - line, area.top(), line, area.height()), gridColor) ? 1 : 0;

    for (int column = 1; column < m_activeSettings.grid.columns; ++column) {
        const qreal x = area.left() + area.width() * column / m_activeSettings.grid.columns;
        rectanglesDrawn += drawGlRect(viewport, KWin::RectF(x - line / 2.0, area.top(), line, area.height()), gridColor) ? 1 : 0;
    }

    for (int row = 1; row < m_activeSettings.grid.rows; ++row) {
        const qreal y = area.top() + area.height() * row / m_activeSettings.grid.rows;
        rectanglesDrawn += drawGlRect(viewport, KWin::RectF(area.left(), y - line / 2.0, area.width(), line), gridColor) ? 1 : 0;
    }

    return rectanglesDrawn;
}

bool Effect::drawGlRect(const KWin::RenderViewport &viewport, const KWin::RectF &rect, const QColor &color)
{
    if (rect.isEmpty() || color.alpha() == 0) {
        return false;
    }

    KWin::GLShader *shader = KWin::ShaderManager::instance()->getBoundShader();
    if (!shader) {
        return false;
    }

    shader->setUniform(KWin::GLShader::ColorUniform::Color, color);

    KWin::GLVertexBuffer *vbo = KWin::GLVertexBuffer::streamingBuffer();
    vbo->reset();
    const KWin::RectF mappedRect = mapOverlayRect(viewport, rect);
    if (mappedRect.isEmpty()) {
        return false;
    }
    const auto vertices = verticesForRect(mappedRect);
    vbo->setVertices(vertices);
    vbo->render(GL_TRIANGLES);
    return true;
}

QString Effect::overlayQmlPath() const
{
    return QString();
}

} // namespace Tiles
