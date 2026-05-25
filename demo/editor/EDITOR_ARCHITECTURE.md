# OdysseyEngine Editor Architecture

## Overview

This document describes the architecture for a Unity-like GUI editor for OdysseyEngine using ImGui (Dear ImGui) with Vulkan integration.

## Core Requirements

1. **Unity-like interface** - hierarchy, inspector, scene view, game view
2. **CLI/MCP parity** - features accessible via GUI also work via CLI
3. **Level building** - drag-drop entities, transform gizmos
4. **UI building** - WYSIWYG editor
5. **Claude/OpenClaw integration** - AI assistant panel, mesh generation, behavior editing
6. **Real-time preview** - live gameplay preview

## Architecture

### Framework: Dear ImGui

- Lightweight, immediate-mode GUI
- Works with Vulkan via ImGui_Impl_Vulkan
- Dock system via ImGui::GetWindowDockNode() for panel layout
- Professional styling with ImGui::GetStyle()

### Panel Layout (Unity-style docks)

```
┌─────────────────────────────────────────────────────────────────┐
│  Menu Bar                                                       │
├────────┬───────────────────────────────────────┬───────────────┤
│        │                                       │               │
│  Hie-  │         Scene View                    │   Inspector   │
│  rarchy│         (Viewport)                    │   (Properties)│
│        │                                       │               │
│        │                                       ├───────────────┤
│        │                                       │   AI Assistant│
├────────┼───────────────────────────────────────┤               │
│        │         Game View                     │               │
│  Console│         (Preview)                    │               │
│        │                                       │               │
└────────┴───────────────────────────────────────┴───────────────┘
```

### Panel Components

1. **Menu Bar** - File, Edit, View, Game, Tools, Help
2. **Toolbar** - Play, Pause, Stop, Step, Gizmo modes (translate/rotate/scale)
3. **Scene Hierarchy** (left panel) - Tree view of all entities
4. **Scene View** (center) - 3D viewport with gizmos
5. **Game View** (center bottom) - Live gameplay preview
6. **Inspector** (right panel) - Property grid for selected entity
7. **Console** (bottom left) - Log output
8. **AI Assistant** (right bottom) - Claude API integration

### Core Subsystems

#### 1. Editor Engine (EditorEngine class)
- Runs engine in "editor mode" vs "play mode"
- Manages undo/redo stack
- Handles selection state
- Tracks dirty state for scenes

```cpp
class EditorEngine : public Engine {
public:
    // Editor-specific
    void enter_play_mode();
    void exit_play_mode();
    void step_frame();
    
    // Selection
    void select_entity(EntityId id);
    EntityId get_selected() const;
    
    // Undo/Redo
    void push_undo(ICommand* cmd);
    void undo();
    void redo();
};
```

#### 2. Entity Manager (extends existing)
- CRUD operations on entities
- Hierarchy management (parenting)
- Component inspection via reflection

#### 3. Inspector System
- Auto-generates UI from component properties
- Supports: floats, vectors, colors, enums, assets
- Custom editors for complex types (curves, curvesets)

```cpp
class Inspector {
public:
    void draw(EntityId selected);
    void draw_component(Transform& t);
    void draw_component(Mesh& m);
    void draw_component(Material& m);
    // ... auto-generated from reflection
};
```

#### 4. Scene Hierarchy
- Tree view with entity icons
- Drag-drop reordering
- Context menus (create, delete, duplicate)
- Search/filter

```cpp
class HierarchyPanel {
public:
    void draw();
    void draw_entity(EntityId id, int depth);
    void handle_drag_drop();
};
```

#### 5. Viewport (Scene View)
- 3D rendering with camera controls
- Gizmo system (translate, rotate, scale)
- Grid, axis visualization
- Selection picking

```cpp
class ViewportPanel {
public:
    void draw();
    void draw_gizmos();
    void handle_input();
    Camera& get_camera();
};
```

#### 6. AI Assistant Panel
- Chat interface with Claude API
- Context injection (selected entity, scene state)
- Quick actions: generate mesh, create behavior, write script

```cpp
class AIAssistantPanel {
public:
    void draw();
    void send_message(const std::string& msg);
    void on_response(const std::string& response);
    void add_context(const std::string& context);
};
```

#### 7. Transform Gizmos
- Translate (G) - RGB axis arrows
- Rotate (R) - RGB circles
- Scale (S) - RGB cubes

### CLI/MCP Parity

All GUI operations exposed via:

1. **CLI Commands** - `odyssey_cli entity create --type Cube --pos 0,0,0`
2. **MCP Tools** - JSON-RPC interface for external clients
3. **Python Bindings** - For scripting

```cpp
// MCP tool definitions (in src/mcp/tools.h)
{
    "name": "entity_create",
    "description": "Create a new entity",
    "parameters": {
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "components": {"type": "array"}
        }
    }
}
```

### Drag & Drop

- From hierarchy to hierarchy (reparent)
- From assets to viewport (instantiate)
- From hierarchy to inspector (focus)

### WYSIWYG UI Editor

- Canvas-based UI layout
- Drag-drop UI elements (buttons, images, text)
- Real-time preview
- Export to UI schema

### Integration Points

1. **Scene Loader** - Load/save .scene files
2. **Prefab System** - Instantiate prefabs
3. **Asset Browser** - Browse meshes, materials, behaviors
4. **Behavior Editor** - Edit Nadir shaders
5. **Mesh Generator** - Procedural mesh via AI

## Build Configuration

New CMake target:

```cmake
# Editor executable
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
  "demo/editor/*.cpp" "demo/editor/*.h"
)

add_executable(odyssey_editor ${ODYSSEY_MAIN_SOURCE} ${EDITOR_SOURCES})
target_link_libraries(odyssey_editor PRIVATE odyssey_engine)
```

Dependencies:
- Dear ImGui (via vcpkg or submodule)
- ImGui_Impl_Vulkan
- ImGui_Impl_GLFW

## File Structure

```
demo/editor/
├── editor_main.cpp        # Entry point
├── editor.h              # Main editor class
├── editor.cpp            
├── panels/
│   ├── hierarchy.cpp/h
│   ├── inspector.cpp/h
│   ├── viewport.cpp/h
│   ├── console.cpp/h
│   └── ai_assistant.cpp/h
├── widgets/
│   ├── gizmos.cpp/h
│   ├── toolbar.cpp/h
│   └── menu_bar.cpp/h
├── commands/
│   └── command.h         # Undo/redo commands
└── resources/
    └── editor.ini        # ImGui layout config
```

## Future Enhancements

1. **Multi-scene editing** - Tabs for multiple scenes
2. **Profiler panel** - Frame timing, memory
3. **Animation timeline** - Keyframe editing
4. **Particle system editor** - Visual particle editor
5. **Audio mixer** - Sound bank management
