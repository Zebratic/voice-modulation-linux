#pragma once
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPushButton>
#include <QVBoxLayout>
#include <vector>
#include "gui/EffectBlockWidget.h"

class EffectPipeline;
class EffectSettingsPanel;
class ModulationManager;

class PipelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit PipelineWidget(QWidget* parent = nullptr);

    void setPipeline(EffectPipeline* pipeline);
    void setSettingsPanel(EffectSettingsPanel* panel);
    void setModulationManager(ModulationManager* mgr) { m_modulationMgr = mgr; }
    void addEffect(const std::string& effectId);
    void removeSelectedEffect();
    void rebuild();

signals:
    void effectSelected(int index);

private:
    void onBlockClicked(EffectBlockWidget* block);
    void moveSelectedUp();
    void moveSelectedDown();
    void updateButtons();
    void addFlowArrow(qreal y);

    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
    QPushButton* m_upBtn;
    QPushButton* m_downBtn;
    EffectPipeline* m_pipeline = nullptr;
    EffectSettingsPanel* m_settingsPanel = nullptr;
    ModulationManager* m_modulationMgr = nullptr;
    std::vector<EffectBlockWidget*> m_blocks;
    int m_selectedIndex = -1;
};
