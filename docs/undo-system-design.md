# Moon Engine - Undo/Redo System Design

## Overview

Command Pattern based Undo/Redo system with full scene state snapshot support.

---

## Architecture

```
User Action → Command → UndoManager → Engine API + UI Store Update
                ↓
        [undoStack, redoStack]
                ↓
        Ctrl+Z / Ctrl+Y
```

### Core Components

**Location**: `editor/webui/src/undo/`

- `Command.ts` - Command interface + MultiCommand
- `UndoManager.ts` - Command stack manager
- `TransformCommands.ts` - Position/Rotation/Scale commands
- `NodeCommands.ts` - Create/Delete/Rename/SetParent/SetActive commands
- `PrimitiveCommands.ts` - Create primitive commands
- `gizmoUndo.ts` - Gizmo drag integration
- `useUndoShortcuts.ts` - Keyboard shortcuts (Ctrl+Z, Ctrl+Y)

---

## Design Principles

### 1. Snapshot at Construction
Commands capture all necessary state at construction time, **not** in `execute()`.

```typescript
// ✅ Good
constructor(nodeId: number, oldName: string, newName: string) {
  this.nodeId = nodeId;
  this.oldName = oldName;  // Captured at construction
  this.newName = newName;
}

// ❌ Bad
execute() {
  this.oldName = await engine.getNodeName(this.nodeId);  // Query in execute
}
```

### 2. Execute = Redo (Idempotent)
`execute()` and `redo()` must be identical. Both call `execute()`.

```typescript
execute() {
  engine.setPosition(this.nodeId, this.newPosition);
  updateStore(this.nodeId, this.newPosition);
}

// UndoManager.redo() internally calls command.execute()
```

### 3. Minimal Snapshot
Only save necessary data, not entire scene state.

```typescript
// ✅ Minimal
class RenameNodeCommand {
  private nodeId: number;
  private oldName: string;
  private newName: string;
}

// ❌ Wasteful
class RenameNodeCommand {
  private entireScene: Scene;  // Too much
}
```

### 4. Command Independence
Commands must be self-contained with no external dependencies.

```typescript
// ✅ Self-contained
class SetPositionCommand {
  constructor(nodeId: number, oldPos: Vector3, newPos: Vector3) {
    this.nodeId = nodeId;
    this.oldPos = { ...oldPos };  // Deep copy
    this.newPos = { ...newPos };
  }
}
```

---

## Implementation Patterns

### Pattern 1: Simple Property Change

For properties that can be directly set/restored:

```typescript
class SetPositionCommand implements Command {
  constructor(
    private nodeId: number,
    private oldPosition: Vector3,
    private newPosition: Vector3
  ) {}

  execute(): void {
    engine.setPosition(this.nodeId, this.newPosition);
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.newPosition
    });
  }

  undo(): void {
    engine.setPosition(this.nodeId, this.oldPosition);
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.oldPosition
    });
  }
}
```

### Pattern 2: Deferred Snapshot (Create Node)

For operations where state is unknown until first execution:

```typescript
class CreateNodeCommand implements Command {
  private serializedNodeData?: string;  // Saved after first execute
  private createdNodeId?: number;

  async execute(): Promise<void> {
    if (this.serializedNodeData) {
      // Redo: Deserialize snapshot
      await engine.deserializeNode(this.serializedNodeData);
    } else {
      // First execute: Create node and save snapshot
      await engine.createNode(this.nodeType, this.parentId);
      const scene = await engine.getScene();
      // Find newly created node...
      this.serializedNodeData = await engine.serializeNode(this.createdNodeId);
    }
  }

  async undo(): Promise<void> {
    await engine.deleteNode(this.createdNodeId);
  }
}
```

### Pattern 3: Pre-Serialization (Delete Node)

For operations that destroy data - snapshot **before** execution:

```typescript
class DeleteNodeCommand implements Command {
  private constructor(
    private nodeId: number,
    private serializedNodeData: string  // Captured at construction
  ) {}

  // Static factory: Serialize before construction
  static async create(nodeId: number): Promise<DeleteNodeCommand> {
    const serializedData = await engine.serializeNode(nodeId);
    return new DeleteNodeCommand(nodeId, serializedData);
  }

  async execute(): Promise<void> {
    await engine.deleteNode(this.nodeId);
  }

  async undo(): Promise<void> {
    // Restore from snapshot
    await engine.deserializeNode(this.serializedNodeData);
  }
}

// Usage
const cmd = await DeleteNodeCommand.create(nodeId);
undoManager.execute(cmd);
```

---

## Gizmo Integration

Gizmo manipulation (drag/rotate/scale) uses callback-based recording:

### C++ Side (`EditorApp_Gizmo.cpp`)
```cpp
// Detect drag start
if (!g_WasUsingGizmo && usingGizmo) {
  ExecuteJavaScript("window.onGizmoStart(" + nodeId + ")");
}

// Detect drag end
if (g_WasUsingGizmo && !usingGizmo) {
  ExecuteJavaScript("window.onGizmoEnd(" + nodeId + ", pos, rot, scale)");
}
```

### JS Side (`gizmoUndo.ts`)
```typescript
export function onGizmoStart(nodeId: number): void {
  // Record initial transform state
  gizmoStartState = { nodeId, startPosition, startRotation, startScale };
}

export function onGizmoEnd(nodeId, endPos, endRot, endScale): void {
  // Create SetTransformCommand with old→new state
  const command = new SetTransformCommand(nodeId, startState, endState);
  
  // Push without execute (C++ already modified transform)
  undoManager.pushExecutedCommand(command);
}
```

**Key**: C++ applies transform in real-time, JS only records for undo.

---

## API Integration

### Inspector Property Changes
```typescript
// Inspector.tsx
const handlePositionChange = (axis: 'x' | 'y' | 'z', value: number) => {
  const oldPos = selectedNode.transform.position;
  const newPos = { ...oldPos, [axis]: value };
  
  const command = new SetPositionCommand(selectedNode.id, oldPos, newPos);
  undoManager.execute(command);
};
```

### Toolbar Node Creation
```typescript
// Toolbar.tsx
const handleCreateCube = () => {
  const command = new CreatePrimitiveCommand('cube', undefined);
  undoManager.execute(command);
};
```

### Hierarchy Node Deletion
```typescript
// Hierarchy.tsx
const handleDeleteNode = async (nodeId: number) => {
  const command = await DeleteNodeCommand.create(nodeId);
  undoManager.execute(command);
};
```

### Hierarchy Parent Change
```typescript
// Hierarchy.tsx (drag-drop)
const handleParentChange = (nodeId: number, oldParent: number, newParent: number) => {
  const command = new SetParentCommand(nodeId, oldParent, newParent);
  undoManager.execute(command);
};
```

---

## Keyboard Shortcuts

**File**: `useUndoShortcuts.ts`

```typescript
useEffect(() => {
  const handleKeyDown = (e: KeyboardEvent) => {
    if (e.ctrlKey && e.key === 'z') {
      undoManager.undo();
    } else if (e.ctrlKey && e.key === 'y') {
      undoManager.redo();
    }
  };
  window.addEventListener('keydown', handleKeyDown);
  return () => window.removeEventListener('keydown', handleKeyDown);
}, []);
```

---

## C++ Engine Support

### Required APIs
```cpp
// Serialization
std::string serializeNode(uint32_t nodeId);
void deserializeNode(const std::string& data);

// Node operations
void createNode(const std::string& type, uint32_t parentId);
void deleteNode(uint32_t nodeId);
void renameNode(uint32_t nodeId, const std::string& name);
void setNodeActive(uint32_t nodeId, bool active);
void setNodeParent(uint32_t nodeId, uint32_t parentId);

// Transform operations
void setPosition(uint32_t nodeId, const Vector3& pos);
void setRotation(uint32_t nodeId, const Quaternion& rot);
void setScale(uint32_t nodeId, const Vector3& scale);

// Scene query
Scene getScene();
```

### ID Management
```cpp
// SceneNode.cpp
SceneNode::SceneNode(uint32_t id, const std::string& name) {
  m_id = id;
  // Prevent ID conflicts
  if (id >= s_nextID) {
    s_nextID = id + 1;
  }
}
```

---

## Configuration

```typescript
// App.tsx or initialization
const undoManager = getUndoManager({
  maxHistorySize: 50,  // Default: 50
  debug: false         // Enable console logs
});
```

---

## Limitations & Future Work

### Current Limitations
- ❌ Node deletion only saves basic components (no custom components)
- ❌ Materials not saved/restored
- ❌ CreateNodeCommand is async but UndoManager expects sync

### Future Enhancements
- [ ] Transaction support (`beginTransaction` / `endTransaction`)
- [ ] Command merging (merge consecutive small changes)
- [ ] Undo history UI panel
- [ ] Component serialization (scripts, colliders, etc.)
- [ ] Material serialization
- [ ] Async command support in UndoManager

---

## Testing

Test files:
- `simpleHierarchyTest.ts` - Node hierarchy operations
- `nodePropertiesTest.ts` - Rename and active state

Run tests:
```typescript
window.runSimpleHierarchyTest();
window.runNodePropertiesTest();
```

Test coverage:
- ✅ Create/Delete nodes with hierarchy
- ✅ Rename nodes
- ✅ Set node active state
- ✅ Transform modifications
- ✅ Full Undo/Redo cycles
- ✅ Gizmo drag operations

---

**Version**: 2.0  
**Status**: Production Ready  
**Last Updated**: 2025-11-30
