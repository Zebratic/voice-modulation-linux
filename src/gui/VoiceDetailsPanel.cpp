#include "gui/VoiceDetailsPanel.h"
#include "profile/ProfileManager.h"
#include "effects/EffectRegistry.h"
#include "effects/EffectBase.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QInputDialog>
#include <QMessageBox>
#include <nlohmann/json.hpp>

VoiceDetailsPanel::VoiceDetailsPanel(ProfileManager& profileManager, QWidget* parent)
    : QWidget(parent)
    , m_profileManager(profileManager)
{
    setupUI();
    clear();
}

void VoiceDetailsPanel::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // ── Header: voice icon + name + rename ──────────────────────────────────
    auto* headerRow = new QHBoxLayout();

    m_nameLabel = new QLabel("(No voice selected)", this);
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSize(14);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    headerRow->addWidget(m_nameLabel, 1);

    m_renameBtn = new QPushButton("Rename", this);
    m_renameBtn->setFixedHeight(26);
    headerRow->addWidget(m_renameBtn);
    connect(m_renameBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentFilename.empty()) return;
        bool ok = false;
        QString newName = QInputDialog::getText(this, "Rename Voice", "New name:",
            QLineEdit::Normal, m_nameLabel->text(), &ok);
        if (!ok || newName.trimmed().isEmpty()) return;
        QString safe = newName.trimmed();
        emit renameRequested(m_currentFilename);
        // Update display to the new name
        m_nameLabel->setText(safe);
    });

    layout->addLayout(headerRow);

    // ── Action buttons ────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);

    m_activateBtn = new QPushButton("Activate", this);
    m_activateBtn->setFixedHeight(30);
    btnRow->addWidget(m_activateBtn);
    connect(m_activateBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilename.empty())
            emit activateRequested(m_currentFilename);
    });

    m_editBtn = new QPushButton("Edit", this);
    m_editBtn->setFixedHeight(30);
    btnRow->addWidget(m_editBtn);
    connect(m_editBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilename.empty())
            emit editRequested(m_currentFilename);
    });

    m_exportBtn = new QPushButton("Export", this);
    m_exportBtn->setFixedHeight(30);
    btnRow->addWidget(m_exportBtn);
    connect(m_exportBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilename.empty())
            emit exportRequested(m_currentFilename);
    });

    m_deleteBtn = new QPushButton("Delete", this);
    m_deleteBtn->setFixedHeight(30);
    m_deleteBtn->setStyleSheet("QPushButton { color: #c44; }");
    btnRow->addWidget(m_deleteBtn);
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentFilename.empty()) return;
        auto reply = QMessageBox::question(this, "Delete Voice",
            "Are you sure you want to delete this voice?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes)
            emit deleteRequested(m_currentFilename);
    });

    layout->addLayout(btnRow);

    // ── Separator ─────────────────────────────────────────────────────────────
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep);

    // ── Effects section header ────────────────────────────────────────────────
    auto* effectsHeader = new QLabel("Effects", this);
    QFont ehFont = effectsHeader->font();
    ehFont.setPointSize(11);
    ehFont.setBold(true);
    effectsHeader->setFont(ehFont);
    layout->addWidget(effectsHeader);

    // ── Effects list container ─────────────────────────────────────────────────
    m_effectsContainer = new QWidget(this);
    m_effectsLayout = new QVBoxLayout(m_effectsContainer);
    m_effectsLayout->setContentsMargins(8, 4, 4, 4);
    m_effectsLayout->setSpacing(4);
    m_effectsLayout->addStretch();

    layout->addWidget(m_effectsContainer, 1);
    layout->addStretch();
}

void VoiceDetailsPanel::clear() {
    m_currentFilename.clear();
    m_isBuiltin = false;
    m_nameLabel->setText("(No voice selected)");
    m_renameBtn->setVisible(false);
    m_deleteBtn->setVisible(false);

    m_activateBtn->setEnabled(false);
    m_editBtn->setEnabled(false);
    m_exportBtn->setEnabled(false);

    // Clear effects list
    while (auto* child = m_effectsLayout->takeAt(0)) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
}

void VoiceDetailsPanel::setVoice(const std::string& filename) {
    m_currentFilename = filename;

    // Load profile metadata
    try {
        auto profile = m_profileManager.loadProfile(filename);
        m_isBuiltin = profile.builtin;
        m_nameLabel->setText(QString::fromStdString(profile.name));
        populateEffects(profile.data);
    } catch (const std::exception& e) {
        m_nameLabel->setText("(Error loading voice)");
        populateEffects(nlohmann::json::object());
    }

    // Rename/delete only for non-builtin
    m_renameBtn->setVisible(!m_isBuiltin);
    m_deleteBtn->setVisible(!m_isBuiltin);

    // All buttons always enabled when a voice is selected
    m_activateBtn->setEnabled(true);
    m_editBtn->setEnabled(true);
    m_exportBtn->setEnabled(true);
}

void VoiceDetailsPanel::populateEffects(const nlohmann::json& profileData) {
    // Clear existing effect items
    while (auto* child = m_effectsLayout->takeAt(0)) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    // Gather modulators per effect_id from the profile's modulator list
    std::map<std::string, std::vector<std::string>> modulatorsByEffect;
    if (profileData.contains("modulators") && profileData["modulators"].is_array()) {
        for (const auto& m : profileData["modulators"]) {
            std::string eid = m.value("effect_id", "");
            std::string pid = m.value("param_id", "");
            if (!eid.empty())
                modulatorsByEffect[eid].push_back(pid);
        }
    }

    // Count effects
    int effectCount = 0;
    if (profileData.contains("effects") && profileData["effects"].is_array())
        effectCount = static_cast<int>(profileData["effects"].size());

    // Header line showing total count
    auto* countLabel = new QLabel(QString("(%1)").arg(effectCount), this);
    countLabel->setStyleSheet("QLabel { color: #888; font-size: 11px; }");
    // Replace "Effects" header text — update it inline
    // (We embed the count directly in the effect list header below)

    if (effectCount == 0) {
        auto* emptyLbl = new QLabel("No effects", this);
        emptyLbl->setStyleSheet("QLabel { color: #666; font-style: italic; }");
        m_effectsLayout->insertWidget(0, emptyLbl);
        return;
    }

    // Iterate over effects array
    for (const auto& ej : profileData["effects"]) {
        std::string effectId = ej.value("id", "");
        if (effectId.empty()) continue;

        auto effect = EffectRegistry::instance().create(effectId);
        QString effectName = effect
            ? QString::fromStdString(effect->name())
            : QString::fromStdString(effectId);

        auto* row = new QHBoxLayout();
        row->setSpacing(4);

        // Bullet
        auto* bullet = new QLabel("•", this);
        bullet->setStyleSheet("QLabel { color: #aaa; }");
        row->addWidget(bullet);

        // Effect name
        auto* nameLbl = new QLabel(effectName, this);
        nameLbl->setStyleSheet("QLabel { color: #ddd; }");
        row->addWidget(nameLbl, 1);

        // Keyframe badge
        auto it = modulatorsByEffect.find(effectId);
        if (it != modulatorsByEffect.end() && !it->second.empty()) {
            int count = static_cast<int>(it->second.size());
            auto* badge = new QLabel(QString("(%1)").arg(count), this);
            badge->setStyleSheet(
                "QLabel { background-color: #2d4a2d; color: #8f8; "
                "border-radius: 4px; padding: 1px 5px; font-size: 10px; }");

            // Tooltip: join param IDs by comma
            QStringList paramList;
            for (const auto& p : it->second) paramList << QString::fromStdString(p);
            badge->setToolTip("Keyframed: " + paramList.join(", "));

            row->addWidget(badge);
        }

        // Wrap in a widget to add to layout
        auto* itemWidget = new QWidget(this);
        itemWidget->setLayout(row);
        m_effectsLayout->insertWidget(m_effectsLayout->count() - 1, itemWidget);
    }
}
