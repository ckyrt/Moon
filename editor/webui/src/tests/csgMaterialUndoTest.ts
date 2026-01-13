/**
 * CSG Material Undo/Redo 测试
 * 
 * 测试场景：修改 CSG 物体的 Material 后，Undo 删除物体，再 Redo 恢复，Material 是否正确保留
 * 
 * 测试步骤：
 * 1. 创建 CSG Box
 * 2. 修改 Material (BaseColor = 红色, Metallic = 0.8, Roughness = 0.2)
 * 3. Undo 删除物体（应该在删除前保存最新快照）
 * 4. Redo 恢复物体
 * 5. 验证 Material 是否恢复到修改后的值
 */

import { engine } from '../utils/engine-bridge';
import { useEditorStore } from '../store/editorStore';
import { CreatePrimitiveCommand } from '../undo/PrimitiveCommands';
import { 
  SetMaterialPresetCommand,
  SetMaterialBaseColorCommand,
  SetMaterialMetallicCommand,
  SetMaterialRoughnessCommand
} from '../undo/MaterialCommands';
import { getUndoManager } from '../undo/UndoManager';
import type { MaterialPreset } from '../types/engine';
import {
  assertEquals,
  assertNodeExists,
  assertNodeNotExists,
  sleep,
  printScene,
  printUndoStacks,
} from './testUtils';

export async function runCSGMaterialUndoTest() {
  console.log('\n\n🎨🎨🎨 开始 CSG Material Undo/Redo 测试 🎨🎨🎨\n');
  
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
    
    // 获取初始 Material
    let scene = await engine.getScene();
    let boxNode = scene.allNodes[boxId];
    const materialComp = boxNode.components.find(c => c.type === 'Material');
    if (!materialComp || !('baseColor' in materialComp)) {
      throw new Error('Material 组件未找到');
    }
    
    const initialPreset = (materialComp.preset || 'None') as MaterialPreset;
    const initialColor = [...materialComp.baseColor];
    const initialMetallic = materialComp.metallic;
    const initialRoughness = materialComp.roughness;
    console.log('初始 Material:', {
      preset: initialPreset,
      baseColor: initialColor,
      metallic: initialMetallic,
      roughness: initialRoughness
    });
    
    // Step 2: 修改 Material Preset - 设置为 Metal
    console.log('\n--- Step 2: 修改 Material Preset (设置为 Metal) ---');
    const newPreset: MaterialPreset = 'Metal';
    const setPreset = new SetMaterialPresetCommand(
      boxId,
      initialPreset,
      newPreset
    );
    await undoManager.execute(setPreset);
    await sleep(100);
    console.log(`✅ Preset: ${initialPreset} → ${newPreset}`);
    
    // Step 3: 修改 Material - BaseColor 改为红色
    console.log('\n--- Step 3: 修改 Material (BaseColor = 红色) ---');
    const newColor = [1.0, 0.0, 0.0]; // 红色
    const setColor = new SetMaterialBaseColorCommand(
      boxId,
      initialColor,
      newColor
    );
    await undoManager.execute(setColor);
    await sleep(100);
    console.log(`✅ BaseColor: [${initialColor}] → [${newColor}]`);
    
    // Step 4: 修改 Material - Metallic
    console.log('\n--- Step 4: 修改 Material (Metallic = 0.8) ---');
    const newMetallic = 0.8;
    const setMetallic = new SetMaterialMetallicCommand(
      boxId,
      initialMetallic,
      newMetallic
    );
    await undoManager.execute(setMetallic);
    await sleep(100);
    console.log(`✅ Metallic: ${initialMetallic} → ${newMetallic}`);
    
    // Step 5: 修改 Material - Roughness
    console.log('\n--- Step 5: 修改 Material (Roughness = 0.2) ---');
    const newRoughness = 0.2;
    const setRoughness = new SetMaterialRoughnessCommand(
      boxId,
      initialRoughness,
      newRoughness
    );
    await undoManager.execute(setRoughness);
    await sleep(100);
    console.log(`✅ Roughness: ${initialRoughness} → ${newRoughness}`);
    
    printScene('修改 Material 后', useEditorStore.getState().scene);
    printUndoStacks(undoManager, '修改 Material 后的命令栈');
    
    // 验证 Material 修改成功
    scene = await engine.getScene();
    boxNode = scene.allNodes[boxId];
    const modifiedMaterial = boxNode.components.find(c => c.type === 'Material');
    if (!modifiedMaterial || !('baseColor' in modifiedMaterial)) {
      throw new Error('Material 组件未找到');
    }
    
    console.log('修改后的 Material:', {
      preset: modifiedMaterial.preset,
      baseColor: modifiedMaterial.baseColor,
      metallic: modifiedMaterial.metallic,
      roughness: modifiedMaterial.roughness
    });
    
    // Step 6: Undo 所有操作直到物体被删除
    console.log('\n--- Step 6: Undo 所有操作到初始状态 (物体删除) ---');
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
    assertNodeNotExists(boxId, scene, 'CSG Box 应该被删除');
    console.log('✅ Undo 到初始状态验证通过');
    
    // Step 7: Redo 所有操作 (恢复物体和 Material 修改)
    console.log('\n--- Step 7: Redo 所有操作 (恢复物体和 Material) ---');
    let redoCount = 0;
    while (undoManager.canRedo()) {
      const desc = undoManager.getRedoDescription();
      console.log(`\nRedo #${++redoCount}: ${desc}`);
      await undoManager.redo();
      await sleep(50);
      printScene(`Redo #${redoCount} 后`, useEditorStore.getState().scene);
    }
    printUndoStacks(undoManager, 'Redo 所有操作后的命令栈');
    
    // Step 8: 验证物体和 Material 是否正确恢复
    console.log('\n--- Step 8: 验证最终状态 ---');
    scene = await engine.getScene();
    
    assertNodeExists(boxId, scene, 'CSG Box 应该存在');
    
    const finalBoxNode = scene.allNodes[boxId];
    const finalMaterial = finalBoxNode.components.find(c => c.type === 'Material');
    if (!finalMaterial || !('baseColor' in finalMaterial)) {
      throw new Error('Material 组件未找到');
    }
    
    console.log('最终 Material:', {
      preset: finalMaterial.preset,
      baseColor: finalMaterial.baseColor,
      metallic: finalMaterial.metallic,
      roughness: finalMaterial.roughness
    });
    
    // 🎯 最重要：验证 Preset (Metal)
    if (finalMaterial.preset !== newPreset) {
      throw new Error(
        `❌ Preset 不匹配！\n` +
        `  期望: ${newPreset}\n` +
        `  实际: ${finalMaterial.preset}`
      );
    }
    console.log(`✅ Preset 验证通过: ${finalMaterial.preset}`);
    
    // 验证 BaseColor (允许小误差)
    const colorMatch = finalMaterial.baseColor.every((v, i) => 
      Math.abs(v - newColor[i]) < 0.001
    );
    if (!colorMatch) {
      throw new Error(
        `❌ BaseColor 不匹配！\n` +
        `  期望: [${newColor}]\n` +
        `  实际: [${finalMaterial.baseColor}]`
      );
    }
    console.log('✅ BaseColor 验证通过');
    
    // 验证 Metallic
    if (Math.abs(finalMaterial.metallic - newMetallic) > 0.001) {
      throw new Error(
        `❌ Metallic 不匹配！\n` +
        `  期望: ${newMetallic}\n` +
        `  实际: ${finalMaterial.metallic}`
      );
    }
    console.log('✅ Metallic 验证通过');
    
    // 验证 Roughness
    if (Math.abs(finalMaterial.roughness - newRoughness) > 0.001) {
      throw new Error(
        `❌ Roughness 不匹配！\n` +
        `  期望: ${newRoughness}\n` +
        `  实际: ${finalMaterial.roughness}`
      );
    }
    console.log('✅ Roughness 验证通过');
    
    console.log('\n\n🎉🎉🎉 所有断言通过！CSG Material Undo/Redo 测试成功！ 🎉🎉🎉');
    console.log('最终状态:');
    console.log(`  - CSG Box (${boxId}): 存在 ✅`);
    console.log(`  - Preset: ${finalMaterial.preset} (Metal) ✅`);
    console.log(`  - BaseColor: [${finalMaterial.baseColor}] (红色) ✅`);
    console.log(`  - Metallic: ${finalMaterial.metallic} ✅`);
    console.log(`  - Roughness: ${finalMaterial.roughness} ✅`);
    
    printScene('最终场景状态', useEditorStore.getState().scene);
    
    // 等待所有异步操作完成
    console.log('\n⏳ 等待所有异步操作完成...');
    await sleep(500);
    console.log('✅ 清理完成');
    
    // 显示测试结果
    console.log('\n' + '='.repeat(60));
    console.log('🎨 CSG Material Undo 测试成功！');
    console.log('='.repeat(60));
    results.push('✅ 所有测试通过！');
    results.push('');
    results.push(`CSG Box (${boxId}): 存在 ✅`);
    results.push(`Preset: ${finalMaterial.preset} (Metal) ✅`);
    results.push(`BaseColor: 红色 [${finalMaterial.baseColor}] ✅`);
    results.push(`Metallic: ${finalMaterial.metallic} ✅`);
    results.push(`Roughness: ${finalMaterial.roughness} ✅`);
    results.push('');
    results.push(`总命令数: ${undoCount + redoCount}`);
    results.push(`Undo 次数: ${undoCount}`);
    results.push(`Redo 次数: ${redoCount}`);
    console.log(results.join('\n'));
    console.log('='.repeat(60));
    
  } catch (error) {
    console.error('\n❌❌❌ CSG Material Undo 测试异常:', error);
    console.error('='.repeat(60));
    throw error;
  }
}
