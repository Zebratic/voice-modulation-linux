# Voice Foldering System Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement folder-based organization for voice profiles with drag-drop support, nested folders (max 5 levels), and persistent storage.

**Architecture:** Replace grid layout with master-detail view using Qt's model/view architecture. Folder structure stored in JSON with folder tree widget handling expand/collapse, drag-drop, and context menus. Voice details panel shows selected voice info with action buttons and compact effects list.

**Tech Stack:** Qt5/C++, QTreeWidget for folder tree, QMimeData for drag-drop, nlohmann/json for persistence, QSettings for app settings.

---

## Task 1: Folder Structure Data Layer

**Files:**
- Modify: `src/profile/ProfileManager.h`
- Modify: `src/profile/ProfileManager.cpp`
- Test: `tests/test_folder_structure.cpp`

### Implementation Steps

**Step 1: Add folder structure classes and methods to header**

Add to `src/profile/ProfileManager.h`:

```cpp
// Folder structure for voice organization
struct VoiceFolder {
    std::string id;           // UUID or "builtin" sentinel
    std::string name;         // Display name
    std::string parentId;     // null for root-level folders
    bool expanded = true;
    int order = 0;
};

struct FolderStructure {
    int version = 1;
    std::vector<VoiceFolder> folders;
    std::map<std::string, std::string> voiceAssignments; // filename -> folderId
};

// Folder management methods
void saveFolderStructure(const FolderStructure& structure) const;
FolderStructure loadFolderStructure() const;
void moveVoiceToFolder(const std::string& voiceFilename, const std::string& folderId);
std::vector<std::string> getVoicesInFolder(const std::string& folderId) const;
int getFolderDepth(const std::string& folderId) const;
```

**Step 2: Implement folder persistence methods**

Implement `saveFolderStructure`, `loadFolderStructure`, `moveVoiceToFolder`, `getVoicesInFolder`, and `getFolderDepth` in `src/profile/ProfileManager.cpp`.

Default behavior on first run: Create Built-in folder, assign all builtin voices to it, user voices at root level.

**Step 3: Write unit tests**

Create `tests/test_folder_structure.cpp` with tests for initialization, move operations, and depth calculation.

**Step 4: Commit**

---

## Task 2: Create FolderSidebar Widget

**Files:**
- Create: `src/gui/FolderSidebar.h`
- Create: `src/gui/FolderSidebar.cpp`

### Implementation Steps

**Step 1: Create header with signals, slots, and drag-drop support**

Use QTreeWidget for folder tree with custom context menus.

**Step 2: Implement folder tree population**

- "All Voices" virtual item at top
- "Built-in" folder (locked, immutable)
- User folders with expand/collapse
- Voices nested inside their folders

**Step 3: Implement context menus**

- Right-click folders: New Subfolder, Rename, Delete
- Right-click voices: Move to..., Delete
- Enforce constraints (Built-in lock, max depth)

**Step 4: Implement drag-drop**

- Enable drag on voice items
- Validate drop targets (not Built-in, depth limit)
- Update folder assignments on drop

**Step 5: Commit**

---

## Task 3: Create VoiceDetailsPanel Widget

**Files:**
- Create: `src/gui/VoiceDetailsPanel.h`
- Create: `src/gui/VoiceDetailsPanel.cpp`

### Implementation Steps

**Step 1: Create panel layout**

- Header: icon + name + rename button
- Action buttons: Activate, Edit, Export, Delete
- Effects list section
- Delete confirmation handled in MainWindow

**Step 2: Implement effects display**

Parse profile JSON, count keyframes per effect, display as:
```
Effects (3)
• Pitch Shift (2)
• Gain
• Echo
```

Badge shows keyframe count, tooltip shows which params.

**Step 3: Implement signals**

- `activateRequested`
- `editRequested`
- `exportRequested`
- `deleteRequested`
- `renameRequested`

**Step 4: Commit**

---

## Task 4: Integrate Sidebar and Details Panel into MainWindow

**Files:**
- Modify: `src/gui/MainWindow.h`
- Modify: `src/gui/MainWindow.cpp`

### Implementation Steps

**Step 1: Add new members**

Add `FolderSidebar*`, `VoiceDetailsPanel*` members and forward declarations.

**Step 2: Replace createVoicesTab()**

Replace grid layout with:
- Left: FolderSidebar (280px fixed)
- Vertical divider
- Right: Details panel with New Voice, Import, Export Folder buttons

**Step 3: Connect signals**

Connect sidebar selection to details panel, panel actions to handlers.

**Step 4: Add rename and export methods**

- `renameVoice()`
- `exportCurrentFolder()`

**Step 5: Update rebuildVoiceGrid()**

Legacy method now refreshes sidebar instead of grid.

**Step 6: Commit**

---

## Task 5: Implement Drag-Drop for Voice Organization

**Files:**
- Modify: `src/gui/FolderSidebar.cpp`

### Implementation Steps

**Step 1: Override drag event handlers**

- `dragEnterEvent()`
- `dragMoveEvent()`
- `dropEvent()`

**Step 2: Set drag-enabled on voice items**

**Step 3: Validate drop targets**

- Not Built-in folder
- Not exceeding max depth (5)

**Step 4: Update folder assignments on successful drop**

**Step 5: Commit**

---

## Task 6: Implement Folder Export as ZIP

**Files:**
- Modify: `src/profile/ProfileManager.h`
- Modify: `src/profile/ProfileManager.cpp`

### Implementation Steps

**Step 1: Add export methods**

```cpp
void exportFolderAsZip(const std::string& folderId, const fs::path& destPath) const;
void exportAllAsZip(const fs::path& destPath) const;
```

**Step 2: Implement ZIP creation**

Use miniz or QuaZip for proper ZIP handling. Structure inside ZIP mirrors folder structure.

**Step 3: Update MainWindow exportCurrentFolder()**

**Step 4: Commit**

---

## Task 7: Fix Delete Confirmation Dialog

**Files:**
- Modify: `src/gui/MainWindow.cpp`

### Implementation Steps

**Step 1: Update delete dialog message**

Include voice name in confirmation dialog. Emphasize permanence.

**Step 2: Refresh sidebar after deletion**

**Step 3: Commit**

---

## Task 8: Polish and Edge Cases

**Files:**
- Modify: `src/gui/FolderSidebar.cpp`
- Modify: `src/gui/VoiceDetailsPanel.cpp`

### Implementation Steps

**Step 1: Handle empty selection**

Clear details panel when clicking non-voice items.

**Step 2: Update activate button state**

Show correct state for active voice.

**Step 3: Handle folder depth warnings**

Show message when trying to create folder at max depth.

**Step 4: Commit**

---

## Task 9: Build and Test

**Step 1: Build**

```bash
cd build && cmake .. && make -j4
```

**Step 2: Run and test manually**

All interactions from design document.

---

## Summary of Files

| File | Action |
|------|--------|
| `src/profile/ProfileManager.h` | Modify |
| `src/profile/ProfileManager.cpp` | Modify |
| `src/gui/FolderSidebar.h` | Create |
| `src/gui/FolderSidebar.cpp` | Create |
| `src/gui/VoiceDetailsPanel.h` | Create |
| `src/gui/VoiceDetailsPanel.cpp` | Create |
| `src/gui/MainWindow.h` | Modify |
| `src/gui/MainWindow.cpp` | Modify |
| `tests/test_folder_structure.cpp` | Create |
