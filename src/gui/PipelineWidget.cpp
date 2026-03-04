#include "gui/PipelineWidget.h"
#include "audio/EffectPipeline.h"
#include "gui/EffectSettingsPanel.h"
#include "effects/EffectRegistry.h"
#include "modulation/ModulationManager.h"
#include <QHBoxLayout>
#include <QGraphicsLineItem>
#include <QPen>
#include <QFont>
#include <algorithm>

static constexpr qreal BLOCK_W = EffectBlockWidget::BLOCK_W;
static constexpr qreal BLOCK_H = EffectBlockWidget::BLOCK_H;
static constexpr qreal SPACING = EffectBlockWidget::SPACING;
static constexpr qreal LABEL_H = 30;

PipelineWidget::PipelineWidget(QWidget* parent) : QWidget(parent) {
    auto* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Graphics view for the pipeline visualization
    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setStyleSheet("background-color: #1a1a2e; border: 1px solid #333;");
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    outerLayout->addWidget(m_view, 1);

    // Up/Down buttons on the right side
    auto* btnLayout = new QVBoxLayout();
    btnLayout->addStretch();
    m_upBtn = new QPushButton("\xe2\x96\xb2", this); // Unicode up triangle
    m_upBtn->setFixedSize(36, 36);
    m_upBtn->setToolTip("Move selected effect up");
    m_upBtn->setEnabled(false);
    connect(m_upBtn, &QPushButton::clicked, this, &PipelineWidget::moveSelectedUp);
    btnLayout->addWidget(m_upBtn);

    m_downBtn = new QPushButton("\xe2\x96\xbc", this); // Unicode down triangle
    m_downBtn->setFixedSize(36, 36);
    m_downBtn->setToolTip("Move selected effect down");
    m_downBtn->setEnabled(false);
    connect(m_downBtn, &QPushButton::clicked, this, &PipelineWidget::moveSelectedDown);
    btnLayout->addWidget(m_downBtn);
    btnLayout->addStretch();
    outerLayout->addLayout(btnLayout);
}

void PipelineWidget::setPipeline(EffectPipeline* pipeline) {
    m_pipeline = pipeline;
    rebuild();
}

void PipelineWidget::setSettingsPanel(EffectSettingsPanel* panel) {
    m_settingsPanel = panel;
}

void PipelineWidget::addEffect(const std::string& effectId) {
    if (!m_pipeline) return;
    auto effect = EffectRegistry::instance().create(effectId);
    if (!effect) return;
    m_pipeline->addEffect(std::move(effect));
    rebuild();
}

void PipelineWidget::removeSelectedEffect() {
    if (!m_pipeline || m_selectedIndex < 0) return;
    // Remove modulators targeting this effect before removing it
    if (m_modulationMgr) {
        auto* effect = m_pipeline->effectAt(m_selectedIndex);
        if (effect)
            m_modulationMgr->removeModulatorsForEffect(effect->id());
    }
    m_pipeline->removeEffect(m_selectedIndex);
    m_selectedIndex = -1;
    if (m_settingsPanel) m_settingsPanel->clearEffect();
    rebuild();
}

void PipelineWidget::addFlowArrow(qreal y) {
    qreal cx = BLOCK_W / 2;
    m_scene->addLine(cx, y, cx, y + SPACING, QPen(QColor(150, 150, 150), 2));
    // Arrowhead
    qreal arrowY = y + SPACING - 2;
    m_scene->addLine(cx - 5, arrowY - 6, cx, arrowY, QPen(QColor(150, 150, 150), 2));
    m_scene->addLine(cx + 5, arrowY - 6, cx, arrowY, QPen(QColor(150, 150, 150), 2));
}

void PipelineWidget::rebuild() {
    m_scene->clear();
    m_blocks.clear();
    if (!m_pipeline) return;

    qreal y = 0;

    // "INPUT" label at top
    auto* inputLabel = m_scene->addText("INPUT", QFont("sans-serif", 11, QFont::Bold));
    inputLabel->setDefaultTextColor(QColor(100, 200, 100));
    qreal textW = inputLabel->boundingRect().width();
    inputLabel->setPos((BLOCK_W - textW) / 2, y);
    y += LABEL_H;

    int count = m_pipeline->effectCount();

    if (count > 0) {
        addFlowArrow(y);
        y += SPACING;
    }

    for (int i = 0; i < count; ++i) {
        auto* effect = m_pipeline->effectAt(i);
        if (!effect) continue;

        auto* block = new EffectBlockWidget(
            QString::fromStdString(effect->id()),
            QString::fromStdString(effect->name()), i);
        block->setBypassed(!effect->enabled());
        block->setPos(0, y);
        block->setSelected(i == m_selectedIndex);
        block->onClicked = [this](EffectBlockWidget* b) { onBlockClicked(b); };

        m_scene->addItem(block);
        m_blocks.push_back(block);

        y += BLOCK_H;
        addFlowArrow(y);
        y += SPACING;
    }

    if (count == 0) {
        addFlowArrow(y);
        y += SPACING;
    }

    // "OUTPUT" label at bottom
    auto* outputLabel = m_scene->addText("OUTPUT", QFont("sans-serif", 11, QFont::Bold));
    outputLabel->setDefaultTextColor(QColor(200, 100, 100));
    textW = outputLabel->boundingRect().width();
    outputLabel->setPos((BLOCK_W - textW) / 2, y);
    y += LABEL_H;

    m_scene->setSceneRect(-10, -10, BLOCK_W + 20, y + 10);
    updateButtons();
}

void PipelineWidget::onBlockClicked(EffectBlockWidget* block) {
    // Deselect previous
    for (auto* b : m_blocks)
        b->setSelected(false);

    m_selectedIndex = block->index();
    block->setSelected(true);

    if (m_settingsPanel && m_pipeline)
        m_settingsPanel->setEffect(m_pipeline->effectAt(m_selectedIndex));

    updateButtons();
    emit effectSelected(m_selectedIndex);
}

void PipelineWidget::moveSelectedUp() {
    if (!m_pipeline || m_selectedIndex <= 0) return;
    m_pipeline->moveEffect(m_selectedIndex, m_selectedIndex - 1);
    m_selectedIndex--;
    rebuild();
    // Re-select after rebuild
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_blocks.size()))
        m_blocks[m_selectedIndex]->setSelected(true);
    if (m_settingsPanel && m_pipeline)
        m_settingsPanel->setEffect(m_pipeline->effectAt(m_selectedIndex));
    updateButtons();
}

void PipelineWidget::moveSelectedDown() {
    if (!m_pipeline || m_selectedIndex < 0) return;
    int count = m_pipeline->effectCount();
    if (m_selectedIndex >= count - 1) return;
    m_pipeline->moveEffect(m_selectedIndex, m_selectedIndex + 1);
    m_selectedIndex++;
    rebuild();
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_blocks.size()))
        m_blocks[m_selectedIndex]->setSelected(true);
    if (m_settingsPanel && m_pipeline)
        m_settingsPanel->setEffect(m_pipeline->effectAt(m_selectedIndex));
    updateButtons();
}

void PipelineWidget::updateButtons() {
    int count = m_pipeline ? m_pipeline->effectCount() : 0;
    m_upBtn->setEnabled(m_selectedIndex > 0);
    m_downBtn->setEnabled(m_selectedIndex >= 0 && m_selectedIndex < count - 1);
}
