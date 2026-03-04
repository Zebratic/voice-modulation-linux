#pragma once
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QString>
#include <QPointF>
#include <functional>

class EffectBlockWidget : public QGraphicsRectItem {
public:
    EffectBlockWidget(const QString& effectId, const QString& name, int index);

    QString effectId() const { return m_effectId; }
    int index() const { return m_index; }
    void setIndex(int idx);
    void setSelected(bool selected);

    bool bypassed() const { return m_bypassed; }
    void setBypassed(bool b);

    std::function<void(EffectBlockWidget*)> onClicked;

    static constexpr qreal BLOCK_W = 200;
    static constexpr qreal BLOCK_H = 50;
    static constexpr qreal SPACING = 16;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void updateAppearance();

    QString m_effectId;
    QString m_name;
    int m_index;
    bool m_bypassed = false;
    bool m_isSelected = false;
    QGraphicsTextItem* m_label;
    QPointF m_dragStartPos;
};
