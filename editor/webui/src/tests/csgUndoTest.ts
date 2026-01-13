/**
 * CSG Undo/Redo 测试
 * 
 * 测试步骤：
 * 1. 创建 CSG Box
 * 2. 创建 CSG Sphere
 * 3. 创建 CSG Cylinder
 * 4. 修改 CSG Box 的 Position
 * 5. Undo Position 修改
 * 6. Redo Position 修改
 * 7. 删除 CSG Sphere
 * 8. Undo 删除
 * 9. Redo 删除
 * 10. Undo 所有操作到初始状态
 * 11. Redo 所有操作
 * 12. 验证最终状态
 */

import { engine } from '../utils/engine-bridge';
import { useEditorStore } from '../store/editorStore';
import { CreatePrimitiveCommand } from '../undo/PrimitiveCommands';
import { SetPositionCommand } from '../undo/TransformCommands';
import { DeleteNodeCommand } from '../undo/NodeCommands';
import { getUndoManager } from '../undo/UndoManager';
import {
  assertEquals,
  assertVector3Equals,
  assertNodeExists,
  assertNodeNotExists,
  sleep,
  printScene,
  printUndoStacks,
} from './testUtils';

export async function runCSGUndoTest() {
  console.log('\n\n🚀🚀🚀 开始 CSG Undo/Redo 测试 🚀🚀🚀\n');
  
  const undoManager = getUndoManager();
  undoManager.clear();
  
  const results: string[] = [];
  
  try {
    // 记录初始场景状态
    const initialScene = await engine.getScene();
    const initialNodeCount = Object.keys(initialScene.allNodes).length;
    console.log(`\n📊 初始场景节点数: ${initialNodeCount}`);
    printScene('初始场景', initialScene);
    
    // Step 1: 创建 CSG Box
    console.log('\n--- Step 1: 创建 CSG Box ---');
    const createBox = new CreatePrimitiveCommand('csg_box');
    await undoManager.execute(createBox);
    await sleep(100);
    
    const boxId = createBox.getCreatedNodeId();
    if (!boxId) throw new Error('CSG Box 创建失败 (未获取到 ID)');
    console.log(`✅ 创建 CSG Box，ID=${boxId}`);
    printScene('创建 CSG Box 后', useEditorStore.getState().scene);
    
    // Step 2: 创建 CSG Sphere
    console.log('\n--- Step 2: 创建 CSG Sphere ---');
    const createSphere = new CreatePrimitiveCommand('csg_sphere');
    await undoManager.execute(createSphere);
    await sleep(100);
    
    const sphereId = createSphere.getCreatedNodeId();
    if (!sphereId) throw new Error('CSG Sphere 创建失败 (未获取到 ID)');
    console.log(`✅ 创建 CSG Sphere，ID=${sphereId}`);
    printScene('创建 CSG Sphere 后', useEditorStore.getState().scene);
    
    // Step 3: 创建 CSG Cylinder
    console.log('\n--- Step 3: 创建 CSG Cylinder ---');
    const createCylinder = new CreatePrimitiveCommand('csg_cylinder');
    await undoManager.execute(createCylinder);
    await sleep(100);
    
    const cylinderId = createCylinder.getCreatedNodeId();
    if (!cylinderId) throw new Error('CSG Cylinder 创建失败 (未获取到 ID)');
    console.log(`✅ 创建 CSG Cylinder，ID=${cylinderId}`);
    printScene('创建 CSG Cylinder 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '创建 3 个 CSG 对象后的命令栈');
    
    // 验证节点存在
    let scene = await engine.getScene();
    assertNodeExists(boxId, scene, 'CSG Box 应该存在');
    assertNodeExists(sphereId, scene, 'CSG Sphere 应该存在');
    assertNodeExists(cylinderId, scene, 'CSG Cylinder 应该存在');
    console.log('✅ 所有 CSG 节点创建成功');
    
    // Step 4: 修改 CSG Box 的 Position
    console.log('\n--- Step 4: 修改 CSG Box 的 Position ---');
    const boxNode = scene.allNodes[boxId];
    if (!boxNode) throw new Error('无法获取 CSG Box 节点');
    
    const oldPos = { ...boxNode.transform.position };
    const newPos = { x: 5.0, y: 3.0, z: -2.0 };
    const setPos = new SetPositionCommand(boxId, oldPos, newPos);
    await undoManager.execute(setPos);
    await sleep(100);
    console.log(`✅ CSG Box Position: (${oldPos.x}, ${oldPos.y}, ${oldPos.z}) → (${newPos.x}, ${newPos.y}, ${newPos.z})`);
    printScene('修改 CSG Box Position 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '修改 Position 后的命令栈');
    
    // 验证 Position 修改
    scene = await engine.getScene();
    const boxAfterMove = scene.allNodes[boxId];
    assertVector3Equals(boxAfterMove.transform.position, newPos, 'CSG Box Position 应该是修改后的值');
    console.log('✅ Position 修改验证通过');
    
    // Step 5: Undo Position 修改
    console.log('\n--- Step 5: Undo Position 修改 ---');
    console.log(`Undo: ${undoManager.getUndoDescription()}`);
    await undoManager.undo();
    await sleep(100);
    printScene('Undo Position 后', useEditorStore.getState().scene);
    
    scene = await engine.getScene();
    const boxAfterUndo = scene.allNodes[boxId];
    assertVector3Equals(boxAfterUndo.transform.position, oldPos, 'CSG Box Position 应该恢复到原值');
    console.log('✅ Undo Position 验证通过');
    
    // Step 6: Redo Position 修改
    console.log('\n--- Step 6: Redo Position 修改 ---');
    console.log(`Redo: ${undoManager.getRedoDescription()}`);
    await undoManager.redo();
    await sleep(100);
    printScene('Redo Position 后', useEditorStore.getState().scene);
    
    scene = await engine.getScene();
    const boxAfterRedo = scene.allNodes[boxId];
    assertVector3Equals(boxAfterRedo.transform.position, newPos, 'CSG Box Position 应该再次变为修改后的值');
    console.log('✅ Redo Position 验证通过');
    
    // Step 7: 删除 CSG Sphere
    console.log('\n--- Step 7: 删除 CSG Sphere ---');
    const deleteSphere = await DeleteNodeCommand.create(sphereId);
    await undoManager.execute(deleteSphere);
    await sleep(100);
    console.log(`✅ 删除 CSG Sphere (ID=${sphereId})`);
    printScene('删除 CSG Sphere 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '删除 Sphere 后的命令栈');
    
    // 验证删除
    scene = await engine.getScene();
    assertNodeNotExists(sphereId, scene, 'CSG Sphere 应该被删除');
    assertNodeExists(boxId, scene, 'CSG Box 应该仍然存在');
    assertNodeExists(cylinderId, scene, 'CSG Cylinder 应该仍然存在');
    console.log('✅ 删除验证通过');
    
    // Step 8: Undo 删除
    console.log('\n--- Step 8: Undo 删除 ---');
    console.log(`Undo: ${undoManager.getUndoDescription()}`);
    await undoManager.undo();
    await sleep(100);
    printScene('Undo 删除后', useEditorStore.getState().scene);
    
    scene = await engine.getScene();
    assertNodeExists(sphereId, scene, 'CSG Sphere 应该被恢复');
    assertNodeExists(boxId, scene, 'CSG Box 应该仍然存在');
    assertNodeExists(cylinderId, scene, 'CSG Cylinder 应该仍然存在');
    console.log('✅ Undo 删除验证通过');
    
    // Step 9: Redo 删除
    console.log('\n--- Step 9: Redo 删除 ---');
    console.log(`Redo: ${undoManager.getRedoDescription()}`);
    await undoManager.redo();
    await sleep(100);
    printScene('Redo 删除后', useEditorStore.getState().scene);
    
    scene = await engine.getScene();
    assertNodeNotExists(sphereId, scene, 'CSG Sphere 应该再次被删除');
    assertNodeExists(boxId, scene, 'CSG Box 应该仍然存在');
    assertNodeExists(cylinderId, scene, 'CSG Cylinder 应该仍然存在');
    console.log('✅ Redo 删除验证通过');
    
    // Step 10: Undo 所有操作到初始状态
    console.log('\n--- Step 10: Undo 所有操作到初始状态 ---');
    let undoCount = 0;
    while (undoManager.canUndo()) {
      const desc = undoManager.getUndoDescription();
      console.log(`\nUndo #${++undoCount}: ${desc}`);
      await undoManager.undo();
      await sleep(50);
      printScene(`Undo #${undoCount} 后`, useEditorStore.getState().scene);
    }
    printUndoStacks(undoManager, 'Undo 到初始状态后的命令栈');
    
    scene = await engine.getScene();
    const finalNodeCount = Object.keys(scene.allNodes).length;
    assertEquals(finalNodeCount, initialNodeCount, '场景应该恢复到初始节点数');
    assertNodeNotExists(boxId, scene, 'CSG Box 应该不存在');
    assertNodeNotExists(sphereId, scene, 'CSG Sphere 应该不存在');
    assertNodeNotExists(cylinderId, scene, 'CSG Cylinder 应该不存在');
    console.log('✅ Undo 到初始状态验证通过');
    
    // Step 11: Redo 所有操作
    console.log('\n--- Step 11: Redo 所有操作 ---');
    let redoCount = 0;
    while (undoManager.canRedo()) {
      const desc = undoManager.getRedoDescription();
      console.log(`\nRedo #${++redoCount}: ${desc}`);
      await undoManager.redo();
      await sleep(50);
      printScene(`Redo #${redoCount} 后`, useEditorStore.getState().scene);
    }
    printUndoStacks(undoManager, 'Redo 所有操作后的命令栈');
    
    // Step 12: 验证最终状态
    console.log('\n--- Step 12: 验证最终状态 ---');
    scene = await engine.getScene();
    
    // CSG Sphere 应该是删除状态（因为最后一个操作是 Redo Delete）
    assertNodeNotExists(sphereId, scene, 'CSG Sphere 应该被删除（最终状态）');
    assertNodeExists(boxId, scene, 'CSG Box 应该存在');
    assertNodeExists(cylinderId, scene, 'CSG Cylinder 应该存在');
    
    // 验证 Box 的 Position
    const finalBox = scene.allNodes[boxId];
    assertVector3Equals(finalBox.transform.position, newPos, 'CSG Box Position 应该是修改后的值');
    
    console.log('\n\n🎉🎉🎉 所有断言通过！CSG Undo/Redo 测试成功！ 🎉🎉🎉');
    console.log('最终状态:');
    console.log(`  - CSG Box (${boxId}): Position=(${newPos.x}, ${newPos.y}, ${newPos.z}) ✅`);
    console.log(`  - CSG Sphere (${sphereId}): 已删除 ✅`);
    console.log(`  - CSG Cylinder (${cylinderId}): 存在 ✅`);
    
    printScene('最终场景状态', useEditorStore.getState().scene);
    
    // 等待所有异步操作完成
    console.log('\n⏳ 等待所有异步操作完成...');
    await sleep(500);
    console.log('✅ 清理完成');
    
    // 显示测试结果
    console.log('\n' + '='.repeat(60));
    console.log('🎉 CSG Undo 测试成功！');
    console.log('='.repeat(60));
    results.push('✅ 所有测试通过！');
    results.push('');
    results.push(`CSG Box (${boxId}): 存在 ✅`);
    results.push(`CSG Sphere (${sphereId}): 已删除 ✅`);
    results.push(`CSG Cylinder (${cylinderId}): 存在 ✅`);
    results.push('');
    results.push(`总命令数: ${undoCount + redoCount}`);
    results.push(`Undo 次数: ${undoCount}`);
    results.push(`Redo 次数: ${redoCount}`);
    console.log(results.join('\n'));
    console.log('='.repeat(60));
    
  } catch (error) {
    console.error('\n❌❌❌ CSG Undo 测试异常:', error);
    console.error('='.repeat(60));
    throw error;
  }
}
