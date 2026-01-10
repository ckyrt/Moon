# CSG用户界面设计方案

## 📌 设计目标

根据 [MVP_DEFINITION.md](MVP_DEFINITION.md) 和产品愿景，CSG功能应该：

1. **分层暴露**：L1用户（Builder）不可见，L2用户（Asset Builder）可用
2. **直观易用**：参考Roblox但更适合写实建模
3. **工作流完整**：创建图元 → 布尔运算 → 封装资产 → 上传服务器

---

## 🎯 与Roblox CSG的对比

### Roblox CSG工作流

```
1. 创建Part（基础图元）
2. 选中两个Part
3. 右键菜单 → Union/Negate/Intersect
4. 生成UnionOperation对象
5. 直接在游戏中使用
```

**特点**:
- ✅ 简单直观
- ✅ 实时预览
- ⚠️ 适合体素风格，不适合精细建模

### Moon Engine CSG工作流

```
1. 进入Asset Builder模式
2. 创建CSG图元（Box/Sphere/Cylinder/Cone）
3. 使用Gizmo调整位置
4. 选中两个物体 → 执行布尔运算（Union/Subtract/Intersect）
5. 调整参数（暴露宽度、高度等）
6. 封装为资产并上传服务器
7. L1用户在资产库中使用
```

**特点**:
- ✅ 写实风格
- ✅ 参数化建模
- ✅ 组件生态系统
- ✅ 专业工具链完整

---

## 🖥️ UI布局设计

### 方案A：独立CSG工具面板（推荐）

```
┌─────────────────────────────────────────────────────┐
│  Moon Editor                                  [L2]  │
├─────────────────────────────────────────────────────┤
│  [场景] [资产库] [🛠️ CSG建模] [测试]               │
├──────────┬────────────────────────────┬─────────────┤
│ 场景树   │      3D视图 (ImGuizmo)     │  属性面板  │
│          │                            │            │
│ □ Scene  │                            │ 📦 Box_1   │
│  ├ Box_1 │        📦 ← Gizmo          │ Width: 2.0 │
│  └ Sphe- │                            │ Height:2.0 │
│    re_1  │        ⚪                   │ Depth: 2.0 │
│          │                            │            │
│          │                            │ [删除节点] │
├──────────┴────────────────────────────┴─────────────┤
│  🛠️ CSG建模工具                                      │
│  ┌─────────────────────────────────────────────┐   │
│  │ 1️⃣ 创建基础图元                              │   │
│  │    [📦 Box]  [⚪ Sphere]  [⚙️ Cyl]  [🔺 Cone] │   │
│  │                                              │   │
│  │ 2️⃣ 布尔运算（已选中 2 个物体）                │   │
│  │    [➕ Union]    [➖ Subtract]    [✂️ Inter]  │   │
│  │                                              │   │
│  │ 3️⃣ 封装为资产                                 │   │
│  │    名称: [门框]                              │   │
│  │    暴露参数: [宽度] [高度]                   │   │
│  │    [💾 保存为资产]                           │   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

### 方案B：右键菜单（类似Roblox）

```
3D视图中选中两个物体 → 右键菜单：
┌─────────────────────┐
│ 复制               │
│ 粘贴               │
│ 删除               │
├─────────────────────┤
│ CSG 布尔运算 ▶     │──┬─ ➕ Union
│                    │  ├─ ➖ Subtract
│                    │  └─ ✂️ Intersect
└─────────────────────┘
```

**推荐使用方案A + 方案B结合**：
- 方案A：专业工作流，功能完整
- 方案B：快捷操作，提高效率

---

## 💻 技术实现

### 前端（React/TypeScript）

#### 1. CSG工具栏组件

已创建：[`editor/webui/src/components/CSGToolbar.tsx`](../editor/webui/src/components/CSGToolbar.tsx)

功能：
- ✅ 创建基础图元按钮
- ✅ 布尔运算按钮（根据选中状态激活/禁用）
- ✅ 封装资产入口
- ✅ 实时显示选中物体数量

#### 2. 集成到主界面

```tsx
// editor/webui/src/components/EditorLayout.tsx

import { CSGToolbar } from './CSGToolbar';

export const EditorLayout: React.FC = () => {
  const userLevel = useEditorStore(state => state.userLevel);
  
  return (
    <div className="editor-layout">
      <div className="left-panel">
        <SceneHierarchy />
      </div>
      
      <div className="center-viewport">
        {/* 3D视图 */}
      </div>
      
      <div className="right-panel">
        <PropertiesPanel />
      </div>
      
      {/* 🎯 L2用户可见CSG工具栏 */}
      {userLevel >= 2 && (
        <div className="bottom-panel">
          <CSGToolbar />
        </div>
      )}
    </div>
  );
};
```

### 后端（C++）

#### 1. 添加CSG命令处理器

在 `MoonEngineMessageHandler.cpp` 中添加：

```cpp
// ========== CSG基础图元创建 ==========
std::string HandleCreateCSGPrimitive(
    MoonEngineMessageHandler* handler, 
    const json& req, 
    Moon::Scene* scene) 
{
    std::string type = req["type"];  // "box", "sphere", "cylinder", "cone"
    
    MOON_LOG_INFO("CSG", "Creating CSG primitive: %s", type.c_str());
    
    // 创建节点
    std::string nodeName = "CSG_" + type;
    Moon::SceneNode* newNode = scene->CreateNode(nodeName);
    
    // 创建CSG组件
    Moon::CSGComponent* csgComp = newNode->AddComponent<Moon::CSGComponent>();
    
    // 根据类型创建CSG节点
    if (type == "box") {
        auto csgNode = Moon::CSGNode::CreateBox(1.0f, 1.0f, 1.0f);
        csgComp->SetRoot(csgNode);
    }
    else if (type == "sphere") {
        auto csgNode = Moon::CSGNode::CreateSphere(0.5f, 32);
        csgComp->SetRoot(csgNode);
    }
    else if (type == "cylinder") {
        auto csgNode = Moon::CSGNode::CreateCylinder(0.5f, 1.0f, 32);
        csgComp->SetRoot(csgNode);
    }
    else if (type == "cone") {
        auto csgNode = Moon::CSGNode::CreateCone(0.5f, 1.0f, 32);
        csgComp->SetRoot(csgNode);
    }
    else {
        return CreateErrorResponse("Unknown CSG primitive type");
    }
    
    // 生成Mesh并添加MeshRenderer
    auto mesh = csgComp->GetMesh();
    if (mesh) {
        Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
        renderer->SetMesh(mesh);
        
        // 添加默认材质
        Moon::Material* material = newNode->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.5f);
        material->SetBaseColor(Moon::Vector3(0.8f, 0.8f, 0.8f));
    }
    
    // 返回节点信息
    return CreateSuccessResponse();
}

// ========== CSG布尔运算 ==========
std::string HandlePerformCSGBoolean(
    MoonEngineMessageHandler* handler, 
    const json& req, 
    Moon::Scene* scene) 
{
    int nodeIdA = req["nodeIdA"];
    int nodeIdB = req["nodeIdB"];
    std::string operation = req["operation"];  // "union", "subtract", "intersect"
    
    MOON_LOG_INFO("CSG", "Performing %s on nodes %d and %d", 
                 operation.c_str(), nodeIdA, nodeIdB);
    
    // 获取两个节点
    Moon::SceneNode* nodeA = scene->FindNodeById(nodeIdA);
    Moon::SceneNode* nodeB = scene->FindNodeById(nodeIdB);
    
    if (!nodeA || !nodeB) {
        return CreateErrorResponse("Node not found");
    }
    
    // 获取CSG组件
    Moon::CSGComponent* csgCompA = nodeA->GetComponent<Moon::CSGComponent>();
    Moon::CSGComponent* csgCompB = nodeB->GetComponent<Moon::CSGComponent>();
    
    if (!csgCompA || !csgCompB) {
        return CreateErrorResponse("Nodes must have CSG components");
    }
    
    // 创建布尔运算节点
    auto csgNodeA = csgCompA->GetRoot();
    auto csgNodeB = csgCompB->GetRoot();
    
    std::shared_ptr<Moon::CSGNode> resultNode;
    
    if (operation == "union") {
        resultNode = Moon::CSGNode::CreateUnion(csgNodeA, csgNodeB);
    }
    else if (operation == "subtract") {
        resultNode = Moon::CSGNode::CreateSubtract(csgNodeA, csgNodeB);
    }
    else if (operation == "intersect") {
        resultNode = Moon::CSGNode::CreateIntersect(csgNodeA, csgNodeB);
    }
    else {
        return CreateErrorResponse("Unknown CSG operation");
    }
    
    // 创建新节点存储结果
    Moon::SceneNode* resultSceneNode = scene->CreateNode("CSG_Result");
    Moon::CSGComponent* resultComp = resultSceneNode->AddComponent<Moon::CSGComponent>();
    resultComp->SetRoot(resultNode);
    
    // 生成Mesh
    auto resultMesh = resultComp->GetMesh();
    if (resultMesh) {
        Moon::MeshRenderer* renderer = resultSceneNode->AddComponent<Moon::MeshRenderer>();
        renderer->SetMesh(resultMesh);
        
        // 复制材质
        Moon::Material* materialA = nodeA->GetComponent<Moon::Material>();
        if (materialA) {
            Moon::Material* resultMaterial = resultSceneNode->AddComponent<Moon::Material>();
            *resultMaterial = *materialA;
        }
    }
    
    // 删除原始节点
    scene->DestroyNode(nodeA);
    scene->DestroyNode(nodeB);
    
    MOON_LOG_INFO("CSG", "Boolean operation completed successfully");
    
    return CreateSuccessResponse();
}

// 注册命令
static std::unordered_map<std::string, CommandHandler> g_CommandHandlers = {
    // ... 现有命令 ...
    {"createCSGPrimitive",       CommandHandlers::HandleCreateCSGPrimitive},
    {"performCSGBoolean",        CommandHandlers::HandlePerformCSGBoolean},
};
```

#### 2. 修改 `engine-bridge.ts` 实现

```typescript
// editor/webui/src/utils/engine-bridge.ts

createPrimitive: wrapEngineCall('createPrimitive', async (type: string) => {
  return new Promise<void>((resolve, reject) => {
    const message = {
      command: 'createCSGPrimitive',
      type: type,  // 'box', 'sphere', 'cylinder', 'cone'
    };
    
    cefQuery({
      request: JSON.stringify(message),
      onSuccess: (response: string) => {
        console.log('[Engine] CSG primitive created:', type);
        resolve();
      },
      onFailure: (errorCode: number, errorMessage: string) => {
        console.error('[Engine] Failed to create CSG primitive:', errorMessage);
        reject(new Error(errorMessage));
      }
    });
  });
}),

performCSGOperation: wrapEngineCall('performCSGOperation', 
  async (nodeIdA: number, nodeIdB: number, operation: 'union' | 'subtract' | 'intersect') => {
    return new Promise<SceneNode>((resolve, reject) => {
      const message = {
        command: 'performCSGBoolean',
        nodeIdA: nodeIdA,
        nodeIdB: nodeIdB,
        operation: operation,
      };
      
      cefQuery({
        request: JSON.stringify(message),
        onSuccess: (response: string) => {
          console.log('[Engine] CSG operation completed:', operation);
          const result = JSON.parse(response);
          resolve(result.node);
        },
        onFailure: (errorCode: number, errorMessage: string) => {
          console.error('[Engine] CSG operation failed:', errorMessage);
          reject(new Error(errorMessage));
        }
      });
    });
  }
),
```

---

## 🎨 用户体验优化

### 1. 视觉反馈

- **选中高亮**：选中的CSG物体显示蓝色边框
- **预览模式**：执行布尔运算前显示半透明预览
- **错误提示**：操作失败时显示友好的错误信息

### 2. 快捷键

```
Ctrl + B     → 打开CSG建模面板
Ctrl + 1-4   → 快速创建Box/Sphere/Cylinder/Cone
Ctrl + U     → Union（选中2个物体后）
Ctrl + D     → Subtract
Ctrl + I     → Intersect
```

### 3. 操作提示

```
┌─────────────────────────────────────┐
│ 💡 提示：CSG建模                    │
│                                     │
│ 1. 创建基础图元                     │
│ 2. 使用Gizmo调整位置                │
│ 3. 选中两个物体执行布尔运算          │
│ 4. 完成后可封装为资产供他人使用      │
│                                     │
│ [不再显示] [关闭]                   │
└─────────────────────────────────────┘
```

---

## 📝 实现步骤

### Phase 1：基础功能（1-2天）
- [x] 前端UI组件（CSGToolbar.tsx）
- [ ] C++命令处理器（HandleCreateCSGPrimitive）
- [ ] C++布尔运算处理器（HandlePerformCSGBoolean）
- [ ] 集成到EditorLayout

### Phase 2：交互优化（1天）
- [ ] 选中状态管理
- [ ] 布尔运算预览
- [ ] 错误提示优化
- [ ] 快捷键支持

### Phase 3：资产封装（2-3天）
- [ ] 参数暴露系统
- [ ] 资产保存对话框
- [ ] 上传到服务器
- [ ] 资产库显示

### Phase 4：测试与优化（1-2天）
- [ ] 完整工作流测试
- [ ] 性能优化
- [ ] 文档完善
- [ ] 用户反馈收集

---

## 🚀 未来扩展

### 1. 高级CSG功能
- 多物体批量布尔运算
- CSG历史记录与编辑
- 参数化约束系统

### 2. 预制CSG模板
- 门/窗模板库
- 楼梯/栏杆模板
- 建筑结构模板

### 3. AI辅助建模
- 自然语言描述生成CSG
- 智能参数推荐
- 风格迁移

---

## 📚 参考资料

- [MVP_DEFINITION.md](MVP_DEFINITION.md) - 产品定义与用户分层
- [CSGComponent.h](../engine/core/CSG/CSGComponent.h) - CSG系统实现
- [CSGTest.cpp](../engine/samples/CSGTest.cpp) - CSG功能测试
- [Roblox CSG Documentation](https://create.roblox.com/docs/parts/solid-modeling)
