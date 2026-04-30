// Copyright (c) 2024-2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/gaugewidget.h>

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <cmath>

GaugeWidget::GaugeWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(140, 160);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_spinTimer = new QTimer(this);
    connect(m_spinTimer, &QTimer::timeout, this, [this]() {
        m_spinAngle += 3.0; // 3 degrees per tick — smooth bounce
        if (m_spinAngle >= 360.0) m_spinAngle -= 360.0;
        update();
    });
}

void GaugeWidget::setValue(double value)
{
    m_value = std::max(0.0, std::min(value, m_maxValue));
    update();
}

void GaugeWidget::setMaxValue(double maxValue)
{
    m_maxValue = std::max(0.001, maxValue);
    m_value = std::min(m_value, m_maxValue);
    update();
}

void GaugeWidget::setLabel(const QString& label)
{
    m_label = label;
    update();
}

void GaugeWidget::setSubtitle(const QString& text)
{
    m_subtitle = text;
    update();
}

void GaugeWidget::setValueText(const QString& text)
{
    m_valueText = text;
    update();
}

void GaugeWidget::setArcColor(const QColor& color)
{
    m_arcColor = color;
    update();
}

void GaugeWidget::setTrackColor(const QColor& color)
{
    m_trackColor = color;
    update();
}

void GaugeWidget::setLabelAbove(bool above)
{
    m_labelAbove = above;
    update();
}

void GaugeWidget::setSmallValueFont(bool small)
{
    m_smallValueFont = small;
    update();
}

void GaugeWidget::setSpinning(bool spinning)
{
    m_spinning = spinning;
    m_spinAngle = 0.0;
    if (spinning) {
        m_spinTimer->start(30); // ~33fps
    } else {
        m_spinTimer->stop();
    }
    update();
}

QSize GaugeWidget::minimumSizeHint() const
{
    return QSize(140, 160);
}

QSize GaugeWidget::sizeHint() const
{
    return QSize(200, 220);
}

void GaugeWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Gauge dimensions
    const int arcThickness = 14;
    const int padding = 8;
    const int labelHeight = 22;
    const int subtitleHeight = m_subtitle.isEmpty() ? 0 : 18;
    const int textReserve = labelHeight + subtitleHeight + 8;
    const int gaugeSize = std::min(w - 2 * padding, h - textReserve - padding);
    const int arcDiameter = std::max(60, gaugeSize);
    const int arcX = (w - arcDiameter) / 2;

    // Position arc depending on label placement
    int arcY = padding;
    if (m_labelAbove) {
        arcY = padding + labelHeight;
    }

    // Draw label
    painter.setPen(QColor(0xc8, 0xc8, 0xd8)); // #c8c8d8
    QFont labelFont = font();
    labelFont.setPointSize(11);
    labelFont.setBold(true);
    painter.setFont(labelFont);

    if (m_labelAbove) {
        QRectF labelRect(0, padding, w, labelHeight);
        painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, m_label);
    } else {
        QRectF labelRect(0, arcY + arcDiameter + 4, w, labelHeight);
        painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, m_label);
    }

    QRectF arcRect(arcX + arcThickness / 2.0, arcY + arcThickness / 2.0,
                   arcDiameter - arcThickness, arcDiameter - arcThickness);

    // Draw track arc (background)
    QPen trackPen(m_trackColor, arcThickness, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(arcRect, ARC_START * 16, ARC_SPAN * 16);

    // Draw value arc or spinner
    if (m_spinning) {
        // Ping-pong: use sine wave to bounce within arc bounds
        // m_spinAngle goes 0→360 continuously, sine gives -1→1
        double sineVal = std::sin(m_spinAngle * 3.14159265 / 180.0); // -1 to 1
        double t = (sineVal + 1.0) / 2.0; // 0 to 1 (bounce)

        // Available travel = total arc span minus spinner size
        double travel = std::abs(ARC_SPAN) - SPINNER_ARC; // ~225 degrees of travel
        double offset = t * travel; // how far along the arc

        double spinStart = ARC_START - offset; // move clockwise from start
        double spinSpan = -SPINNER_ARC;

        QPen spinPen(m_arcColor, arcThickness, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(spinPen);
        painter.drawArc(arcRect, spinStart * 16, spinSpan * 16);
    } else {
        double fraction = (m_maxValue > 0) ? (m_value / m_maxValue) : 0.0;
        fraction = std::max(0.0, std::min(1.0, fraction));
        if (fraction > 0.001) {
            QPen valuePen(m_arcColor, arcThickness, Qt::SolidLine, Qt::RoundCap);
            painter.setPen(valuePen);
            double valueSpan = ARC_SPAN * fraction;
            painter.drawArc(arcRect, ARC_START * 16, valueSpan * 16);
        }
    }

    // Draw center value text
    painter.setPen(m_arcColor);
    QFont valueFont = font();
    valueFont.setPointSize(m_smallValueFont ? 10 : 14);
    valueFont.setBold(true);
    painter.setFont(valueFont);

    // Use full widget width so long text (e.g. "Not staking") doesn't clip against the arc
    QRectF centerRect(0, arcY, w, arcDiameter);
    painter.drawText(centerRect, Qt::AlignCenter, m_valueText);

    // Draw subtitle below the arc (if set)
    if (!m_subtitle.isEmpty()) {
        painter.setPen(QColor(0x7a, 0x7a, 0x96)); // #7a7a96
        QFont subFont = font();
        subFont.setPointSize(8);
        painter.setFont(subFont);

        int subY = m_labelAbove ? (arcY + arcDiameter + 4) : (arcY + arcDiameter + labelHeight + 6);
        QRectF subRect(0, subY, w, subtitleHeight);
        painter.drawText(subRect, Qt::AlignHCenter | Qt::AlignTop, m_subtitle);
    }
}
