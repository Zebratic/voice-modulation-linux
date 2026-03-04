#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <vector>
#include <string>

class EffectBase;
class ModulationManager;

class EffectSettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit EffectSettingsPanel(QWidget* parent = nullptr);

    void setEffect(EffectBase* effect);
    void clearEffect();
    void setModulationManager(ModulationManager* mgr) { m_modulationMgr = mgr; }

private:
    void buildSliders();
    void refreshModulatedValues();

    EffectBase* m_effect = nullptr;
    ModulationManager* m_modulationMgr = nullptr;
    QVBoxLayout* m_layout;
    QLabel* m_titleLabel;
    QWidget* m_slidersContainer = nullptr;
    QTimer m_refreshTimer;

    struct SliderEntry {
        QSlider* slider;
        QLabel* valueLabel;
        QPushButton* modBtn;
        std::string paramId;
    };
    std::vector<SliderEntry> m_sliderEntries;
};
