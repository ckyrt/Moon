import React, { useEffect, useMemo, useRef, useState } from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import type { AssetPreset } from '@/types/engine';
import styles from './AssetPreview.module.css';

type AssetKind = 'object' | 'building';

const EMPTY_LIST: AssetPreset[] = [];

export const AssetPreview: React.FC = () => {
  const updateScene = useEditorStore((state) => state.updateScene);
  const [assetKind, setAssetKind] = useState<AssetKind>('object');
  const [presets, setPresets] = useState<AssetPreset[]>(EMPTY_LIST);
  const [selectedFile, setSelectedFile] = useState('');
  const [selectedJson, setSelectedJson] = useState('');
  const [isLoadingList, setIsLoadingList] = useState(false);
  const [isPreviewing, setIsPreviewing] = useState(false);
  const [status, setStatus] = useState('');
  const [error, setError] = useState('');
  const requestIdRef = useRef(0);

  const buildingPresets = useMemo(
    () => presets.filter((preset) => preset.file.startsWith('building/')),
    [presets]
  );

  const activePresets = assetKind === 'object' ? presets : buildingPresets;

  useEffect(() => {
    let disposed = false;

    const loadPresets = async () => {
      setIsLoadingList(true);
      setError('');
      setStatus('');
      setSelectedFile('');
      setSelectedJson('');

      try {
        if (assetKind === 'object') {
          const objectPresets = await engine.listObjectPresets();
          if (!disposed) {
            setPresets(objectPresets);
          }
          return;
        }

        const presetList = await engine.listMassingPresets();
        if (!disposed) {
          setPresets(presetList);
        }
      } catch (loadError) {
        if (!disposed) {
          setError(loadError instanceof Error ? loadError.message : 'Failed to load asset list.');
        }
      } finally {
        if (!disposed) {
          setIsLoadingList(false);
        }
      }
    };

    loadPresets();

    return () => {
      disposed = true;
    };
  }, [assetKind]);

  const refreshScene = async () => {
    const scene = await engine.getScene();
    updateScene(scene);
  };

  const handlePreview = async (presetFile: string) => {
    const requestId = requestIdRef.current + 1;
    requestIdRef.current = requestId;

    setSelectedFile(presetFile);
    setIsPreviewing(true);
    setError('');
    setStatus('Loading JSON...');

    try {
      const jsonText = assetKind === 'object'
        ? await engine.loadObjectPreset(presetFile)
        : await engine.loadMassingPreset(presetFile);

      if (requestId !== requestIdRef.current) {
        return;
      }

      setSelectedJson(jsonText);
      setStatus('Previewing in scene...');

      if (assetKind === 'object') {
        await engine.previewObject(jsonText, { focusCamera: true });
      } else {
        await engine.previewBuilding(jsonText, { focusCamera: true });
      }

      await refreshScene();

      if (requestId !== requestIdRef.current) {
        return;
      }

      setStatus('Preview loaded.');
    } catch (previewError) {
      if (requestId !== requestIdRef.current) {
        return;
      }

      setError(previewError instanceof Error ? previewError.message : 'Failed to preview asset.');
      setStatus('');
    } finally {
      if (requestId === requestIdRef.current) {
        setIsPreviewing(false);
      }
    }
  };

  const handleClear = async () => {
    setError('');
    setStatus('Clearing preview...');
    try {
      await engine.clearMassingPreview();
      await refreshScene();
      setStatus('Preview cleared.');
    } catch (clearError) {
      setError(clearError instanceof Error ? clearError.message : 'Failed to clear preview.');
      setStatus('');
    }
  };

  return (
    <div className={styles.panel}>
      <div className={styles.header}>
        <span className={styles.title}>Asset Preview</span>
        <button className={styles.clearButton} onClick={handleClear} type="button">
          Clear
        </button>
      </div>

      <div className={styles.kindSwitch}>
        <button
          type="button"
          className={`${styles.kindButton} ${assetKind === 'object' ? styles.kindButtonActive : ''}`}
          onClick={() => setAssetKind('object')}
        >
          Objects
        </button>
        <button
          type="button"
          className={`${styles.kindButton} ${assetKind === 'building' ? styles.kindButtonActive : ''}`}
          onClick={() => setAssetKind('building')}
        >
          Buildings
        </button>
      </div>

      <div className={styles.controls}>
        <select
          className={styles.select}
          value={selectedFile}
          onChange={(event) => {
            const file = event.target.value;
            if (file) {
              void handlePreview(file);
            }
          }}
          disabled={isLoadingList || isPreviewing || activePresets.length === 0}
        >
          <option value="">
            {isLoadingList ? 'Loading assets...' : `Select a ${assetKind} JSON`}
          </option>
          {activePresets.map((preset) => (
            <option key={preset.id} value={preset.file}>
              {preset.name}
            </option>
          ))}
        </select>

        <button
          type="button"
          className={styles.previewButton}
          onClick={() => {
            if (selectedFile) {
              void handlePreview(selectedFile);
            }
          }}
          disabled={!selectedFile || isPreviewing}
        >
          {isPreviewing ? 'Loading...' : 'Reload'}
        </button>
      </div>

      {status ? <div className={styles.status}>{status}</div> : null}
      {error ? <div className={styles.error}>{error}</div> : null}

      <textarea
        className={styles.jsonView}
        value={selectedJson}
        readOnly
        placeholder={`Selected ${assetKind} JSON will appear here.`}
      />
    </div>
  );
};
