#include "gui/EffectBlockWidget.h"
#include <QBrush>
#include <QPen>
#include <QFont>

EffectBlockWidget::EffectBlockWidget(const QString& effectId, const QString& name, int index)
    : m_effectId(effectId), m_name(name), m_index(index) {
    setRect(0, 0, BLOCK_W, BLOCK_H);
    setPos(0, index * (BLOCK_H + SPACING));

    m_label = new QGraphicsTextItem(name, this);
    m_label->setDefaultTextColor(Qt::white);
    QFont f = m_label->font();
    f.setPointSize(10);
    f.setBold(true);
    m_label->setFont(f);
    qreal textW = m_label->boundingRect().width();
    m_label->setPos((BLOCK_W - textW) / 2, (BLOCK_H - m_label->boundingRect().height()) / 2);

    updateAppearance();
}

void EffectBlockWidget::setIndex(int idx) {
    m_index = idx;
    setPos(0, idx * (BLOCK_H + SPACING));
}

void EffectBlockWidget::setSelected(bool selected) {
    m_isSelected = selected;
    updateAppearance();
}

void EffectBlockWidget::setBypassed(bool b) {
    m_bypassed = b;
    updateAppearance();
}

void EffectBlockWidget::updateAppearance() {
    if (m_bypassed) {
        setBrush(QBrush(QColor(80, 80, 80)));
        setPen(QPen(m_isSelected ? QColor(255, 200, 50) : QColor(120, 120, 120), 2));
    } else {
        setBrush(QBrush(QColor(50, 100, 180)));
        setPen(QPen(m_isSelected ? QColor(255, 200, 50) : QColor(80, 150, 220), 2));
    }
}

void EffectBlockWidget::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton && onClicked)
        onClicked(this);
    QGraphicsRectItem::mousePressEvent(event);
}

void EffectBlockWidget::mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) {
    setBypassed(!m_bypassed);
}
