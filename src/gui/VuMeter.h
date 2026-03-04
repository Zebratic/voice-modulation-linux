#pragma once
#include <QWidget>
#include <QTimer>

class VuMeter : public QWidget {
    Q_OBJECT
public:
    explicit VuMeter(QWidget* parent = nullptr);

    void setLevel(float level); // 0.0 - 1.0+
    void setLabel(const QString& label);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override { return {200, 24}; }

private:
    float m_level = 0.f;
    float m_peak = 0.f;
    QString m_label;
    QTimer m_decayTimer;
};
