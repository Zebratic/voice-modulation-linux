#include "gui/FolderSidebar.h"
#include "profile/ProfileManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QMenu>
#include <QHeaderView>
#include <QVariant>
#include <QDebug>

FolderSidebar::FolderSidebar(ProfileManager& profileManager, QWidget* parent)
    : QWidget(parent)
    , m_profileManager(profileManager)
    , m_tree(nullptr)
    , m_newFolderBtn(nullptr)
{
    setupUI();
    refresh();
}

void FolderSidebar::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(16);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setDragDropMode(QAbstractItemView::DragDrop);

    layout->addWidget(m_tree, 1);

    m_newFolderBtn = new QPushButton("+ New Folder", this);
    m_newFolderBtn->setObjectName("newFolderBtn");
    layout->addWidget(m_newFolderBtn);

    connect(m_newFolderBtn, &QPushButton::clicked, this, &FolderSidebar::onNewFolderClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &FolderSidebar::onTreeItemDoubleClicked);
    connect(m_tree, &QTreeWidget::itemClicked,
            this, &FolderSidebar::onTreeItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
                QTreeWidgetItem* item = m_tree->itemAt(pos);
                if (!item) return;
                showFolderContextMenu(item, m_tree->viewport()->mapToGlobal(pos));
            });
}

void FolderSidebar::refresh() {
    m_tree->clear();
    populateTree();
}

void FolderSidebar::populateTree() {
    FolderStructure structure = m_profileManager.loadFolderStructure();

    // "All Voices" virtual item at top (represents root / all unfiled voices)
    QTreeWidgetItem* allVoicesItem = new QTreeWidgetItem(m_tree);
    allVoicesItem->setText(0, "All Voices");
    allVoicesItem->setData(0, Qt::UserRole, QVariant::fromValue(QStringLiteral("__all__")));
    allVoicesItem->setData(0, Qt::UserRole + 1, QVariant::fromValue(QStringLiteral("all_voices")));
    allVoicesItem->setFlags(allVoicesItem->flags() & ~Qt::ItemIsDragEnabled);

    // Build user folder tree recursively from root (empty parentId)
    buildFolderItems(nullptr, "");

    m_tree->expandAll();
}

void FolderSidebar::buildFolderItems(QTreeWidgetItem* parentItem, const std::string& parentId) {
    FolderStructure structure = m_profileManager.loadFolderStructure();

    // Find folders with this parentId, sorted by order
    std::vector<VoiceFolder> children;
    for (const auto& f : structure.folders) {
        if (f.parentId == parentId) {
            children.push_back(f);
        }
    }
    std::sort(children.begin(), children.end(),
              [](const VoiceFolder& a, const VoiceFolder& b) {
                  return a.order < b.order;
              });

    for (const auto& folder : children) {
        QTreeWidgetItem* folderItem = parentItem
            ? new QTreeWidgetItem(parentItem)
            : new QTreeWidgetItem(m_tree);

        bool isBuiltin = (folder.id == "builtin");
        QString label = isBuiltin
            ? QString::fromStdString(folder.name) + " \xE2\x9D\x94"  // padlock emoji
            : QString::fromStdString(folder.name);
        folderItem->setText(0, label);
        folderItem->setData(0, Qt::UserRole, QVariant::fromValue(QString::fromStdString(folder.id)));
        folderItem->setData(0, Qt::UserRole + 1, QVariant::fromValue(QStringLiteral("folder")));
        folderItem->setData(0, Qt::UserRole + 2, QVariant::fromValue(isBuiltin));

        // Prevent drag on builtin folder
        if (isBuiltin) {
            folderItem->setFlags(folderItem->flags()
                & ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
        } else {
            folderItem->setFlags(folderItem->flags() | Qt::ItemIsDropEnabled);
        }

        // Recurse into subfolders
        buildFolderItems(folderItem, folder.id);

        // Add voices assigned to this folder
        std::vector<std::string> voices = m_profileManager.getVoicesInFolder(folder.id);
        for (const auto& voiceFilename : voices) {
            try {
                auto profile = m_profileManager.loadProfile(voiceFilename);
                QTreeWidgetItem* voiceItem = new QTreeWidgetItem(folderItem);
                voiceItem->setText(0, QString::fromStdString(profile.name) + " \xF0\x9F\x90\xA3");
                voiceItem->setData(0, Qt::UserRole, QVariant::fromValue(
                    QString::fromStdString(voiceFilename)));
                voiceItem->setData(0, Qt::UserRole + 1, QVariant::fromValue(QStringLiteral("voice")));
                voiceItem->setData(0, Qt::UserRole + 2, QVariant::fromValue(profile.builtin));

                if (profile.builtin) {
                    voiceItem->setFlags(voiceItem->flags() & ~Qt::ItemIsDragEnabled);
                }
            } catch (const std::exception&) {
                // Skip voices whose files are missing
            }
        }

        if (folder.expanded) {
            folderItem->setExpanded(true);
        }
    }

    // For the "All Voices" virtual item, also list root-level voices (unfiled)
    if (parentItem && parentItem->data(0, Qt::UserRole + 1).toString() == QStringLiteral("all_voices")) {
        FolderStructure structure = m_profileManager.loadFolderStructure();
        auto profiles = m_profileManager.listProfiles();
        for (const auto& profile : profiles) {
            // Voice is root-level if it has no assignment
            if (structure.voiceAssignments.find(profile.filename) != structure.voiceAssignments.end())
                continue;

            QTreeWidgetItem* voiceItem = new QTreeWidgetItem(parentItem);
            voiceItem->setText(0, QString::fromStdString(profile.name) + " \xF0\x9F\x90\xA3");
            voiceItem->setData(0, Qt::UserRole, QVariant::fromValue(
                QString::fromStdString(profile.filename)));
            voiceItem->setData(0, Qt::UserRole + 1, QVariant::fromValue(QStringLiteral("voice")));
            voiceItem->setData(0, Qt::UserRole + 2, QVariant::fromValue(profile.builtin));

            if (profile.builtin) {
                voiceItem->setFlags(voiceItem->flags() & ~Qt::ItemIsDragEnabled);
            }
        }
    }
}

void FolderSidebar::onNewFolderClicked() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "New Folder", "Folder name:",
                                          QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    FolderStructure structure = m_profileManager.loadFolderStructure();

    VoiceFolder newFolder;
    newFolder.id = ProfileManager::generateFolderId();
    newFolder.name = name.trimmed().toStdString();
    newFolder.parentId = "";
    newFolder.expanded = true;
    newFolder.order = static_cast<int>(structure.folders.size());

    structure.folders.push_back(newFolder);
    m_profileManager.saveFolderStructure(structure);
    refresh();
}

void FolderSidebar::onNewSubfolder(QTreeWidgetItem* folderItem) {
    QString folderId = folderItem->data(0, Qt::UserRole).toString();

    int depth = m_profileManager.getFolderDepth(folderId.toStdString());
    if (depth >= MaxFolderDepth) {
        QMessageBox::warning(this, "Depth Limit",
            QString("Folders can only be nested up to %1 levels deep.").arg(MaxFolderDepth));
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "New Subfolder", "Folder name:",
                                          QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    FolderStructure structure = m_profileManager.loadFolderStructure();

    VoiceFolder newFolder;
    newFolder.id = ProfileManager::generateFolderId();
    newFolder.name = name.trimmed().toStdString();
    newFolder.parentId = folderId.toStdString();
    newFolder.expanded = true;
    newFolder.order = 0;

    structure.folders.push_back(newFolder);
    m_profileManager.saveFolderStructure(structure);
    refresh();
}

void FolderSidebar::onRenameFolder(QTreeWidgetItem* item) {
    QString folderId = item->data(0, Qt::UserRole).toString();
    if (folderId == BuiltinFolderId)
        return; // Cannot rename built-in

    bool ok = false;
    QString currentName = item->text(0);
    // Strip trailing lock emoji if present
    if (currentName.endsWith(" \xE2\x9D\x94"))
        currentName.chop(3);
    QString name = QInputDialog::getText(this, "Rename Folder", "New name:",
                                          QLineEdit::Normal, currentName, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    FolderStructure structure = m_profileManager.loadFolderStructure();
    for (auto& f : structure.folders) {
        if (f.id == folderId.toStdString()) {
            f.name = name.trimmed().toStdString();
            break;
        }
    }
    m_profileManager.saveFolderStructure(structure);
    refresh();
}

void FolderSidebar::onDeleteFolder(QTreeWidgetItem* item) {
    QString folderId = item->data(0, Qt::UserRole).toString();
    if (folderId == BuiltinFolderId)
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Delete Folder",
        "Delete this folder? Voices inside will be moved to the root level.",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    FolderStructure structure = m_profileManager.loadFolderStructure();

    // Move child voices to root
    for (auto it = structure.voiceAssignments.begin();
         it != structure.voiceAssignments.end(); ) {
        if (it->second == folderId.toStdString()) {
            it = structure.voiceAssignments.erase(it);
        } else {
            ++it;
        }
    }

    // Move subfolder children to root and delete subfolders recursively
    std::vector<std::string> toDelete;
    std::function<void(const std::string&)> collectDescendants =
        [&](const std::string& parent) {
            for (const auto& f : structure.folders) {
                if (f.parentId == parent) {
                    toDelete.push_back(f.id);
                    collectDescendants(f.id);
                }
            }
        };
    toDelete.push_back(folderId.toStdString());
    collectDescendants(folderId.toStdString());

    std::erase_if(structure.folders,
                   [&](const VoiceFolder& f) {
                       return std::find(toDelete.begin(), toDelete.end(), f.id) != toDelete.end();
                   });

    m_profileManager.saveFolderStructure(structure);
    refresh();
}

void FolderSidebar::showFolderContextMenu(QTreeWidgetItem* item, const QPoint& globalPos) {
    QString type = item->data(0, Qt::UserRole + 1).toString();
    QString folderId = item->data(0, Qt::UserRole).toString();
    bool isBuiltin = item->data(0, Qt::UserRole + 2).toBool();

    QMenu menu;

    if (type == QStringLiteral("all_voices")) {
        menu.addAction("New Folder", this, &FolderSidebar::onNewFolderClicked);
        menu.exec(globalPos);
        return;
    }

    if (type != QStringLiteral("folder"))
        return;

    if (!isBuiltin) {
        QAction* newSubAction = menu.addAction("New Subfolder", this, [this, item]() {
            onNewSubfolder(item);
        });
        int depth = m_profileManager.getFolderDepth(folderId.toStdString());
        if (depth >= MaxFolderDepth)
            newSubAction->setEnabled(false);

        menu.addAction("Rename", this, [this, item]() {
            onRenameFolder(item);
        });
        menu.addSeparator();
        menu.addAction("Delete", this, [this, item]() {
            onDeleteFolder(item);
        });
    }

    if (!menu.isEmpty())
        menu.exec(globalPos);
}

void FolderSidebar::onTreeItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    QString type = item->data(0, Qt::UserRole + 1).toString();
    if (type != QStringLiteral("voice"))
        return;

    QString filename = item->data(0, Qt::UserRole).toString();
    emit voiceDoubleClicked(filename.toStdString());
}

void FolderSidebar::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    QString type = item->data(0, Qt::UserRole + 1).toString();
    if (type != QStringLiteral("voice"))
        return;

    QString filename = item->data(0, Qt::UserRole).toString();
    emit voiceSelected(filename.toStdString());
}
