# Voice Foldering System Design

## Overview

Implement a folder-based organization system for voice profiles in VML, replacing the current grid layout with a master-detail view featuring drag-and-drop support, nested folders (max 5 levels), and persistent storage.

---

## Layout Structure

### Master-Detail Layout
```
┌─────────────────────┬────────────────────────────────────────┐
│  Voice Sidebar      │  Voice Details Panel                  │
│  (250px fixed)      │  (flexible width)                    │
│                     │                                        │
│  📁 All Voices      │  [Voice name]           [Rename]     │
│  📁 Built-in        │                                        │
│    ├─ Deep Voice    │  [Activate] [Edit] [Export] [Delete]│
│    ├─ Robot         │                                        │
│    └─ ...           │  ──────────────────────────────────── │
│  📁 My Folders       │                                        │
│    └─ ...           │  Effects (n)                          │
│                     │  • Effect Name (badge)                 │
│  [+ New Folder]     │  • Effect Name                        │
│                     │                                        │
└─────────────────────┴────────────────────────────────────────┘
```

---

## Folder Sidebar

### Structure
- **Built-in folder**: Immutable, always at top, contains 14 default voices
- **User folders**: Expandable/collapsible, can be nested up to 5 levels
- **Root level**: "All Voices" virtual node shows flat list of all voices

### Interactions
| Action | Behavior |
|--------|----------|
| Click folder | Expand/collapse |
| Click voice | Select, show details in right panel |
| Double-click voice | Open Edit Voice tab |
| Drag voice to folder | Move voice into folder |
| Drag voice to "All Voices" | Move to root (uncategorized) |
| Right-click folder | Context menu (Rename, Delete, New Subfolder) |
| Right-click voice | Context menu (Rename, Move to..., Delete) |

### Constraints
- Cannot drop voices into Built-in folder
- Cannot delete Built-in folder
- Cannot create folder if parent is at depth 5
- Cannot rename Built-in folder

### Visual Indicators
- Lock icon on Built-in folder
- Drop indicator line when dragging
- Folder depth indentation (16px per level)
- Folder/voice icons (folder vs. speaker icon)

---

## Voice Details Panel

### Header Section
```
┌─────────────────────────────────────────┐
│ 🎤 [Voice Name]              [Rename ✏️]│
└─────────────────────────────────────────┘
```
- Voice icon + editable name
- Rename button opens inline edit or dialog
- For built-in voices: name is read-only, no rename button

### Action Buttons
```
[ Activate ]  - Green if active, toggles voice
[ Edit ]       - Opens Edit Voice tab with this voice loaded
[ Export ]     - Saves single voice JSON file
[ Delete ]     - Shows confirmation dialog (disabled for built-in)
```

### Effects Section
```
Effects (3)
• Pitch Shift (2)    ← badge if keyframes present
• Gain
• Echo
```

- Vertical compact list
- Badge shows keyframe count: `(n)`
- Hover on badge shows tooltip: "Mix, Frequency" (keyframed params)

### Delete Confirmation
```
┌─────────────────────────────────────────┐
│ Delete Voice?                           │
│                                         │
│ "My Custom Voice" will be permanently  │
│ deleted and cannot be recovered.         │
│                                         │
│           [Cancel]  [Delete]            │
└─────────────────────────────────────────┘
```

---

## Keyframe Badge Implementation

### Data Structure
Each modulator in `modulators` array has:
```json
{
  "effect_id": "pitch_shift",
  "param_id": "mix",
  ...
}
```

### Badge Logic
1. Group modulators by `effect_id`
2. Count modulators per effect
3. On hover: collect all `param_id` values, join with comma

### Example
If pitch_shift has modulators on "mix" and "semitones":
- Badge shows: `(2)`
- Tooltip: "Mix, Semitones"

---

## Data Persistence

### File Location
`~/.config/vml/folder-structure.json`

### Schema
```json
{
  "version": 1,
  "folders": [
    {
      "id": "uuid-1",
      "name": "Gaming",
      "parentId": null,
      "expanded": true,
      "order": 0
    },
    {
      "id": "uuid-2",
      "name": "Streaming",
      "parentId": "uuid-1",
      "expanded": false,
      "order": 1
    }
  ],
  "voiceAssignments": {
    "my-voice.json": "uuid-1",
    "robot.json": null  // null = root level
  }
}
```

### Persistence Rules
- Auto-save on every structure change
- Load on application start
- Migrate gracefully if file is missing/corrupt

### Default Behavior
- If no file exists: create with built-in voices assigned to Built-in folder, user voices at root
- Built-in folder ID is always `"builtin"` (special sentinel)

---

## Export Functionality

### Export Single Voice
- Existing behavior: single JSON file via save dialog

### Export Folder
- Creates ZIP file containing all voice JSONs
- Filename: `[FolderName]-voices.zip`
- Structure inside ZIP:
  ```
  Gaming-voices.zip
  ├── voice1.json
  ├── voice2.json
  └── subfolder/
      └── voice3.json
  ```

### Export All Voices
- Export entire library as ZIP
- Maintains folder structure in ZIP

---

## Component Changes

### Files to Modify
1. `src/gui/MainWindow.h` - Add sidebar members, folder state
2. `src/gui/MainWindow.cpp` - Replace grid with master-detail layout
3. `src/profile/ProfileManager.h` - Add folder structure methods
4. `src/profile/ProfileManager.cpp` - Implement folder persistence

### New Files
1. `src/gui/FolderSidebar.h` - Folder tree widget
2. `src/gui/FolderSidebar.cpp` - Drag-drop, context menus
3. `src/gui/VoiceDetailsPanel.h` - Right panel widget
4. `src/gui/VoiceDetailsPanel.cpp` - Voice info display

---

## Technical Approach

### Qt Components
- `QTreeWidget` for folder tree (or custom widget)
- `QMimeData` for drag-drop
- `QTimer` for auto-save debouncing
- Standard widgets for details panel

### Drag-Drop Implementation
- Set `setDragEnabled(true)` and `setAcceptDrops(true)`
- Override `dragEnterEvent`, `dragMoveEvent`, `dropEvent`
- Validate drop target (not Built-in, depth limit)
- Update data model and persist

### Keyframe Analysis
- Load profile JSON when voice selected
- Iterate `modulators` array, group by `effect_id`
- Count and extract `param_id` for tooltip

---

## Testing Checklist

- [ ] Built-in folder always at top, locked
- [ ] Create/rename/delete user folders works
- [ ] Nest folders up to 5 levels (block at 6)
- [ ] Drag voice to folder moves it
- [ ] Drag voice to "All Voices" moves to root
- [ ] Built-in folder rejects drops
- [ ] Voice details panel updates on selection
- [ ] Keyframe badges show correct count
- [ ] Hover tooltip shows parameter names
- [ ] Delete shows confirmation, works for custom voices
- [ ] Export folder creates valid ZIP
- [ ] Folder structure persists across restarts
- [ ] No crashes on corrupted/missing data file

---

## Implementation Order

1. **Data Layer**: Folder structure persistence in ProfileManager
2. **Sidebar Widget**: Basic folder tree with expand/collapse
3. **Details Panel**: Voice info display, action buttons
4. **Drag-Drop**: Move voices between folders
5. **Context Menus**: Rename, delete, new folder
6. **Export**: Folder export as ZIP
7. **Polish**: Animations, hover states, edge cases
