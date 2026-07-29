"use client";

/* eslint-disable @next/next/no-img-element -- renderer frames are pre-sized WebP assets and must swap without a Next image loader */

import {
  ChevronLeft,
  ChevronRight,
  GitCompareArrows,
  Pause,
  Play,
  RefreshCcw,
  ScanLine,
} from "lucide-react";
import {
  CartesianGrid,
  Line,
  LineChart,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";
import { useCallback, useEffect, useMemo, useState } from "react";

type SceneKey = "Sponza" | "SponzaIvy" | "Bistro";
type ValidationView = "comparison" | "difference";
type WorkloadKey =
  | "none"
  | "indexCount"
  | "triangleCount"
  | "totalFragments"
  | "averageOverdraw"
  | "quadEfficiency";

interface FrameRecord {
  imageIndex: number;
  renderFrame: number;
  measurementFrame: number;
  src: string;
}

interface CaptureSequence {
  id: string;
  label: string;
  scene: SceneKey;
  view: string;
  viewLabel: string;
  viewKind: "beauty" | "debug";
  renderer: string;
  rendererVariant: number;
  debugMode: number | null;
  description: string;
  profileId: string;
  source: {
    width: number;
    height: number;
    warmupFrames: number;
    measureFrames: number;
    captureStride: number;
    captureFps: number;
    sourceExperiment?: string;
  };
  frameCount: number;
  frames: FrameRecord[];
}

interface ValidationFrame {
  imageIndex: number;
  measurementFrame: number;
  comparisonSrc: string;
  differenceSrc: string;
  metrics: {
    interiorMaeLsb: number;
    interiorP99Lsb: number;
    interiorMaxLsb: number;
    interiorExactRatio: number;
    coverageMismatchRatio: number;
  };
}

interface ValidationSequence {
  id: string;
  scene: SceneKey;
  mode: number;
  modeName: string;
  label: string;
  shortLabel: string;
  order: number;
  description: string;
  profileId: string;
  source: {
    hardwareId: string;
    referenceRenderer: string;
    referenceRendererVariant: number;
    visbufRenderer: string;
    visbufRendererVariant: number;
    width: number;
    height: number;
    captureStride: number;
  };
  frameCount: number;
  frames: ValidationFrame[];
}

interface ProfileSample {
  frame: number;
  totalMs: number;
  indexCount: number;
  passes: Record<string, number | null>;
}

interface CameraProfile {
  id: string;
  hardwareId: string;
  scene: SceneKey;
  renderer: string;
  runIndex: number;
  samples: ProfileSample[];
}

interface RasterSample {
  scene: SceneKey;
  measurementFrame: number;
  triangleCount: number;
  totalFragments: number;
  coveredPixels: number;
  overdrawExtra: number;
  averageOverdraw: number;
  maximumOverdraw: number;
  rasterizedTriangles: number;
  skippedTriangles: number;
  quadInstances: number;
  quadCoveredLanes: number;
  quadWasteLanes: number;
  quadEfficiency: number;
}

interface AtlasData {
  schemaVersion: number;
  provenance: {
    campaignStatus: string;
    sourceBranch: string;
    baseCommit: string;
  };
  hardware: {
    gpu: {
      name: string;
      driver: string;
      memoryMiB: number;
    };
  };
  summary: {
    resultRows: number;
    validationPairs: number;
  };
  profiles: CameraProfile[];
  sequences: CaptureSequence[];
  validationSequences: ValidationSequence[];
  rasterTimeline: RasterSample[];
}

interface DebugChoice {
  id: string;
  scene: SceneKey;
  label: string;
  description: string;
  kind: "identity" | "validation";
  capture?: CaptureSequence;
  validation?: ValidationSequence;
}

interface SeriesSpec {
  key: string;
  renderer: "Deferred" | "VisBuf";
  label: string;
  shortLabel: string;
  pass: string | null;
  color: string;
  width: number;
  dash?: string;
}

const SCENE_LABELS: Record<SceneKey, string> = {
  Sponza: "Sponza",
  SponzaIvy: "Sponza + Ivy",
  Bistro: "Bistro",
};

const SERIES_SPECS: SeriesSpec[] = [
  {
    key: "d:total",
    renderer: "Deferred",
    label: "Deferred · total",
    shortLabel: "Total",
    pass: null,
    color: "#f0b75d",
    width: 2.6,
  },
  {
    key: "d:depth_prepass",
    renderer: "Deferred",
    label: "Deferred · depth prepass",
    shortLabel: "Depth prepass",
    pass: "depth_prepass",
    color: "#70b9f2",
    width: 1.8,
  },
  {
    key: "d:geometry",
    renderer: "Deferred",
    label: "Deferred · geometry",
    shortLabel: "Geometry",
    pass: "geometry",
    color: "#55c987",
    width: 1.8,
  },
  {
    key: "d:lighting",
    renderer: "Deferred",
    label: "Deferred · lighting",
    shortLabel: "Lighting",
    pass: "lighting",
    color: "#c9ced6",
    width: 1.5,
  },
  {
    key: "d:tonemap",
    renderer: "Deferred",
    label: "Deferred · tonemap",
    shortLabel: "Tonemap",
    pass: "tonemap",
    color: "#8c949f",
    width: 1.4,
  },
  {
    key: "v:total",
    renderer: "VisBuf",
    label: "VisBuf · total",
    shortLabel: "Total",
    pass: null,
    color: "#6ee1ff",
    width: 2.6,
  },
  {
    key: "v:visibility",
    renderer: "VisBuf",
    label: "VisBuf · visibility",
    shortLabel: "Visibility",
    pass: "visibility",
    color: "#438fd4",
    width: 1.8,
  },
  {
    key: "v:visutil_histogram",
    renderer: "VisBuf",
    label: "VisBuf · histogram",
    shortLabel: "Histogram",
    pass: "visutil_histogram",
    color: "#f09b45",
    width: 1.5,
  },
  {
    key: "v:visutil_prefix",
    renderer: "VisBuf",
    label: "VisBuf · prefix",
    shortLabel: "Prefix",
    pass: "visutil_prefix",
    color: "#ffc66a",
    width: 1.5,
  },
  {
    key: "v:visutil_flatten",
    renderer: "VisBuf",
    label: "VisBuf · flatten",
    shortLabel: "Flatten",
    pass: "visutil_flatten",
    color: "#d97835",
    width: 1.5,
  },
  {
    key: "v:gbuffer",
    renderer: "VisBuf",
    label: "VisBuf · compute G-buffer",
    shortLabel: "G-buffer",
    pass: "gbuffer",
    color: "#a885ef",
    width: 1.8,
  },
  {
    key: "v:lighting",
    renderer: "VisBuf",
    label: "VisBuf · lighting",
    shortLabel: "Lighting",
    pass: "lighting",
    color: "#aeb6c1",
    width: 1.5,
  },
  {
    key: "v:tonemap",
    renderer: "VisBuf",
    label: "VisBuf · tonemap",
    shortLabel: "Tonemap",
    pass: "tonemap",
    color: "#717a86",
    width: 1.4,
  },
];

const DEFAULT_SERIES = new Set([
  "d:total",
  "d:depth_prepass",
  "d:geometry",
  "v:total",
  "v:visibility",
  "v:gbuffer",
]);

const WORKLOAD_META: Record<
  Exclude<WorkloadKey, "none">,
  { label: string; shortLabel: string; color: string; formatter: (value: number) => string }
> = {
  indexCount: {
    label: "Visible index count",
    shortLabel: "indices",
    color: "#ef7da4",
    formatter: formatCompact,
  },
  triangleCount: {
    label: "Input triangles",
    shortLabel: "triangles",
    color: "#7dd9b1",
    formatter: formatCompact,
  },
  totalFragments: {
    label: "Generated fragments",
    shortLabel: "fragments",
    color: "#f29e66",
    formatter: formatCompact,
  },
  averageOverdraw: {
    label: "Average overdraw",
    shortLabel: "overdraw",
    color: "#d892ff",
    formatter: (value) => `${value.toFixed(2)}×`,
  },
  quadEfficiency: {
    label: "Quad-lane efficiency",
    shortLabel: "quad efficiency",
    color: "#84c8ff",
    formatter: (value) => `${(value * 100).toFixed(1)}%`,
  },
};

function formatMs(value: number | null | undefined, digits = 3) {
  return value === null || value === undefined || !Number.isFinite(value)
    ? "—"
    : `${value.toFixed(digits)} ms`;
}

function formatCompact(value: number | null | undefined) {
  if (value === null || value === undefined || !Number.isFinite(value)) return "—";
  return new Intl.NumberFormat("en", {
    notation: "compact",
    maximumFractionDigits: 2,
  }).format(value);
}

function nearestByFrame<T>(
  values: T[],
  frame: number,
  getFrame: (value: T) => number,
) {
  if (!values.length) return undefined;
  return values.reduce((nearest, value) =>
    Math.abs(getFrame(value) - frame) < Math.abs(getFrame(nearest) - frame)
      ? value
      : nearest,
  );
}

function choiceFrames(choice: DebugChoice | undefined) {
  if (!choice) return [];
  return choice.kind === "identity"
    ? (choice.capture?.frames ?? [])
    : (choice.validation?.frames ?? []);
}

export function AtlasApp() {
  const [data, setData] = useState<AtlasData | null>(null);
  const [error, setError] = useState("");
  const [scene, setScene] = useState<SceneKey>("Sponza");
  const [debugId, setDebugId] = useState("sponza-geometry-primitive");
  const [frameIndex, setFrameIndex] = useState(0);
  const [playing, setPlaying] = useState(true);
  const [loop, setLoop] = useState(true);
  const [playbackFps, setPlaybackFps] = useState(8);
  const [validationView, setValidationView] =
    useState<ValidationView>("comparison");
  const [visibleSeries, setVisibleSeries] = useState<Set<string>>(
    () => new Set(DEFAULT_SERIES),
  );
  const [workloadKey, setWorkloadKey] = useState<WorkloadKey>("indexCount");

  useEffect(() => {
    let live = true;
    fetch("/data/dashboard.json")
      .then((response) => {
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return response.json() as Promise<AtlasData>;
      })
      .then((payload) => {
        if (live) setData(payload);
      })
      .catch((reason: unknown) => {
        if (live) {
          setError(reason instanceof Error ? reason.message : "Unknown data error");
        }
      });
    return () => {
      live = false;
    };
  }, []);

  const debugChoices = useMemo<DebugChoice[]>(() => {
    if (!data) return [];
    const identity = data.sequences
      .filter((sequence) => sequence.scene === scene && sequence.viewKind === "debug")
      .map((sequence) => ({
        id: sequence.id,
        scene,
        label: sequence.viewLabel,
        description: sequence.description,
        kind: "identity" as const,
        capture: sequence,
      }));
    const validation = data.validationSequences
      .filter((sequence) => sequence.scene === scene)
      .sort((left, right) => left.order - right.order)
      .map((sequence) => ({
        id: sequence.id,
        scene,
        label: sequence.label,
        description: sequence.description,
        kind: "validation" as const,
        validation: sequence,
      }));
    return [...identity, ...validation];
  }, [data, scene]);

  const debugChoice = useMemo(
    () => debugChoices.find((choice) => choice.id === debugId) ?? debugChoices[0],
    [debugChoices, debugId],
  );
  const frames = useMemo(() => choiceFrames(debugChoice), [debugChoice]);
  const activeFrame = frames[Math.min(frameIndex, Math.max(0, frames.length - 1))];
  const measurementFrame = activeFrame?.measurementFrame ?? 0;

  useEffect(() => {
    if (!playing || frames.length < 2) return;
    const timer = window.setInterval(() => {
      setFrameIndex((current) => {
        if (current + 1 < frames.length) return current + 1;
        if (loop) return 0;
        setPlaying(false);
        return current;
      });
    }, 1000 / playbackFps);
    return () => window.clearInterval(timer);
  }, [frames.length, loop, playbackFps, playing]);

  useEffect(() => {
    for (const offset of [0, 1, 2]) {
      const candidate = frames[(frameIndex + offset) % Math.max(1, frames.length)];
      if (!candidate) continue;
      const src =
        "src" in candidate
          ? candidate.src
          : validationView === "comparison"
            ? candidate.comparisonSrc
            : candidate.differenceSrc;
      const image = new Image();
      image.src = src;
    }
  }, [frameIndex, frames, validationView]);

  const normalSequence = useMemo(
    () =>
      data?.sequences.find(
        (sequence) => sequence.scene === scene && sequence.viewKind === "beauty",
      ),
    [data, scene],
  );
  const normalFrame = useMemo(() => {
    if (!normalSequence?.frames.length) return undefined;
    const nearest = nearestByFrame(
      normalSequence.frames,
      measurementFrame,
      (frame) => frame.measurementFrame,
    );
    if (
      nearest &&
      Math.abs(nearest.measurementFrame - measurementFrame) <=
        Math.max(normalSequence.source.captureStride * 2, 120)
    ) {
      return nearest;
    }
    const progress = frames.length > 1 ? frameIndex / (frames.length - 1) : 0;
    return normalSequence.frames[
      Math.round(progress * (normalSequence.frames.length - 1))
    ];
  }, [frameIndex, frames.length, measurementFrame, normalSequence]);

  const sceneProfiles = useMemo(
    () => data?.profiles.filter((profile) => profile.scene === scene) ?? [],
    [data, scene],
  );
  const deferredProfile = sceneProfiles.find(
    (profile) => profile.renderer === "DonutDeferredPrepass",
  );
  const visbufProfile = sceneProfiles.find(
    (profile) => profile.renderer === "DonutVisGBuffer",
  );
  const deferredSample = nearestByFrame(
    deferredProfile?.samples ?? [],
    measurementFrame,
    (sample) => sample.frame,
  );
  const visbufSample = nearestByFrame(
    visbufProfile?.samples ?? [],
    measurementFrame,
    (sample) => sample.frame,
  );
  const rasterSamples = useMemo(
    () => data?.rasterTimeline.filter((sample) => sample.scene === scene) ?? [],
    [data, scene],
  );
  const rasterSample = nearestByFrame(
    rasterSamples,
    measurementFrame,
    (sample) => sample.measurementFrame,
  );

  const timeline = useMemo(() => {
    const frameNumbers = [
      ...new Set(
        sceneProfiles.flatMap((profile) =>
          profile.samples.map((sample) => sample.frame),
        ),
      ),
    ].sort((left, right) => left - right);
    return frameNumbers.map((frame) => {
      const row: Record<string, number> = { frame };
      for (const profile of sceneProfiles) {
        const sample = profile.samples.find((entry) => entry.frame === frame);
        if (!sample) continue;
        const prefix =
          profile.renderer === "DonutDeferredPrepass" ? "d" : "v";
        row[`${prefix}:total`] = sample.totalMs;
        for (const [pass, value] of Object.entries(sample.passes)) {
          if (pass === "clear" || value === null || value <= 0) continue;
          row[`${prefix}:${pass}`] = value;
        }
        if (profile.renderer === "DonutVisGBuffer") {
          row.indexCount = sample.indexCount;
        }
      }
      const raster = rasterSamples.find(
        (sample) => sample.measurementFrame === frame,
      );
      if (raster) {
        row.triangleCount = raster.triangleCount;
        row.totalFragments = raster.totalFragments;
        row.averageOverdraw = raster.averageOverdraw;
        row.quadEfficiency = raster.quadEfficiency;
      }
      return row;
    });
  }, [rasterSamples, sceneProfiles]);

  const availableSeries = useMemo(
    () =>
      SERIES_SPECS.filter((series) =>
        timeline.some((row) => Number.isFinite(row[series.key])),
      ),
    [timeline],
  );

  const moveFrame = useCallback(
    (delta: number) => {
      if (!frames.length) return;
      setFrameIndex((current) => {
        const next = current + delta;
        if (loop) return (next + frames.length) % frames.length;
        return Math.max(0, Math.min(frames.length - 1, next));
      });
    },
    [frames.length, loop],
  );

  const setFrameForMeasurement = useCallback(
    (target: number) => {
      if (!frames.length) return;
      let bestIndex = 0;
      for (let index = 1; index < frames.length; index += 1) {
        if (
          Math.abs(frames[index].measurementFrame - target) <
          Math.abs(frames[bestIndex].measurementFrame - target)
        ) {
          bestIndex = index;
        }
      }
      setFrameIndex(bestIndex);
    },
    [frames],
  );

  const changeScene = useCallback(
    (nextScene: SceneKey) => {
      if (nextScene === scene) return;
      setScene(nextScene);
      setFrameIndex(0);
      setPlaying(true);
    },
    [scene],
  );

  const changeDebug = useCallback(
    (nextId: string) => {
      const next = debugChoices.find((choice) => choice.id === nextId);
      if (!next) return;
      const progress = frames.length > 1 ? frameIndex / (frames.length - 1) : 0;
      const nextFrames = choiceFrames(next);
      setDebugId(nextId);
      setFrameIndex(Math.round(progress * Math.max(0, nextFrames.length - 1)));
    },
    [debugChoices, frameIndex, frames.length],
  );

  const toggleSeries = useCallback((key: string) => {
    setVisibleSeries((current) => {
      const next = new Set(current);
      if (next.has(key)) next.delete(key);
      else next.add(key);
      return next;
    });
  }, []);

  const onPlayerKey = useCallback(
    (event: React.KeyboardEvent<HTMLElement>) => {
      if (event.key === " ") {
        event.preventDefault();
        setPlaying((current) => !current);
      } else if (event.key === "ArrowLeft") {
        event.preventDefault();
        moveFrame(-1);
      } else if (event.key === "ArrowRight") {
        event.preventDefault();
        moveFrame(1);
      }
    },
    [moveFrame],
  );

  if (error) {
    return (
      <main className="state-shell">
        <section className="state-card">
          <p className="eyebrow">Data bundle unavailable</p>
          <h1>Could not open the camera atlas.</h1>
          <p>{error}</p>
        </section>
      </main>
    );
  }

  if (!data || !debugChoice || !activeFrame || !normalFrame) {
    return (
      <main className="state-shell">
        <section className="state-card" role="status">
          <p className="eyebrow">TVB Camera Atlas</p>
          <h1>Loading synchronized evidence</h1>
          <p>Preparing capture frames, pass timings, and raster workload.</p>
          <div className="loading-line" />
        </section>
      </main>
    );
  }

  const validationFrame =
    debugChoice.kind === "validation" ? (activeFrame as ValidationFrame) : null;
  const debugSrc =
    debugChoice.kind === "identity"
      ? (activeFrame as FrameRecord).src
      : validationView === "comparison"
        ? validationFrame?.comparisonSrc
        : validationFrame?.differenceSrc;
  const delta =
    (visbufSample?.totalMs ?? 0) - (deferredSample?.totalMs ?? 0);

  return (
    <main
      className="atlas"
      tabIndex={0}
      onKeyDown={onPlayerKey}
      aria-label="TVB synchronized camera and timing atlas"
    >
      <header className="masthead">
        <div className="brand">
          <span className="brand-icon" aria-hidden="true">
            <ScanLine size={18} />
          </span>
          <div>
            <p className="eyebrow">VisibilityBufferInfo · evidence viewer</p>
            <h1>TVB Camera Atlas</h1>
          </div>
        </div>
        <div className="provenance" aria-label="Dataset hardware provenance">
          <span className="dataset-current">
            Measured · RTX 5060 Ti 16GB
          </span>
          <span>Earlier archive · RTX 5070 · never pooled</span>
        </div>
      </header>

      <section className="command-bar" aria-label="Camera playback controls">
        <div className="scene-switcher" aria-label="Capture scene">
          {(Object.keys(SCENE_LABELS) as SceneKey[]).map((sceneKey) => (
            <button
              type="button"
              key={sceneKey}
              className={scene === sceneKey ? "scene-button active" : "scene-button"}
              aria-pressed={scene === sceneKey}
              onClick={() => changeScene(sceneKey)}
            >
              {SCENE_LABELS[sceneKey]}
            </button>
          ))}
        </div>

        <div className="transport">
          <button
            type="button"
            className="icon-button"
            aria-label="Previous frame"
            onClick={() => moveFrame(-1)}
          >
            <ChevronLeft size={17} />
          </button>
          <button
            type="button"
            className="icon-button primary"
            aria-label={playing ? "Pause playback" : "Play capture"}
            onClick={() => setPlaying((current) => !current)}
          >
            {playing ? <Pause size={17} /> : <Play size={17} />}
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Next frame"
            onClick={() => moveFrame(1)}
          >
            <ChevronRight size={17} />
          </button>
          <label className="loop-control">
            <input
              type="checkbox"
              checked={loop}
              onChange={(event) => setLoop(event.target.checked)}
            />
            <RefreshCcw size={13} />
            Infinite loop
          </label>
          <label className="compact-field">
            <span>Rate</span>
            <select
              aria-label="Playback speed"
              value={playbackFps}
              onChange={(event) => setPlaybackFps(Number(event.target.value))}
            >
              {[2, 4, 8, 12].map((fps) => (
                <option value={fps} key={fps}>
                  {fps} fps
                </option>
              ))}
            </select>
          </label>
        </div>

        <div className="frame-control">
          <span>
            frame {String(frameIndex + 1).padStart(2, "0")} /{" "}
            {String(frames.length).padStart(2, "0")}
          </span>
          <input
            aria-label="Select frame"
            type="range"
            min={0}
            max={Math.max(0, frames.length - 1)}
            value={frameIndex}
            onChange={(event) => setFrameIndex(Number(event.target.value))}
          />
          <strong>measurement {measurementFrame}</strong>
        </div>
      </section>

      <section className="viewer-grid">
        <article className="viewer-panel">
          <div className="viewer-heading">
            <div>
              <span className="panel-number">01</span>
              <div>
                <p className="panel-label">Normal output</p>
                <h2>{SCENE_LABELS[scene]} · textured PBR</h2>
              </div>
            </div>
            <span className="renderer-badge">VisBuf G-buffer</span>
          </div>
          <div className="image-stage">
            <img
              src={normalFrame.src}
              alt={`${SCENE_LABELS[scene]} textured PBR at measurement frame ${measurementFrame}`}
            />
            <div className="stage-overlay top">
              <span>camera frame {measurementFrame}</span>
              <span>{normalSequence?.frameCount} capture anchors</span>
            </div>
            <div className="stage-overlay bottom metric-overlay">
              <span>
                Deferred <strong>{formatMs(deferredSample?.totalMs)}</strong>
              </span>
              <span>
                VisBuf <strong>{formatMs(visbufSample?.totalMs)}</strong>
              </span>
              <span className={delta <= 0 ? "positive" : "negative"}>
                Δ <strong>{delta >= 0 ? "+" : ""}{delta.toFixed(3)} ms</strong>
              </span>
            </div>
          </div>
        </article>

        <article className="viewer-panel">
          <div className="viewer-heading debug-heading">
            <div>
              <span className="panel-number">02</span>
              <label className="debug-select">
                <span>Debug output</span>
                <select
                  aria-label="Capture output view"
                  value={debugChoice.id}
                  onChange={(event) => changeDebug(event.target.value)}
                >
                  {debugChoices.map((choice) => (
                    <option value={choice.id} key={choice.id}>
                      {choice.label}
                    </option>
                  ))}
                </select>
              </label>
            </div>
            {debugChoice.kind === "validation" ? (
              <div className="view-toggle" aria-label="Validation display">
                <button
                  type="button"
                  className={validationView === "comparison" ? "active" : ""}
                  aria-pressed={validationView === "comparison"}
                  onClick={() => setValidationView("comparison")}
                >
                  <GitCompareArrows size={13} />
                  Raster | VisBuf
                </button>
                <button
                  type="button"
                  className={validationView === "difference" ? "active" : ""}
                  aria-pressed={validationView === "difference"}
                  onClick={() => setValidationView("difference")}
                >
                  |difference|
                </button>
              </div>
            ) : (
              <span className="renderer-badge">VisBuf debug</span>
            )}
          </div>
          <div className="image-stage debug-stage">
            <img
              src={debugSrc}
              alt={`${debugChoice.label} debug output at measurement frame ${measurementFrame}`}
            />
            {debugChoice.kind === "validation" &&
              validationView === "comparison" && (
                <>
                  <span className="split-label left">Raster reference</span>
                  <span className="split-label right">VisBuf analytic</span>
                  <span className="split-rule" aria-hidden="true" />
                </>
              )}
            {debugChoice.kind === "validation" &&
              validationView === "difference" && (
                <span className="difference-label">
                  Absolute difference · 8 LSB = white
                </span>
              )}
            <div className="stage-overlay bottom">
              {validationFrame ? (
                <>
                  <span>
                    interior MAE{" "}
                    <strong>{validationFrame.metrics.interiorMaeLsb.toFixed(4)} LSB</strong>
                  </span>
                  <span>
                    exact{" "}
                    <strong>
                      {(validationFrame.metrics.interiorExactRatio * 100).toFixed(2)}%
                    </strong>
                  </span>
                  <span>
                    coverage mismatch{" "}
                    <strong>
                      {(validationFrame.metrics.coverageMismatchRatio * 100).toFixed(4)}%
                    </strong>
                  </span>
                </>
              ) : (
                <span>{debugChoice.description}</span>
              )}
            </div>
          </div>
        </article>
      </section>

      <section className="timeline-panel">
        <header className="timeline-heading">
          <div>
            <p className="panel-label">Camera Timeline</p>
            <h2>GPU pass time along the measured camera path</h2>
            <p>
              Debug capture cost is excluded. Passes are ordered by execution;
              clear is intentionally not measured.
            </p>
          </div>
          <label className="workload-select">
            <span>Secondary workload trace</span>
            <select
              aria-label="Timeline workload metric"
              value={workloadKey}
              onChange={(event) =>
                setWorkloadKey(event.target.value as WorkloadKey)
              }
            >
              <option value="none">None</option>
              {Object.entries(WORKLOAD_META).map(([key, metadata]) => (
                <option value={key} key={key}>
                  {metadata.label}
                </option>
              ))}
            </select>
          </label>
        </header>

        <div className="timeline-body">
          <div className="chart-wrap" data-testid="camera-timeline">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart
                data={timeline}
                margin={{ top: 10, right: workloadKey === "none" ? 14 : 58, bottom: 0, left: 0 }}
                onClick={(state: { activeLabel?: string | number } | null) => {
                  if (state?.activeLabel !== undefined) {
                    setFrameForMeasurement(Number(state.activeLabel));
                  }
                }}
              >
                <CartesianGrid stroke="rgba(172, 191, 213, 0.10)" vertical={false} />
                <XAxis
                  dataKey="frame"
                  type="number"
                  domain={["dataMin", "dataMax"]}
                  tick={{ fill: "#798898", fontSize: 10 }}
                  axisLine={{ stroke: "rgba(172, 191, 213, 0.17)" }}
                  tickLine={false}
                  tickFormatter={(value) => `${value}`}
                />
                <YAxis
                  yAxisId="gpu"
                  width={47}
                  tick={{ fill: "#798898", fontSize: 10 }}
                  axisLine={false}
                  tickLine={false}
                  tickFormatter={(value) => `${Number(value).toFixed(1)}`}
                  label={{
                    value: "GPU ms",
                    angle: -90,
                    position: "insideLeft",
                    fill: "#798898",
                    fontSize: 10,
                  }}
                />
                {workloadKey !== "none" && (
                  <YAxis
                    yAxisId="workload"
                    orientation="right"
                    width={54}
                    tick={{ fill: WORKLOAD_META[workloadKey].color, fontSize: 10 }}
                    axisLine={false}
                    tickLine={false}
                    tickFormatter={(value) =>
                      WORKLOAD_META[workloadKey].formatter(Number(value))
                    }
                  />
                )}
                <Tooltip
                  contentStyle={{
                    background: "#0d1319",
                    border: "1px solid rgba(172, 191, 213, 0.2)",
                    borderRadius: 10,
                    fontSize: 11,
                  }}
                  labelFormatter={(value) => `Measurement frame ${value}`}
                  formatter={(value, name) => {
                    const numeric = Number(value);
                    if (name === "__workload" && workloadKey !== "none") {
                      return [
                        WORKLOAD_META[workloadKey].formatter(numeric),
                        WORKLOAD_META[workloadKey].label,
                      ];
                    }
                    const series = SERIES_SPECS.find((item) => item.key === name);
                    return [formatMs(numeric), series?.label ?? name];
                  }}
                />
                <ReferenceLine
                  yAxisId="gpu"
                  x={measurementFrame}
                  stroke="#ffffff"
                  strokeWidth={1.2}
                  strokeDasharray="3 4"
                  ifOverflow="extendDomain"
                />
                {availableSeries
                  .filter((series) => visibleSeries.has(series.key))
                  .map((series) => (
                    <Line
                      key={series.key}
                      yAxisId="gpu"
                      type="monotone"
                      dataKey={series.key}
                      name={series.key}
                      stroke={series.color}
                      strokeWidth={series.width}
                      strokeDasharray={series.dash}
                      dot={false}
                      activeDot={{ r: 3 }}
                      connectNulls
                      isAnimationActive={false}
                    />
                  ))}
                {workloadKey !== "none" && (
                  <Line
                    yAxisId="workload"
                    type="monotone"
                    dataKey={workloadKey}
                    name="__workload"
                    stroke={WORKLOAD_META[workloadKey].color}
                    strokeWidth={1.5}
                    strokeDasharray="4 4"
                    dot={false}
                    connectNulls
                    isAnimationActive={false}
                  />
                )}
              </LineChart>
            </ResponsiveContainer>
          </div>

          <aside className="cursor-readout" aria-label="Current camera workload">
            <div>
              <span>Visible indices</span>
              <strong>{formatCompact(visbufSample?.indexCount)}</strong>
            </div>
            <div>
              <span>Input triangles</span>
              <strong>{formatCompact(rasterSample?.triangleCount)}</strong>
            </div>
            <div>
              <span>Fragments</span>
              <strong>{formatCompact(rasterSample?.totalFragments)}</strong>
            </div>
            <div>
              <span>Overdraw</span>
              <strong>
                {rasterSample ? `${rasterSample.averageOverdraw.toFixed(2)}×` : "—"}
              </strong>
            </div>
            <div>
              <span>Quad efficiency</span>
              <strong>
                {rasterSample
                  ? `${(rasterSample.quadEfficiency * 100).toFixed(1)}%`
                  : "—"}
              </strong>
            </div>
          </aside>
        </div>

        <div className="pass-controls" aria-label="GPU pass visibility controls">
          {(["Deferred", "VisBuf"] as const).map((renderer) => (
            <div className="pass-group" key={renderer}>
              <span className="pass-group-name">{renderer}</span>
              {availableSeries
                .filter((series) => series.renderer === renderer)
                .map((series) => (
                  <button
                    type="button"
                    key={series.key}
                    className={visibleSeries.has(series.key) ? "pass-chip active" : "pass-chip"}
                    aria-pressed={visibleSeries.has(series.key)}
                    onClick={() => toggleSeries(series.key)}
                  >
                    <span
                      className="pass-swatch"
                      style={{ backgroundColor: series.color }}
                    />
                    {series.shortLabel}
                  </button>
                ))}
            </div>
          ))}
        </div>
      </section>

      <footer className="atlas-footer">
        <span>
          {data.summary.resultRows} successful campaign rows ·{" "}
          {data.summary.validationPairs} raster/VisBuf frame pairs
        </span>
        <span>
          {data.hardware.gpu.name} · driver {data.hardware.gpu.driver} · branch{" "}
          {data.provenance.sourceBranch}
        </span>
      </footer>
    </main>
  );
}
