# 材质下载优先级推荐（别墅场景）

## 🎯 优先级1：核心材质（必下载）

这6种材质覆盖别墅80%的使用场景，建议**首批下载测试**：

| 材质名称 | 用途 | 搜索关键词 | 推荐资源 |
|---------|------|-----------|---------|
| **Brick** | 外墙砖、庭院砖墙 | `red brick wall` | [Poly Haven - Red Brick](https://polyhaven.com/textures?s=brick) |
| **Plaster** | 室内墙面、天花板 | `plaster wall white` | [Poly Haven - Plaster](https://polyhaven.com/textures?s=plaster) |
| **WoodFloor** | 室内地板 | `wood floor parquet` | [Poly Haven - Wood Floor](https://polyhaven.com/textures?s=wood+floor) |
| **TileCeramic** | 厨房、卫生间 | `ceramic tile white` | [Poly Haven - Tiles](https://polyhaven.com/textures?s=tiles) |
| **Steel** | 栏杆、扶手、五金 | `steel metal brushed` | [Poly Haven - Metal](https://polyhaven.com/textures?s=metal) |
| **ConcreteFloor** | 车库、地下室、露台 | `concrete floor polished` | [Poly Haven - Concrete](https://polyhaven.com/textures?s=concrete) |

---

## 🌟 优先级2：增强材质（推荐下载）

这5种材质让场景更丰富，建议**第二批下载**：

| 材质名称 | 用途 | 搜索关键词 |
|---------|------|-----------|
| **Stone** | 外墙装饰、柱子 | `stone wall travertine` |
| **WoodPolished** | 高级家具、门 | `wood polished mahogany` |
| **Aluminum** | 门窗框、幕墙 | `aluminum brushed anodized` |
| **Leather** | 沙发、座椅 | `leather brown worn` |
| **Carpet** | 地毯 | `carpet fabric grey` |

---

## 🔧 优先级3：补充材质（按需下载）

这7种材质用于特定场景或细节，可**根据项目需求选择性下载**：

| 材质名称 | 用途 | 使用频率 |
|---------|------|---------|
| **Chrome** | 水龙头、把手 | ⭐⭐ |
| **Copper** | 装饰件、管道 | ⭐⭐ |
| **ConcretePolished** | 现代风格地面 | ⭐⭐ |
| **WoodPainted** | 彩色家具、门框 | ⭐⭐ |
| **Rubber** | 密封条、轮胎 | ⭐⭐ |
| - | - | - |
| **ConcretePolished** | 已有 Concrete044D 可替代 | ⭐ |
| **Aluminum** | 已有 Metal061B 可临时替代 | ⭐ |

---

## 📥 下载操作步骤

### 方案A：快速测试（只下载2个材质）
如果只想快速验证系统，下载这2个即可：
1. **Brick** - 测试外墙渲染
2. **WoodFloor** - 测试室内地板

### 方案B：标准别墅（下载6个核心材质）
按优先级1全部下载，足以搭建完整别墅场景。

### 方案C：完整覆盖（下载全部18个）
如果想要最丰富的材质库，按优先级1→2→3顺序全部下载。

---

## 🎨 具体下载示例：Brick

1. **打开 Poly Haven**  
   https://polyhaven.com/textures?s=brick

2. **选择合适的砖材质**  
   推荐：`red_brick_02` 或 `brick_wall_001`

3. **下载设置**
   - Resolution: **2K** (2048x2048)
   - Format: **PNG**
   - Maps: All (Color, Normal, Roughness, AO, Displacement)

4. **解压到**  
   `Moon/assets/textures/materials/Brick_2K-PNG/`

5. **重命名文件**（如果需要）
   ```
   red_brick_02_diff_2k.png → Brick_2K-PNG_Color.png
   red_brick_02_nor_dx_2k.png → Brick_2K-PNG_NormalDX.png
   red_brick_02_rough_2k.png → Brick_2K-PNG_Roughness.png
   red_brick_02_ao_2k.png → Brick_2K-PNG_AmbientOcclusion.png
   ```

6. **运行测试**
   ```powershell
   # 修改 test_scene.json，把某个物体材质改为 "brick"
   # 重新运行引擎
   .\bin\x64\Debug\HelloEngine.exe
   ```

---

## ⚡ 我的推荐

**如果你现在想快速测试渲染效果**：  
下载 **Brick + WoodFloor** 这两个，修改 `simple_room_v1.json`：
- 墙壁改为 `"brick"`
- 地面改为 `"wood_floor"`

这样可以验证：
- ✅ 新材质系统工作正常
- ✅ Triplanar 映射效果
- ✅ PBR 参数是否合理

**如果你想一次性建完材质库**：  
按优先级1→2顺序下载 **11个材质** (6+5)，覆盖95%的别墅场景需求。

---

## 🤝 需要我帮你做什么？

1. **帮你下载搜索** - 我可以帮你在 Poly Haven 搜索具体的材质名称
2. **生成测试场景** - 创建一个展示所有新材质的测试房间
3. **提供下载脚本** - 写一个PowerShell脚本自动下载+重命名
4. **逐步指导** - 手把手教你下载第一个材质

你想要哪种方式？
