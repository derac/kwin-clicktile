#include "settings.h"

#include <QColorDialog>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <KCModule>
#include <KPluginFactory>

namespace
{

class TilePreview final : public QWidget
{
    Q_OBJECT

public:
    explicit TilePreview(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(220, 140);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setGrid(Tiles::TileGrid grid)
    {
        m_grid = Tiles::sanitizeGrid(grid.columns, grid.rows);
        update();
    }

    void setColors(const Tiles::OverlayColors &colors)
    {
        m_colors = colors;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF bounds = rect().adjusted(12, 12, -12, -12);
        painter.fillRect(rect(), palette().window());

        QColor fill = m_colors.selectionColor;
        if (!fill.isValid()) {
            fill = Tiles::defaultOverlayColors().selectionColor;
        }
        painter.fillRect(bounds, fill);

        QColor border = m_colors.selectionBorderColor;
        if (!border.isValid()) {
            border = Tiles::defaultOverlayColors().selectionBorderColor;
        }
        painter.setPen(QPen(border, 2));
        painter.drawRoundedRect(bounds, 4, 4);

        QColor gridColor = m_colors.gridColor;
        if (!gridColor.isValid()) {
            gridColor = Tiles::defaultOverlayColors().gridColor;
        }
        painter.setPen(QPen(gridColor, 1.5));
        for (int column = 1; column < m_grid.columns; ++column) {
            const qreal x = bounds.left() + bounds.width() * column / m_grid.columns;
            painter.drawLine(QPointF(x, bounds.top()), QPointF(x, bounds.bottom()));
        }
        for (int row = 1; row < m_grid.rows; ++row) {
            const qreal y = bounds.top() + bounds.height() * row / m_grid.rows;
            painter.drawLine(QPointF(bounds.left(), y), QPointF(bounds.right(), y));
        }
    }

private:
    Tiles::TileGrid m_grid = Tiles::TileGrid{2, 2};
    Tiles::OverlayColors m_colors = Tiles::defaultOverlayColors();
};

QColor buttonColor(const QPushButton *button)
{
    return button->property("overlayColor").value<QColor>();
}

void setButtonColor(QPushButton *button, const QColor &color)
{
    button->setProperty("overlayColor", color);
    button->setText(color.name(QColor::HexArgb));
    button->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; }").arg(color.name(QColor::HexArgb)));
}

class ConfigModule final : public KCModule
{
    Q_OBJECT

public:
    explicit ConfigModule(QObject *parent, const KPluginMetaData &data)
        : KCModule(parent, data)
    {
        auto *root = new QVBoxLayout(widget());

        auto *intro = new QLabel(
            tr("Per-monitor grid settings. KWin reports dimensions in logical pixels, so mixed-DPI outputs use their own scale automatically."),
            widget());
        intro->setWordWrap(true);
        root->addWidget(intro);

        m_table = new QTableWidget(widget());
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({tr("Monitor"), tr("Columns"), tr("Rows"), tr("Default")});
        m_table->horizontalHeader()->setStretchLastSection(false);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->verticalHeader()->setVisible(false);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        root->addWidget(m_table);

        m_preview = new TilePreview(widget());
        root->addWidget(m_preview);

        auto *colorRow = new QHBoxLayout;
        m_gridColorButton = addColorButton(colorRow, tr("Grid"));
        m_selectionColorButton = addColorButton(colorRow, tr("Selection"));
        m_selectionBorderColorButton = addColorButton(colorRow, tr("Border"));
        root->addLayout(colorRow);

        connect(m_table, &QTableWidget::currentCellChanged, this, [this] {
            updatePreview();
        });

        load();
    }

    void load() override
    {
        const auto config = Tiles::openKWinConfig();
        const Tiles::OverlayColors colors = Tiles::readOverlayColors(config);
        setButtonColor(m_gridColorButton, colors.gridColor);
        setButtonColor(m_selectionColorButton, colors.selectionColor);
        setButtonColor(m_selectionBorderColorButton, colors.selectionBorderColor);

        m_table->setRowCount(0);
        const auto screens = QGuiApplication::screens();
        for (QScreen *screen : screens) {
            addScreenRow(config, screen);
        }

        if (m_table->rowCount() > 0) {
            m_table->selectRow(0);
        }
        updatePreview();
        setNeedsSave(false);
    }

    void save() override
    {
        const auto config = Tiles::openKWinConfig();
        Tiles::writeOverlayColors(config, currentColors());

        for (int row = 0; row < m_table->rowCount(); ++row) {
            auto *labelItem = m_table->item(row, 0);
            auto *columns = qobject_cast<QSpinBox *>(m_table->cellWidget(row, 1));
            auto *rows = qobject_cast<QSpinBox *>(m_table->cellWidget(row, 2));
            if (!labelItem || !columns || !rows) {
                continue;
            }

            Tiles::OutputSettings settings;
            settings.key = labelItem->data(Qt::UserRole).toString();
            settings.token = Tiles::outputToken(settings.key);
            settings.label = labelItem->text();
            settings.grid = Tiles::sanitizeGrid(columns->value(), rows->value());
            Tiles::writeOutputSettings(config, settings);
        }

        config->sync();
        setNeedsSave(false);
    }

    void defaults() override
    {
        const Tiles::OverlayColors colors = Tiles::defaultOverlayColors();
        setButtonColor(m_gridColorButton, colors.gridColor);
        setButtonColor(m_selectionColorButton, colors.selectionColor);
        setButtonColor(m_selectionBorderColorButton, colors.selectionBorderColor);

        for (int row = 0; row < m_table->rowCount(); ++row) {
            auto *columns = qobject_cast<QSpinBox *>(m_table->cellWidget(row, 1));
            auto *rows = qobject_cast<QSpinBox *>(m_table->cellWidget(row, 2));
            auto *defaultItem = m_table->item(row, 3);
            if (!columns || !rows || !defaultItem) {
                continue;
            }

            const Tiles::TileGrid defaults{
                defaultItem->data(Qt::UserRole).toInt(),
                defaultItem->data(Qt::UserRole + 1).toInt(),
            };
            columns->setValue(defaults.columns);
            rows->setValue(defaults.rows);
        }

        updatePreview();
        setNeedsSave(true);
    }

private:
    QPushButton *addColorButton(QHBoxLayout *layout, const QString &label)
    {
        auto *button = new QPushButton(label, widget());
        button->setMinimumWidth(120);
        connect(button, &QPushButton::clicked, this, [this, button] {
            const QColor chosen = QColorDialog::getColor(buttonColor(button), widget(), tr("Choose Overlay Color"), QColorDialog::ShowAlphaChannel);
            if (!chosen.isValid()) {
                return;
            }
            setButtonColor(button, chosen);
            updatePreview();
            setNeedsSave(true);
        });
        layout->addWidget(button);
        return button;
    }

    void addScreenRow(const KSharedConfigPtr &config, QScreen *screen)
    {
        if (!screen) {
            return;
        }

        const QString key = Tiles::outputKey(screen->name(), screen->manufacturer(), screen->model(), screen->serialNumber());
        const QString label = Tiles::outputLabel(screen->name(), screen->manufacturer(), screen->model(), screen->serialNumber());
        const Tiles::TileGrid defaultGrid = Tiles::defaultGridForGeometry(screen->geometry());
        const Tiles::OutputSettings settings = Tiles::readOutputSettings(config, key, label, screen->geometry());

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *labelItem = new QTableWidgetItem(label);
        labelItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        labelItem->setData(Qt::UserRole, key);
        m_table->setItem(row, 0, labelItem);

        auto *columns = new QSpinBox(m_table);
        columns->setRange(Tiles::MinGridDimension, Tiles::MaxGridDimension);
        columns->setValue(settings.grid.columns);
        m_table->setCellWidget(row, 1, columns);

        auto *rows = new QSpinBox(m_table);
        rows->setRange(Tiles::MinGridDimension, Tiles::MaxGridDimension);
        rows->setValue(settings.grid.rows);
        m_table->setCellWidget(row, 2, rows);

        auto *defaultItem = new QTableWidgetItem(QStringLiteral("%1x%2").arg(defaultGrid.columns).arg(defaultGrid.rows));
        defaultItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        defaultItem->setData(Qt::UserRole, defaultGrid.columns);
        defaultItem->setData(Qt::UserRole + 1, defaultGrid.rows);
        m_table->setItem(row, 3, defaultItem);

        connect(columns, &QSpinBox::valueChanged, this, [this] {
            updatePreview();
            setNeedsSave(true);
        });
        connect(rows, &QSpinBox::valueChanged, this, [this] {
            updatePreview();
            setNeedsSave(true);
        });
    }

    Tiles::OverlayColors currentColors() const
    {
        return Tiles::OverlayColors{
            buttonColor(m_gridColorButton),
            buttonColor(m_selectionColorButton),
            buttonColor(m_selectionBorderColorButton),
        };
    }

    void updatePreview()
    {
        m_preview->setColors(currentColors());

        const int row = m_table->currentRow();
        auto *columns = qobject_cast<QSpinBox *>(m_table->cellWidget(row, 1));
        auto *rows = qobject_cast<QSpinBox *>(m_table->cellWidget(row, 2));
        if (!columns || !rows) {
            m_preview->setGrid(Tiles::TileGrid{2, 2});
            return;
        }
        m_preview->setGrid(Tiles::sanitizeGrid(columns->value(), rows->value()));
    }

    QTableWidget *m_table = nullptr;
    TilePreview *m_preview = nullptr;
    QPushButton *m_gridColorButton = nullptr;
    QPushButton *m_selectionColorButton = nullptr;
    QPushButton *m_selectionBorderColorButton = nullptr;
};

} // namespace

K_PLUGIN_CLASS_WITH_JSON(ConfigModule, "metadata.json")

#include "configmodule.moc"
