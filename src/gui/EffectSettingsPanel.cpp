#include "gui/EffectSettingsPanel.h"
#include "effects/EffectBase.h"
#include "modulation/ModulationManager.h"
#include "gui/ModulatorEditorDialog.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>

EffectSettingsPanel::EffectSettingsPanel(QWidget* parent) : QWidget(parent) {
    m_layout = new QVBoxLayout(this);
    m_titleLabel = new QLabel("No effect selected", this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_layout->addWidget(m_titleLabel);
    m_layout->addStretch();

    m_refreshTimer.setInterval(60);
    connect(&m_refreshTimer, &QTimer::timeout, this, &EffectSettingsPanel::refreshModulatedValues);
}

void EffectSettingsPanel::setEffect(EffectBase* effect) {
    m_effect = effect;
    buildSliders();
}

void EffectSettingsPanel::clearEffect() {
    m_effect = nullptr;
    m_sliderEntries.clear();
    m_refreshTimer.stop();
    if (m_slidersContainer) {
        delete m_slidersContainer;
        m_slidersContainer = nullptr;
    }
    m_titleLabel->setText("No effect selected");
}

void EffectSettingsPanel::buildSliders() {
    if (m_slidersContainer) {
        delete m_slidersContainer;
        m_slidersContainer = nullptr;
    }
    m_sliderEntries.clear();
    m_refreshTimer.stop();

    if (!m_effect) return;

    m_titleLabel->setText(QString::fromStdString(m_effect->name()));

    m_slidersContainer = new QWidget(this);
    auto* vbox = new QVBoxLayout(m_slidersContainer);

    std::string effectId = m_effect->id();
    bool hasModulators = false;

    for (auto& param : m_effect->params()) {
        auto* group = new QGroupBox(QString::fromStdString(param.name), m_slidersContainer);
        auto* hbox = new QHBoxLayout(group);

        auto* slider = new QSlider(Qt::Horizontal, group);
        slider->setRange(0, 1000);
        float norm = (param.value.load() - param.min) / (param.max - param.min);
        slider->setValue(static_cast<int>(norm * 1000));

        auto* valueLabel = new QLabel(QString::number(param.value.load(), 'f', 2), group);
        valueLabel->setMinimumWidth(60);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        hbox->addWidget(slider);
        hbox->addWidget(valueLabel);

        // Modulator button
        auto* modBtn = new QPushButton("~", group);
        modBtn->setFixedSize(28, 28);
        modBtn->setToolTip("Modulate this parameter");
        hbox->addWidget(modBtn);

        // Check if modulator already active
        bool isModulated = m_modulationMgr &&
            m_modulationMgr->hasActiveModulator(effectId, param.id);
        if (isModulated) {
            modBtn->setStyleSheet("background-color: #cc8800; font-weight: bold;");
            slider->setEnabled(false);
            hasModulators = true;
        }

        // Capture for lambdas
        float* minPtr = &param.min;
        float* maxPtr = &param.max;
        std::atomic<float>* valPtr = &param.value;

        connect(slider, &QSlider::valueChanged, this, [minPtr, maxPtr, valPtr, valueLabel](int v) {
            float val = *minPtr + (*maxPtr - *minPtr) * (v / 1000.f);
            valPtr->store(val, std::memory_order_relaxed);
            valueLabel->setText(QString::number(val, 'f', 2));
        });

        // Modulator button click
        std::string paramId = param.id;
        float pMin = param.min, pMax = param.max;
        QString paramName = QString::fromStdString(param.name);

        connect(modBtn, &QPushButton::clicked, this,
            [this, effectId, paramId, paramName, pMin, pMax, valPtr, slider, modBtn, valueLabel]() {
            if (!m_modulationMgr) return;

            const ModulatorConfig* existing = m_modulationMgr->findConfig(effectId, paramId);
            ModulatorEditorDialog dlg(paramName, pMin, pMax, existing, this);

            if (dlg.exec() != QDialog::Accepted) return;

            if (dlg.removeRequested()) {
                // Find and remove
                auto configs = m_modulationMgr->snapshot();
                for (int i = 0; i < static_cast<int>(configs.size()); ++i) {
                    if (configs[i].effectId == effectId && configs[i].paramId == paramId) {
                        m_modulationMgr->removeModulator(i);
                        break;
                    }
                }
                slider->setEnabled(true);
                modBtn->setStyleSheet("");
                emit modulationChanged();
                return;
            }

            ModulatorConfig cfg = dlg.result();
            cfg.effectId = effectId;
            cfg.paramId = paramId;
            m_modulationMgr->addModulator(cfg, valPtr);

            if (cfg.active) {
                slider->setEnabled(false);
                modBtn->setStyleSheet("background-color: #cc8800; font-weight: bold;");
            } else {
                slider->setEnabled(true);
                modBtn->setStyleSheet("");
            }
            emit modulationChanged();
        });

        m_sliderEntries.push_back({slider, valueLabel, modBtn, param.id});
        vbox->addWidget(group);
    }

    m_layout->insertWidget(1, m_slidersContainer);

    if (hasModulators)
        m_refreshTimer.start();
}

void EffectSettingsPanel::refreshModulatedValues() {
    if (!m_effect || !m_modulationMgr) return;

    std::string effectId = m_effect->id();
    bool anyActive = false;

    for (auto& entry : m_sliderEntries) {
        if (m_modulationMgr->hasActiveModulator(effectId, entry.paramId)) {
            float val = m_modulationMgr->currentValue(effectId, entry.paramId);
            entry.valueLabel->setText(QString::number(val, 'f', 2));
            anyActive = true;
        }
    }

    if (!anyActive)
        m_refreshTimer.stop();
}
