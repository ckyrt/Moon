/**
 * CSG Mesh Undo/Redo 测试
 * 
 * 测试场景：CSG 物体在 Undo 删除后 Redo 恢复，Mesh 是否正确渲染
 * 
 * 问题背景：
 * - CSG 节点序列化时只保存参数，不保存 Mesh
 * - 反序列化时需要重新生成 CSG Mesh
 * - 如果没有重新生成，物体虽然存在但不可见
 * 
 * 测试步骤：
 * 1. 创建 CSG Box
 * 2. 验证 MeshRenderer 有 Mesh (hasMesh = true)
 * 3. Undo 删除物体
 * 4. Redo 恢复物体
 * 5. 验证 MeshRenderer 仍然有 Mesh（Mesh 已重新生成）
 */

import { engine } from '../utils/engine-bridge';
import { useEditorStore } from '../store/editorStore';
import { CreatePrimitiveCommand } from '../undo/PrimitiveCommands';
import { getUndoManager } from '../undo/UndoManager';
import {
  assertEquals,
  assertNodeExists,
  assertNodeNotExists,
  sleep,
  printScene,
} from './testUtils';

export async function runCSGMeshUndoTest() {
  console.log('\n\n🔧🔧🔧 开始 CSG Mesh Undo/Redo 测试 🔧🔧🔧\n');
  
  const undoManager = getUndoManager();
  undoManager.clear();
  
  try {
    // 记录初始场景状态
    const initialScene = await engine.getScene();
    const initialNodeCount = Object.keys(initialScene.allNodes).length;
    console.log(`\n📊 初始场景节点数: ${initialNodeCount}`);
    
    // Step 1: 创建 CSG Box
    console.log('\n--- Step 1: 创建 CSG Box ---');
    const createBox = new CreatePrimitiveCommand('csg_box');
    await undoManager.execute(createBox);
    await sleep(100);
    
    const boxId = createBox.getCreatedNodeId();
    if (!boxId) throw new Error('CSG Box 创建失败 (未获取到 ID)');
    console.log(`✅ 创建 CSG Box，ID=${boxId}`);
    
    // Step 2: 验证初始 Mesh 存在
    console.log('\n--- Step 2: 验证初始 Mesh 存在 ---');
    let scene = await engine.getScene();
    let boxNode = scene.allNodes[boxId];
    const meshRenderer = boxNode.components.find(c => c.type === 'MeshRenderer');
    
    if (!meshRenderer || !('hasMesh' in meshRenderer)) {
      throw new Error('MeshRenderer 组件未找到');
    }
    
    if (!meshRenderer.hasMesh) {
      throw new Error('❌ 初始状态：MeshRenderer 没有 Mesh！');
    }
    console.log('✅ 初始状态：MeshRenderer 有 Mesh (hasMesh = true)');
    
    // Step 3: Undo 删除物体
    console.log('\n--- Step 3: Undo 删除物体 ---');
    await undoManager.undo();
    await sleep(50);
    
    scene = await engine.getScene();
    assertNodeNotExists(boxId, scene, 'CSG Box 应该被删除');
    console.log('✅ 物体已删除');
    
    // Step 4: Redo 恢复物体
    console.log('\n--- Step 4: Redo 恢复物体 ---');
    await undoManager.redo();
    await sleep(50);
    
    scene = await engine.getScene();
    assertNodeExists(boxId, scene, 'CSG Box 应该存在');
    console.log('✅ 物体已恢复');
    
    // Step 5: 🎯 关键验证 - Mesh 是否重新生成
    console.log('\n--- Step 5: 验证 Mesh 是否重新生成 ---');
    boxNode = scene.allNodes[boxId];
    const restoredMeshRenderer = boxNode.components.find(c => c.type === 'MeshRenderer');
    
    if (!restoredMeshRenderer || !('hasMesh' in restoredMeshRenderer)) {
      throw new Error('恢复后：MeshRenderer 组件未找到');
    }
    
    console.log('恢复后 MeshRenderer 状态:', {
      hasMesh: restoredMeshRenderer.hasMesh,
      visible: restoredMeshRenderer.visible,
      enabled: restoredMeshRenderer.enabled
    });
    
    // 🎯 核心断言：Mesh 必须存在
    if (!restoredMeshRenderer.hasMesh) {
      throw new Error(
        `❌ Mesh 未重新生成！\n` +
        `  恢复后 hasMesh = false\n` +
        `  物体虽然存在但不可见\n` +
        `  原因：反序列化时没有重新生成 CSG Mesh`
      );
    }
    
    console.log('✅ Mesh 已重新生成 (hasMesh = true)');
    console.log('✅ 物体应该可见并正常渲染');
    
    console.log('\n\n🎉🎉🎉 CSG Mesh Undo/Redo 测试成功！ 🎉🎉🎉');
    console.log('验证要点:');
    console.log('  ✅ 物体恢复后存在');
    console.log('  ✅ MeshRenderer 组件存在');
    console.log('  ✅ Mesh 已重新生成 (hasMesh = true)');
    console.log('  ✅ 物体应该在场景中可见');
    
    printScene('最终场景状态', scene);
    
    console.log('\n' + '='.repeat(60));
    console.log('🔧 CSG Mesh 恢复测试通过！');
    console.log('='.repeat(60));
    
  } catch (error) {
    console.error('\n❌❌❌ CSG Mesh Undo 测试失败:', error);
    console.error('='.repeat(60));
    throw error;
  }
}
