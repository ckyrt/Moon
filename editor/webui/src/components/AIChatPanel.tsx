import React, { useEffect, useMemo, useRef, useState } from 'react';
import { engine } from '@/utils/engine-bridge';
import { logger } from '@/utils/logger';
import { useEditorStore } from '@/store/editorStore';
import styles from './AIChatPanel.module.css';

type ChatRole = 'user' | 'assistant';

interface ChatMessage {
  id: string;
  role: ChatRole;
  text: string;
}

const EMPTY_SCENE = {
  schema: 'moon_scene_design',
  version: 1,
  building_instances: [],
  object_instances: []
};

const EXAMPLE_PROMPTS = [
  '给我创建一个小房子',
  '添加一张木质餐桌',
  '把这个建筑改成 30 层塔楼',
  '把桌子往东边挪动两米'
];

const OBJECT_KEYWORDS = ['桌', '椅', '沙发', '床', '灯', '柜', 'sofa', 'table', 'chair', 'lamp', 'bed', 'furniture', 'object'];

const BUILDING_KEYWORDS = ['建筑', '房子', '楼', '塔', 'house', 'building', 'tower', 'villa', 'apartment', '东方明珠'];

const createMessage = (role: ChatRole, text: string): ChatMessage => ({
  id: `${role}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
  role,
  text
});

const createInitialSceneJson = (): string => JSON.stringify(EMPTY_SCENE, null, 2);

const countInstances = (sceneJson: string): { buildings: number; objects: number } => {
  try {
    const parsed = JSON.parse(sceneJson) as { building_instances?: unknown[]; object_instances?: unknown[] };
    return {
      buildings: parsed.building_instances?.length ?? 0,
      objects: parsed.object_instances?.length ?? 0
    };
  } catch {
    return { buildings: 0, objects: 0 };
  }
};

const hasAnySceneContent = (sceneJson: string): boolean => {
  const counts = countInstances(sceneJson);
  return counts.buildings > 0 || counts.objects > 0;
};

const looksLikeObjectPrompt = (prompt: string): boolean => {
  const lower = prompt.toLowerCase();
  return OBJECT_KEYWORDS.some((keyword) => lower.includes(keyword.toLowerCase()));
};

const looksLikeBuildingPrompt = (prompt: string): boolean => {
  const lower = prompt.toLowerCase();
  return BUILDING_KEYWORDS.some((keyword) => lower.includes(keyword.toLowerCase()));
};

const makeBuildingScene = (buildingJson: string): string =>
  JSON.stringify(
    {
      ...EMPTY_SCENE,
      building_instances: [
        {
          instance_id: 'building_main',
          name: 'Main Building',
          building_json: buildingJson,
          transform: {
            position: [0, 0, 0],
            rotation_euler: [0, 0, 0],
            scale: [1, 1, 1]
          }
        }
      ]
    },
    null,
    2
  );

const makeObjectScene = (objectJson: string): string =>
  JSON.stringify(
    {
      ...EMPTY_SCENE,
      object_instances: [
        {
          instance_id: 'object_main',
          name: 'Generated Object',
          object_json: objectJson,
          parameter_overrides: {},
          transform: {
            position: [0, 0, 0],
            rotation_euler: [0, 0, 0],
            scale: [1, 1, 1]
          }
        }
      ]
    },
    null,
    2
  );

export const AIChatPanel: React.FC = () => {
  const updateScene = useEditorStore((state) => state.updateScene);
  const [prompt, setPrompt] = useState('');
  const [messages, setMessages] = useState<ChatMessage[]>([
    createMessage('assistant', '描述你想要的建筑或物体，我会直接生成并更新当前场景。')
  ]);
  const [workingSceneJson, setWorkingSceneJson] = useState(createInitialSceneJson);
  const [status, setStatus] = useState('');
  const [error, setError] = useState('');
  const [isBusy, setIsBusy] = useState(false);
  const messagesRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    messagesRef.current?.scrollTo({
      top: messagesRef.current.scrollHeight,
      behavior: 'smooth'
    });
  }, [messages]);

  const sceneCounts = useMemo(() => countInstances(workingSceneJson), [workingSceneJson]);

  const refreshEditorScene = async () => {
    const scene = await engine.getScene();
    updateScene(scene);
  };

  const applyAndPreviewScene = async (sceneJson: string, successText: string) => {
    const preview = await engine.previewScene(sceneJson, { focusCamera: true });
    setWorkingSceneJson(preview.sceneJson);
    await refreshEditorScene();
    const warningText = preview.warnings.length > 0 ? `，${preview.warnings.length} 个 warning` : '';
    setStatus(`${successText} 已更新 ${preview.buildingInstanceCount} 个建筑实例和 ${preview.objectInstanceCount} 个物体实例${warningText}`);
    setMessages((current) => [
      ...current,
      createMessage(
        'assistant',
        `${successText}\n建筑实例 ${preview.buildingInstanceCount} 个，物体实例 ${preview.objectInstanceCount} 个。`
      )
    ]);
  };

  const handleInitialGeneration = async (input: string) => {
    if (looksLikeObjectPrompt(input) && !looksLikeBuildingPrompt(input)) {
      const objectResult = await engine.generateObjectFromPrompt(input);
      await applyAndPreviewScene(makeObjectScene(objectResult.objectJson), '已生成新的物体场景');
      return;
    }

    try {
      const buildingResult = await engine.generateBuildingFromPrompt(input);
      await applyAndPreviewScene(makeBuildingScene(buildingResult.buildingJson), '已生成新的建筑场景');
    } catch (buildingError) {
      logger.warn('AIChatPanel', `Initial building generation failed, trying object flow: ${String(buildingError)}`);
      const objectResult = await engine.generateObjectFromPrompt(input);
      await applyAndPreviewScene(makeObjectScene(objectResult.objectJson), '已生成新的物体场景');
    }
  };

  const handleSceneEdit = async (input: string) => {
    const opsResult = await engine.generateSceneOperationsFromPrompt(input, workingSceneJson);
    const nextScene = await engine.applySceneOperations(workingSceneJson, opsResult.opsJson);
    await applyAndPreviewScene(nextScene.sceneJson, '场景编辑已应用');
  };

  const submitPrompt = async (overridePrompt?: string) => {
    const input = (overridePrompt ?? prompt).trim();
    if (!input || isBusy) {
      return;
    }

    setPrompt('');
    setError('');
    setStatus('正在理解你的意图并更新场景...');
    setMessages((current) => [...current, createMessage('user', input)]);
    setIsBusy(true);

    try {
      if (hasAnySceneContent(workingSceneJson)) {
        await handleSceneEdit(input);
      } else {
        await handleInitialGeneration(input);
      }
    } catch (submitError) {
      const message = submitError instanceof Error ? submitError.message : 'AI scene update failed';
      logger.error('AIChatPanel', `Scene chat failed: ${message}`);
      setError(message);
      setStatus('');
      setMessages((current) => [
        ...current,
        createMessage('assistant', `这次更新没有成功：${message}`)
      ]);
    } finally {
      setIsBusy(false);
    }
  };

  return (
    <div className={styles.panel}>
      <div className={styles.hero}>
        <div className={styles.eyebrow}>AI Scene Authoring</div>
        <div className={styles.titleRow}>
          <div>
            <h3 className={styles.title}>一句话生成并编辑场景</h3>
            <p className={styles.subtitle}>
              新建筑和新物体会由 AI 直接生成 JSON，已有场景则自动转成结构化操作并重新渲染。
            </p>
          </div>
          <div className={styles.sceneBadge}>
            <strong>{sceneCounts.buildings + sceneCounts.objects}</strong>
            实例
          </div>
        </div>

        <div className={styles.examples}>
          {EXAMPLE_PROMPTS.map((example) => (
            <button
              key={example}
              type="button"
              className={styles.exampleChip}
              disabled={isBusy}
              onClick={() => void submitPrompt(example)}
            >
              {example}
            </button>
          ))}
        </div>

        <div className={styles.chatShell}>
          <div ref={messagesRef} className={styles.messages}>
            {messages.map((message) => (
              <div
                key={message.id}
                className={`${styles.message} ${message.role === 'user' ? styles.messageUser : styles.messageAssistant}`}
              >
                <div className={styles.messageMeta}>{message.role === 'user' ? 'You' : 'Moon AI'}</div>
                <div
                  className={`${styles.bubble} ${
                    message.role === 'user' ? styles.bubbleUser : styles.bubbleAssistant
                  }`}
                >
                  {message.text}
                </div>
              </div>
            ))}
          </div>

          <div className={styles.composer}>
            <textarea
              className={styles.textarea}
              value={prompt}
              onChange={(event) => setPrompt(event.target.value)}
              placeholder="例如：给我创建一个小房子，然后添加桌子，再把建筑改成 30 层。"
              onKeyDown={(event) => {
                if (event.key === 'Enter' && !event.shiftKey) {
                  event.preventDefault();
                  void submitPrompt();
                }
              }}
            />
            <div className={styles.composerFooter}>
              <div>
                <div className={error ? styles.error : styles.status}>{error || status}</div>
                <div className={styles.hint}>Enter 发送，Shift+Enter 换行。场景会自动刷新，不需要额外预览按钮。</div>
              </div>
              <div className={styles.actions}>
                <button type="button" className={styles.sendButton} disabled={isBusy || !prompt.trim()} onClick={() => void submitPrompt()}>
                  {isBusy ? 'Working...' : 'Send'}
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
