// Copyright (c) 2024-2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VALIDITY_QT_GAUGEWIDGET_H
#define VALIDITY_QT_GAUGEWIDGET_H

#include <QWidget>
#include <QString>
#include <QTimer>

class GaugeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GaugeWidget(QWidget* parent = nullptr);

    void setValue(double value);
    void setMaxValue(double maxValue);
    void setLabel(const QString& label);
    void setSubtitle(const QString& text);
    void setValueText(const QString& text);
    void setArcColor(const QColor& color);
    void setTrackColor(const QColor& color);
    void setLabelAbove(bool above);
    void setSmallValueFont(bool small);
    void setSpinning(bool spinning);
    bool isSpinning() const { return m_spinning; }

    double value() const { return m_value; }
    double maxValue() const { return m_maxValue; }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;   // VALIDITY: resume spinner timer
    void hideEvent(QHideEvent* event) override;   // VALIDITY: pause spinner timer

private:
    double m_value = 0.0;
    double m_maxValue = 100.0;
    QString m_label;
    QString m_subtitle;
    QString m_valueText;
    QColor m_arcColor{0x4e, 0xcc, 0xa3};     // #4ecca3
    QColor m_trackColor{0x2d, 0x2d, 0x4a};   // #2d2d4a
    bool m_labelAbove = false;
    bool m_smallValueFont = false;
    bool m_spinning = false;
    double m_spinAngle = 0.0;
    QTimer* m_spinTimer{nullptr};
    bool m_shown{false};  // VALIDITY: between showEvent and hideEvent (incl. minimize)

    static constexpr double ARC_START = 225.0;
    static constexpr double ARC_SPAN = -270.0;
    static constexpr double SPINNER_ARC = 45.0;  // spinner segment size in degrees
};

#endif // VALIDITY_QT_GAUGEWIDGET_H
