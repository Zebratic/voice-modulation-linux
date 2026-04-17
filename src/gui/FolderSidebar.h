#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <string>

class ProfileManager;

class FolderSidebar : public QWidget {
    Q_OBJECT

public:
    explicit FolderSidebar(ProfileManager& profileManager, QWidget* parent = nullptr);
    ~FolderSidebar() override = default;

    void refresh();
    QString getSelectedFolderId() const;

signals:
    void voiceSelected(const std::string& filename);
    void voiceDoubleClicked(const std::string& filename);

public slots:
    void onNewFolderClicked();
    void onRenameFolder(QTreeWidgetItem* item);
    void onDeleteFolder(QTreeWidgetItem* item);
    void onNewSubfolder(QTreeWidgetItem* folderItem);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupUI();
    void populateTree();
    void buildFolderItems(QTreeWidgetItem* parentItem, const std::string& parentId);
    QTreeWidgetItem* findFolderItem(const std::string& folderId);
    void showFolderContextMenu(QTreeWidgetItem* item, const QPoint& pos);
    void showVoiceContextMenu(QTreeWidgetItem* item, const QPoint& pos);
    void onDragStarted(const QString& filename);

    static constexpr int MaxFolderDepth = 5;
    static const inline QString BuiltinFolderId = QStringLiteral("builtin");

    ProfileManager& m_profileManager;
    QTreeWidget* m_tree;
    QPushButton* m_newFolderBtn;
};
