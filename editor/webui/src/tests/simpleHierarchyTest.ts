/**
 * 简单层级 Undo/Redo 测试
 * 
 * 测试步骤：
 * 1. 创建 A (cube - 橙色立方体)、B (sphere - 蓝色球体)、C (cylinder - 绿色圆柱体)
 * 2. 修改 A 的 Position
 * 3. 修改 B 的 Position
 * 4. Undo/Redo Position 修改
 * 5. 设置 B 为 A 的 child
 * 6. 设置 C 为 B 的 child
 * 7. 删除 B（B 和 C 都被删除）
 * 8. Undo
 * 9. Redo
 * 10. 一直 Undo 到 A、B、C 都没有
 * 11. 一直 Redo 回来
 * 12. 验证最终层级和 Transform
 */

import { engine } from '../utils/engine-bridge';
import { useEditorStore } from '../store/editorStore';
import { CreateNodeCommand, SetParentCommand, DeleteNodeCommand } from '../undo/NodeCommands';
import { SetPositionCommand } from '../undo/TransformCommands';
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

export async function runSimpleHierarchyTest() {
  console.log('\n\n🚀🚀🚀 开始简单层级 Undo/Redo 测试 🚀🚀🚀\n');
  
  const undoManager = getUndoManager();
  undoManager.clear();
  
  try {
  // 记录初始场景状态
  const initialScene = await engine.getScene();
  const initialNodeCount = Object.keys(initialScene.allNodes).length;
  console.log(`\n📊 初始场景节点数: ${initialNodeCount}`);
  printScene('初始场景', initialScene);
    
    // Step 1: 创建 A (Cube - 橙色方块)
    console.log('\n--- Step 1: 创建节点 A (Cube) ---');
    const createA = new CreateNodeCommand('cube', undefined);
    await undoManager.execute(createA);
    await sleep(100);
    
  const nodeAId = createA.getCreatedNodeId();
  if (!nodeAId) throw new Error('节点 A 创建失败 (未获取到 ID)');
  console.log(`✅ 创建 A (Cube)，ID=${nodeAId}`);
  printScene('创建 A 后', useEditorStore.getState().scene);
    
    // Step 2: 创建 B (Sphere - 蓝色球体)
    console.log('\n--- Step 2: 创建节点 B (Sphere) ---');
    const createB = new CreateNodeCommand('sphere', undefined);
    await undoManager.execute(createB);
    await sleep(100);
    
    const nodeBId = createB.getCreatedNodeId();
    if (!nodeBId) throw new Error('节点 B 创建失败 (未获取到 ID)');
    console.log(`✅ 创建 B (Sphere)，ID=${nodeBId}`);
    printScene('创建 B 后', useEditorStore.getState().scene);
    
    // Step 3: 创建 C (Cylinder - 绿色圆柱体)
    console.log('\n--- Step 3: 创建节点 C (Cylinder) ---');
    const createC = new CreateNodeCommand('cylinder', undefined);
    await undoManager.execute(createC);
    await sleep(100);
    
    const nodeCId = createC.getCreatedNodeId();
    if (!nodeCId) throw new Error('节点 C 创建失败 (未获取到 ID)');
    console.log(`✅ 创建 C (Cylinder)，ID=${nodeCId}`);
    printScene('创建 C 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '创建 A B C 后的命令栈');
    
    // 获取初始节点数据
    const sceneAfterCreate = await engine.getScene();
    const nodeA = sceneAfterCreate.allNodes[nodeAId];
    const nodeB = sceneAfterCreate.allNodes[nodeBId];
    const nodeC = sceneAfterCreate.allNodes[nodeCId];
    
    if (!nodeA || !nodeB || !nodeC) {
      throw new Error('无法从场景中获取创建的节点');
    }
    
    // Step 4: 修改 A 的 Position
    console.log('\n--- Step 4: 修改 A 的 Position ---');
    const oldPosA = { ...nodeA.transform.position };
    const newPosA = { x: 10.0, y: 5.0, z: 3.0 };
    const setPosA = new SetPositionCommand(nodeAId, oldPosA, newPosA);
    await undoManager.execute(setPosA);
    await sleep(100);
    console.log(`✅ A 的 Position: (${oldPosA.x}, ${oldPosA.y}, ${oldPosA.z}) → (${newPosA.x}, ${newPosA.y}, ${newPosA.z})`);
    printScene('修改 A Position 后', useEditorStore.getState().scene);
    
    // Step 5: 修改 B 的 Position
    console.log('\n--- Step 5: 修改 B 的 Position ---');
    const oldPosB = { ...nodeB.transform.position };
    const newPosB = { x: -5.0, y: 2.0, z: 8.0 };
    const setPosB = new SetPositionCommand(nodeBId, oldPosB, newPosB);
    await undoManager.execute(setPosB);
    await sleep(100);
    console.log(`✅ B 的 Position: (${oldPosB.x}, ${oldPosB.y}, ${oldPosB.z}) → (${newPosB.x}, ${newPosB.y}, ${newPosB.z})`);
    printScene('修改 B Position 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '修改 Transform 后的命令栈');
    
    // Step 6: Undo B 的 Position
    console.log('\n--- Step 6: Undo B 的 Position 修改 ---');
    await undoManager.undo();
    await sleep(100);
    const afterUndoPosB = useEditorStore.getState().scene.allNodes[nodeBId].transform.position;
    console.log(`Undo B Position: (${afterUndoPosB.x}, ${afterUndoPosB.y}, ${afterUndoPosB.z})`);
    
    // 🎯 断言：Undo 后应该恢复到原始位置
    assertVector3Equals(afterUndoPosB, oldPosB, 'Undo B Position 应该恢复到 oldPosB');
    printScene('Undo B Position 后', useEditorStore.getState().scene);
    
    // Step 7: Redo B 的 Position
    console.log('\n--- Step 7: Redo B 的 Position 修改 ---');
    await undoManager.redo();
    await sleep(100);
    const afterRedoPosB = useEditorStore.getState().scene.allNodes[nodeBId].transform.position;
    console.log(`Redo B Position: (${afterRedoPosB.x}, ${afterRedoPosB.y}, ${afterRedoPosB.z})`);
    
    // 🎯 断言：Redo 后应该恢复到修改后的位置
    assertVector3Equals(afterRedoPosB, newPosB, 'Redo B Position 应该恢复到 newPosB');
    printScene('Redo B Position 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, 'Redo Transform 后的命令栈');
    
    // Step 8: 设置 B 为 A 的 child
    console.log('\n--- Step 8: 设置 B 为 A 的 child ---');
    
    // 🔍 在设置父子关系之前，打印 B 的位置
    const beforeSetParent = await engine.getScene();
    const bBeforeParent = beforeSetParent.allNodes[nodeBId];
    console.log(`⚠️ 设置父子关系前 B 的位置: (${bBeforeParent.transform.position.x}, ${bBeforeParent.transform.position.y}, ${bBeforeParent.transform.position.z})`);
    
    const setBParent = new SetParentCommand(nodeBId, null, nodeAId);
    await undoManager.execute(setBParent);
    await sleep(100);
    
    // 🔍 在设置父子关系之后，再次打印 B 的位置
    const afterSetParent = await engine.getScene();
    const bAfterParent = afterSetParent.allNodes[nodeBId];
    const aAfterParent = afterSetParent.allNodes[nodeAId];
    console.log(`⚠️ 设置父子关系后 B 的位置: (${bAfterParent.transform.position.x}, ${bAfterParent.transform.position.y}, ${bAfterParent.transform.position.z})`);
    console.log(`⚠️ A 的位置: (${aAfterParent.transform.position.x}, ${aAfterParent.transform.position.y}, ${aAfterParent.transform.position.z})`);
    
    // 🎯 计算 B 相对于 A 的本地坐标（用于后续验证）
    // LocalPos = WorldPos(B) - WorldPos(A)
    const expectedLocalPosB = {
      x: newPosB.x - newPosA.x,  // -5 - 10 = -15
      y: newPosB.y - newPosA.y,  //  2 - 5  = -3
      z: newPosB.z - newPosA.z,  //  8 - 3  =  5
    };
    console.log(`🧮 计算出的 B 本地坐标: (${expectedLocalPosB.x}, ${expectedLocalPosB.y}, ${expectedLocalPosB.z})`);
    
    console.log(`✅ B(${nodeBId}) → A(${nodeAId}) 的子节点`);
    printScene('B → A 后', useEditorStore.getState().scene);
    
    // Step 9: 设置 C 为 B 的 child
    console.log('\n--- Step 9: 设置 C 为 B 的 child ---');
    const setCParent = new SetParentCommand(nodeCId, null, nodeBId);
    await undoManager.execute(setCParent);
    await sleep(100);
    console.log(`✅ C(${nodeCId}) → B(${nodeBId}) 的子节点`);
    printScene('C → B 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '设置层级后的命令栈');
    
    // 验证层级
    const sceneBeforeDelete = await engine.getScene();
    const verifyB = sceneBeforeDelete.allNodes[nodeBId];
    const verifyC = sceneBeforeDelete.allNodes[nodeCId];
    console.log(`\n验证层级:`);
    
    // 🎯 断言：层级关系正确
    assertEquals(verifyB.parentId, nodeAId, 'B.parentId 应该等于 A.id');
    assertEquals(verifyC.parentId, nodeBId, 'C.parentId 应该等于 B.id');
    
    // Step 10: 删除 B
    console.log('\n--- Step 10: 删除 B ---');
    const deleteB = await DeleteNodeCommand.create(nodeBId);
    await undoManager.execute(deleteB);
    await sleep(100);
    console.log(`删除 B(${nodeBId})，C(${nodeCId}) 也应该被删除`);
    
    // 🎯 断言：B 和 C 都应该被删除
    let sceneAfterDelete = await engine.getScene();
    assertNodeNotExists(nodeBId, sceneAfterDelete, 'B 应该被删除');
    assertNodeNotExists(nodeCId, sceneAfterDelete, 'C 应该被删除（作为 B 的子节点）');
    assertNodeExists(nodeAId, sceneAfterDelete, 'A 应该仍然存在');
    
    printScene('删除 B 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '删除 B 后的命令栈');
    
    // Step 11: Undo
    console.log('\n--- Step 11: Undo 删除 ---');
    await undoManager.undo();
    await sleep(100);
    console.log(`Undo 删除`);
    
    // 🎯 断言：B 和 C 应该恢复，且层级关系正确
    let sceneAfterUndoDelete = await engine.getScene();
    assertNodeExists(nodeBId, sceneAfterUndoDelete, 'Undo 后 B 应该恢复');
    assertNodeExists(nodeCId, sceneAfterUndoDelete, 'Undo 后 C 应该恢复');
    assertEquals(sceneAfterUndoDelete.allNodes[nodeBId].parentId, nodeAId, 'Undo 后 B.parentId 应该是 A.id');
    assertEquals(sceneAfterUndoDelete.allNodes[nodeCId].parentId, nodeBId, 'Undo 后 C.parentId 应该是 B.id');
    
    printScene('Undo 删除后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, 'Undo 后的命令栈');
    
    // Step 12: Redo
    console.log('\n--- Step 12: Redo 删除 ---');
    await undoManager.redo();
    await sleep(100);
    console.log(`Redo 删除`);
    
    // 🎯 断言：Redo 后 B 和 C 应该再次被删除
    let sceneAfterRedoDelete = await engine.getScene();
    assertNodeNotExists(nodeBId, sceneAfterRedoDelete, 'Redo 后 B 应该被删除');
    assertNodeNotExists(nodeCId, sceneAfterRedoDelete, 'Redo 后 C 应该被删除');
    assertNodeExists(nodeAId, sceneAfterRedoDelete, 'Redo 后 A 应该仍然存在');
    
    printScene('Redo 删除后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, 'Redo 后的命令栈');
    
    // Step 13: 一直 Undo 到 A B C 都没有
    console.log('\n\n🔥🔥🔥 --- Step 13: 一直 Undo 到场景为空 --- 🔥🔥🔥');
    let undoCount = 0;
    while (undoManager.canUndo()) {
      const desc = undoManager.getUndoDescription();
      console.log(`\nUndo #${++undoCount}: ${desc}`);
      await undoManager.undo();
      await sleep(50);
      printScene(`Undo #${undoCount} 后`, useEditorStore.getState().scene);
    }
    printUndoStacks(undoManager, 'Undo 到空场景后的命令栈');
    
    const emptyScene = await engine.getScene();
    const finalNodeCount = Object.keys(emptyScene.allNodes).length;
    console.log(`\n场景节点数: ${finalNodeCount}`);
    
    // 🎯 断言：场景应该恢复到初始状态
    assertEquals(finalNodeCount, initialNodeCount, '场景应该恢复到初始节点数');
    
    // Step 14: 一直 Redo 回来（但不包括 Delete）
    console.log('\n\n🔥🔥🔥 --- Step 14: Redo 恢复层级和 Transform（不包括删除操作）--- 🔥🔥🔥');
    let redoCount = 0;
    // Redo: 3个Create + 2个SetPosition + 2个SetParent = 7次
    const targetRedoCount = 7;
    while (undoManager.canRedo() && redoCount < targetRedoCount) {
      const desc = undoManager.getRedoDescription();
      console.log(`\nRedo #${++redoCount}: ${desc}`);
      await undoManager.redo();
      await sleep(50);
      printScene(`Redo #${redoCount} 后`, useEditorStore.getState().scene);
    }
    printUndoStacks(undoManager, `Redo ${redoCount} 次后的命令栈`);
    
    // Step 15: 最终验证（层级 + Transform）
    console.log('\n\n🎯🎯🎯 --- Step 15: 最终验证（层级 + Transform）--- 🎯🎯🎯');
    const finalScene = await engine.getScene();
    
    console.log(`\n📌 验证节点 ID: A=${nodeAId}, B=${nodeBId}, C=${nodeCId}`);
    
    const finalA = finalScene.allNodes[nodeAId];
    const finalB = finalScene.allNodes[nodeBId];
    const finalC = finalScene.allNodes[nodeCId];
    
    console.log(`\n节点存在性检查:`);
    console.log(`finalA exists: ${!!finalA}, id=${finalA?.id}`);
    console.log(`finalB exists: ${!!finalB}, id=${finalB?.id}`);
    console.log(`finalC exists: ${!!finalC}, id=${finalC?.id}`);
    
    // 🎯 断言：所有节点都应该存在
    assertNodeExists(nodeAId, finalScene, 'A 应该存在');
    assertNodeExists(nodeBId, finalScene, 'B 应该存在');
    assertNodeExists(nodeCId, finalScene, 'C 应该存在');
    
    if (finalA && finalB && finalC) {
      console.log(`\n层级关系检查:`);
      
      // 🎯 断言：层级关系正确
      assertEquals(finalA.parentId, null, 'A.parentId 应该为 null');
      assertEquals(finalB.parentId, nodeAId, 'B.parentId 应该等于 A.id');
      assertEquals(finalC.parentId, nodeBId, 'C.parentId 应该等于 B.id');
      
      console.log(`\nTransform 检查:`);
      const finalPosA = finalA.transform.position;
      const finalPosB = finalB.transform.position;
      
      // 🎯 断言：A 是根节点，其 Position 是世界坐标
      assertVector3Equals(finalPosA, newPosA, 'A 的 Position 应该是修改后的值（世界坐标）');
      
      // 🎯 断言：B 是 A 的子节点，其 Position 是相对于 A 的本地坐标
      // 本地坐标 = 世界坐标(B) - 世界坐标(A) = (-5, 2, 8) - (10, 5, 3) = (-15, -3, 5)
      assertVector3Equals(finalPosB, expectedLocalPosB, 'B 的 Position 应该是相对于 A 的本地坐标');
      
      console.log(`\n📝 说明：`);
      console.log(`  - A 是根节点，Position 是世界坐标: (${newPosA.x}, ${newPosA.y}, ${newPosA.z})`);
      console.log(`  - B 是 A 的子节点，Position 是本地坐标: (${expectedLocalPosB.x}, ${expectedLocalPosB.y}, ${expectedLocalPosB.z})`);
      console.log(`  - B 的世界坐标 = A世界坐标 + B本地坐标 = (${newPosA.x + expectedLocalPosB.x}, ${newPosA.y + expectedLocalPosB.y}, ${newPosA.z + expectedLocalPosB.z})`);
      
      console.log('\n\n🎉🎉🎉 所有断言通过！测试成功！ 🎉🎉🎉');
      console.log('层级结构:');
      console.log(`  A(${nodeAId}) [Cube 橙色] 世界pos:(${newPosA.x}, ${newPosA.y}, ${newPosA.z})`);
      console.log(`    └─ B(${nodeBId}) [Sphere 蓝色] 本地pos:(${expectedLocalPosB.x}, ${expectedLocalPosB.y}, ${expectedLocalPosB.z}) 世界pos:(-5, 2, 8)`);
      console.log(`         └─ C(${nodeCId}) [Cylinder 绿色]`);
    }
    
    printScene('最终场景状态', useEditorStore.getState().scene);
    
    // 🔧 等待所有异步操作完成（避免 CEF 关闭时的 shutdown checker 错误）
    console.log('\n⏳ 等待所有异步操作完成...');
    await sleep(500);
    console.log('✅ 清理完成');
    
  } catch (error) {
    console.error('\n❌❌❌ 测试异常:', error);
    throw error;
  }
}
