#include "effect.h"

#include "core/output.h"
#include "effect/effectwindow.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

namespace Tiles
{

void Effect::openLogFile()
{
    const QString stateRoot = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    const QString baseDir = stateRoot.isEmpty()
        ? QDir::homePath() + QStringLiteral("/.local/state")
        : stateRoot;
    const QString logDir = baseDir + QStringLiteral("/kwin-clicktile/effect");

    QDir().mkpath(logDir);
    m_logFile.setFileName(logDir + QStringLiteral("/events.log"));
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "kwin-clicktile: failed to open log file" << m_logFile.fileName() << m_logFile.errorString();
    } else {
        qInfo().noquote() << "kwin-clicktile: log_file path=" << m_logFile.fileName();
    }
}

void Effect::log(const QString &message)
{
    const QString line = QStringLiteral("%1\t%2")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), message);

    qInfo().noquote() << "kwin-clicktile:" << message;

    if (m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << line << Qt::endl;
        m_logFile.flush();
    }
}

QString Effect::describeOutput(KWin::LogicalOutput *output) const
{
    if (!output) {
        return QStringLiteral("<none>");
    }

    return QStringLiteral("%1 %2")
        .arg(output->name(),
             describeRect(output->geometryF()));
}

QString Effect::describeWindow(KWin::EffectWindow *window) const
{
    if (!window) {
        return QStringLiteral("<none>");
    }

    return QStringLiteral("%1 class=%2 id=%3")
        .arg(window->caption(),
             window->windowClass(),
             window->internalId().toString(QUuid::WithoutBraces));
}

QString Effect::describeRect(const KWin::RectF &rect) const
{
    return QStringLiteral("%1,%2 %3x%4")
        .arg(rect.x(), 0, 'f', 0)
        .arg(rect.y(), 0, 'f', 0)
        .arg(rect.width(), 0, 'f', 0)
        .arg(rect.height(), 0, 'f', 0);
}

QString Effect::describeButtons(Qt::MouseButtons buttons) const
{
    QStringList parts;
    if (buttons.testFlag(Qt::LeftButton)) {
        parts << QStringLiteral("left");
    }
    if (buttons.testFlag(Qt::MiddleButton)) {
        parts << QStringLiteral("middle");
    }
    if (buttons.testFlag(Qt::RightButton)) {
        parts << QStringLiteral("right");
    }
    if (parts.isEmpty()) {
        return QStringLiteral("none");
    }
    return parts.join(QLatin1Char('+'));
}

QString Effect::describeButton(Qt::MouseButton button) const
{
    switch (button) {
    case Qt::LeftButton:
        return QStringLiteral("left");
    case Qt::RightButton:
        return QStringLiteral("right");
    case Qt::MiddleButton:
        return QStringLiteral("middle");
    default:
        return QString::number(static_cast<int>(button));
    }
}

QString Effect::describeButtonState(KWin::PointerButtonState state) const
{
    switch (state) {
    case KWin::PointerButtonState::Pressed:
        return QStringLiteral("pressed");
    case KWin::PointerButtonState::Released:
        return QStringLiteral("released");
    }
    return QStringLiteral("unknown");
}

} // namespace Tiles
