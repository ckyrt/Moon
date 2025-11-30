/**
 * 节点属性 Undo/Redo 测试
 * 
 * 测试 RenameNodeCommand 和 SetNodeActiveCommand
 * 
 * 测试步骤：
 * 1. 创建一个 Cube 节点
 * 2. 测试重命名 (Rename)
 *    - 执行重命名
 *    - Undo 重命名
 *    - Redo 重命名
 * 3. 测试激活状态 (SetActive)
 *    - 设置为非激活
 *    - Undo 激活状态
 *    - Redo 激活状态
 * 4. 混合测试：多次重命名和激活状态切换
 * 5. 一直 Undo 到初始状态
 * 6. 一直 Redo 恢复
 */

import { engine } from '../utils/engine-bridge';
import { useEditorStore } from '../store/editorStore';
import { CreateNodeCommand, RenameNodeCommand, SetNodeActiveCommand } from '../undo/NodeCommands';
import { getUndoManager } from '../undo/UndoManager';
import {
  assertEquals,
  assertNodeExists,
  assertTrue,
  assertFalse,
  sleep,
  printScene,
  printUndoStacks,
} from './testUtils';

export async function runNodePropertiesTest() {
  console.log('\n\n🚀🚀🚀 开始节点属性 Undo/Redo 测试 🚀🚀🚀\n');
  
  const undoManager = getUndoManager();
  undoManager.clear();
  
  try {
    // 记录初始场景状态
    const initialScene = await engine.getScene();
    const initialNodeCount = Object.keys(initialScene.allNodes).length;
    console.log(`\n📊 初始场景节点数: ${initialNodeCount}`);
    printScene('初始场景', initialScene);
    
    // Step 1: 创建测试节点
    console.log('\n--- Step 1: 创建 Cube 节点 ---');
    const createNode = new CreateNodeCommand('cube', undefined);
    await undoManager.execute(createNode);
    await sleep(100);
    
    const nodeId = createNode.getCreatedNodeId();
    if (!nodeId) throw new Error('节点创建失败 (未获取到 ID)');
    console.log(`✅ 创建节点，ID=${nodeId}`);
    
    // 获取初始节点数据
    let scene = await engine.getScene();
    const node = scene.allNodes[nodeId];
    const originalName = node.name;
    const originalActive = node.active;
    console.log(`📝 节点初始状态: name="${originalName}", active=${originalActive}`);
    printScene('创建节点后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '创建节点后的命令栈');
    
    // ========== 测试重命名 ==========
    console.log('\n\n🔷🔷🔷 测试重命名 (RenameNodeCommand) 🔷🔷🔷');
    
    // Step 2: 重命名节点
    console.log('\n--- Step 2: 重命名节点 ---');
    const newName1 = "MyCube";
    const rename1 = new RenameNodeCommand(nodeId, originalName, newName1);
    await undoManager.execute(rename1);
    await sleep(100);
    
    scene = await engine.getScene();
    assertEquals(scene.allNodes[nodeId].name, newName1, `节点名称应该是 "${newName1}"`);
    console.log(`✅ 重命名成功: "${originalName}" → "${newName1}"`);
    printScene('重命名后', useEditorStore.getState().scene);
    
    // Step 3: Undo 重命名
    console.log('\n--- Step 3: Undo 重命名 ---');
    await undoManager.undo();
    await sleep(100);
    
    scene = await engine.getScene();
    assertEquals(scene.allNodes[nodeId].name, originalName, `Undo 后名称应该恢复为 "${originalName}"`);
    console.log(`✅ Undo 成功: "${newName1}" → "${originalName}"`);
    printScene('Undo 重命名后', useEditorStore.getState().scene);
    
    // Step 4: Redo 重命名
    console.log('\n--- Step 4: Redo 重命名 ---');
    await undoManager.redo();
    await sleep(100);
    
    scene = await engine.getScene();
    assertEquals(scene.allNodes[nodeId].name, newName1, `Redo 后名称应该是 "${newName1}"`);
    console.log(`✅ Redo 成功: "${originalName}" → "${newName1}"`);
    printScene('Redo 重命名后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '重命名测试后的命令栈');
    
    // ========== 测试激活状态 ==========
    console.log('\n\n🔶🔶🔶 测试激活状态 (SetNodeActiveCommand) 🔶🔶🔶');
    
    // Step 5: 设置为非激活
    console.log('\n--- Step 5: 设置节点为非激活状态 ---');
    const setInactive = new SetNodeActiveCommand(nodeId, originalActive, false);
    await undoManager.execute(setInactive);
    await sleep(100);
    
    scene = await engine.getScene();
    assertFalse(scene.allNodes[nodeId].active, '节点应该是非激活状态');
    console.log(`✅ 设置非激活成功: ${originalActive} → false`);
    printScene('设置非激活后', useEditorStore.getState().scene);
    
    // Step 6: Undo 激活状态
    console.log('\n--- Step 6: Undo 激活状态 ---');
    await undoManager.undo();
    await sleep(100);
    
    scene = await engine.getScene();
    assertTrue(scene.allNodes[nodeId].active, 'Undo 后节点应该恢复激活状态');
    console.log(`✅ Undo 成功: false → ${originalActive}`);
    printScene('Undo 激活状态后', useEditorStore.getState().scene);
    
    // Step 7: Redo 激活状态
    console.log('\n--- Step 7: Redo 激活状态 ---');
    await undoManager.redo();
    await sleep(100);
    
    scene = await engine.getScene();
    assertFalse(scene.allNodes[nodeId].active, 'Redo 后节点应该是非激活状态');
    console.log(`✅ Redo 成功: ${originalActive} → false`);
    printScene('Redo 激活状态后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '激活状态测试后的命令栈');
    
    // ========== 混合测试 ==========
    console.log('\n\n🔷🔶🔷 混合测试：多次重命名和激活状态切换 🔷🔶🔷');
    
    // Step 8: 再次重命名
    console.log('\n--- Step 8: 第二次重命名 ---');
    const newName2 = "SuperCube";
    const rename2 = new RenameNodeCommand(nodeId, newName1, newName2);
    await undoManager.execute(rename2);
    await sleep(100);
    
    scene = await engine.getScene();
    assertEquals(scene.allNodes[nodeId].name, newName2, `节点名称应该是 "${newName2}"`);
    console.log(`✅ 第二次重命名成功: "${newName1}" → "${newName2}"`);
    
    // Step 9: 再次切换激活状态（激活）
    console.log('\n--- Step 9: 重新激活节点 ---');
    const setActive = new SetNodeActiveCommand(nodeId, false, true);
    await undoManager.execute(setActive);
    await sleep(100);
    
    scene = await engine.getScene();
    assertTrue(scene.allNodes[nodeId].active, '节点应该是激活状态');
    console.log(`✅ 重新激活成功: false → true`);
    
    // Step 10: 第三次重命名
    console.log('\n--- Step 10: 第三次重命名 ---');
    const newName3 = "AwesomeCube";
    const rename3 = new RenameNodeCommand(nodeId, newName2, newName3);
    await undoManager.execute(rename3);
    await sleep(100);
    
    scene = await engine.getScene();
    assertEquals(scene.allNodes[nodeId].name, newName3, `节点名称应该是 "${newName3}"`);
    console.log(`✅ 第三次重命名成功: "${newName2}" → "${newName3}"`);
    printScene('混合测试后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '混合测试后的命令栈');
    
    // ========== 完整 Undo/Redo 测试 ==========
    console.log('\n\n🔥🔥🔥 完整 Undo/Redo 测试 🔥🔥🔥');
    
    // Step 11: 一直 Undo 到初始状态
    console.log('\n--- Step 11: 一直 Undo 到初始状态 ---');
    let undoCount = 0;
    while (undoManager.canUndo()) {
      const desc = undoManager.getUndoDescription();
      console.log(`\nUndo #${++undoCount}: ${desc}`);
      await undoManager.undo();
      await sleep(50);
      
      scene = await engine.getScene();
      const currentNode = scene.allNodes[nodeId];
      if (currentNode) {
        console.log(`  → 当前状态: name="${currentNode.name}", active=${currentNode.active}`);
      } else {
        console.log(`  → 节点已不存在`);
      }
    }
    printUndoStacks(undoManager, 'Undo 到初始状态后的命令栈');
    
    // 验证场景恢复到初始状态
    const emptyScene = await engine.getScene();
    const finalNodeCount = Object.keys(emptyScene.allNodes).length;
    console.log(`\n📊 最终场景节点数: ${finalNodeCount}`);
    assertEquals(finalNodeCount, initialNodeCount, '场景应该恢复到初始节点数');
    console.log(`✅ 场景已恢复到初始状态`);
    
    // Step 12: 一直 Redo 恢复所有操作
    console.log('\n--- Step 12: 一直 Redo 恢复所有操作 ---');
    let redoCount = 0;
    while (undoManager.canRedo()) {
      const desc = undoManager.getRedoDescription();
      console.log(`\nRedo #${++redoCount}: ${desc}`);
      await undoManager.redo();
      await sleep(50);
      
      scene = await engine.getScene();
      const currentNode = scene.allNodes[nodeId];
      if (currentNode) {
        console.log(`  → 当前状态: name="${currentNode.name}", active=${currentNode.active}`);
      }
    }
    printUndoStacks(undoManager, 'Redo 所有操作后的命令栈');
    
    // 最终验证
    console.log('\n\n🎯🎯🎯 最终验证 🎯🎯🎯');
    const finalScene = await engine.getScene();
    const finalNode = finalScene.allNodes[nodeId];
    
    assertNodeExists(nodeId, finalScene, '节点应该存在');
    assertEquals(finalNode.name, newName3, `最终名称应该是 "${newName3}"`);
    assertTrue(finalNode.active, '最终应该是激活状态');
    
    console.log(`\n📝 最终节点状态:`);
    console.log(`  - ID: ${nodeId}`);
    console.log(`  - Name: "${finalNode.name}"`);
    console.log(`  - Active: ${finalNode.active}`);
    
    console.log('\n\n🎉🎉🎉 所有测试通过！ 🎉🎉🎉');
    console.log(`总计执行了 ${undoCount} 次 Undo 和 ${redoCount} 次 Redo`);
    
    printScene('最终场景状态', useEditorStore.getState().scene);
    
    // 🔧 等待所有异步操作完成
    console.log('\n⏳ 等待所有异步操作完成...');
    await sleep(500);
    console.log('✅ 测试完成');
    
  } catch (error) {
    console.error('\n❌❌❌ 测试异常:', error);
    throw error;
  }
}
