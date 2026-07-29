"use client";

import {
  Boxes,
  ChevronLeft,
  ChevronRight,
  Cpu,
  Gauge,
  MonitorPlay,
  Pause,
  Play,
  RefreshCcw,
  ScanLine,
} from "lucide-react";
import {
  Bar,
  BarChart,
  CartesianGrid,
  Cell,
  Legend,
  Line,
  LineChart,
  ReferenceLine,
  ResponsiveContainer,
  Scatter,
  ScatterChart,
  Tooltip,
  XAxis,
  YAxis,
  ZAxis,
} from "recharts";
import { useCallback, useEffect, useMemo, useState } from "react";

type MetricKey = "avgMs" | "medianMs" | "p90Ms" | "p99Ms";
type EvidenceTab = "camera" | "experiments" | "hardware" | "runs";

interface FrameRecord {
  imageIndex: number;
  renderFrame: number;
  measurementFrame: number;
  src: string;
}

interface CaptureSequence {
  id: string;
  label: string;
  scene: string;
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
  };
  frameCount: number;
  frames: FrameRecord[];
}

interface ProfileSample {
  frame: number;
  totalMs: number;
  indexCount: number;
  passes: Record<string, number>;
}

interface CameraProfile {
  id: string;
  hardwareId: string;
  scene: string;
  renderer: string;
  runIndex: number;
  samples: ProfileSample[];
}

interface ResultRow {
  id: string;
  hardwareId: string;
  config: string;
  scene: string;
  renderer: string;
  status: string;
  runIndex: number;
  repeat: number;
  metrics: Record<MetricKey | "minMs" | "maxMs", number>;
  parameters: Record<string, string | number | null>;
  passes: Array<{ name: string; timeAvgMs: number }>;
}

interface Experiment {
  config: string;
  title: string;
  group: string;
  xKey: string;
  xLabel: string;
  secondaryKey?: string;
  secondaryLabel?: string;
  rowCount: number;
}

interface Dataset {
  id: string;
  label: string;
  gpu: string;
  role: "current" | "historical";
  measuredOn: string | null;
  scope: string;
  includedInExperimentExplorer: boolean;
  attributionNote?: string;
}

interface HardwareComparisonRow {
  id: string;
  datasetId: string;
  hardwareLabel: string;
  scene: string;
  renderer: string;
  rendererVariant: number;
  resolution: string;
  warmupFrames: number;
  measureFrames: number;
  profileWindowFrames: number;
  vfc: number;
  textures: number;
  metrics: Record<MetricKey, number>;
  source: string;
}

interface AtlasData {
  title: string;
  subtitle: string;
  provenance: {
    campaignStatus: string;
    campaignStartedAt: string;
    campaignFinishedAt: string;
    captureFinishedAt: string;
    campaignExpectedRuns: number;
    campaignSuccessfulRuns: number;
    campaignSalvagedRuns: number;
    campaignFailedRuns: number;
    campaignSkippedRuns: number;
    sourceBranch: string;
    baseCommit: string;
  };
  hardware: {
    id: string;
    gpu: {
      name: string;
      driver: string;
      memoryMiB: number;
      powerLimitW: number;
      maxGraphicsClockMHz: number;
      maxMemoryClockMHz: number;
      computeCapability: string;
    };
    cpu: {
      name: string;
      cores: number;
      logicalProcessors: number;
      maxClockMHz: number;
    };
    system: {
      memoryBytes: number;
      os: string;
      build: string;
    };
    toolchain: {
      visualStudio: string;
      msvc: string;
      python: string;
      build: string;
    };
  };
  datasets: Dataset[];
  summary: {
    resultRows: number;
    captureSequences: number;
    captureFrames: number;
    playableFrames: number;
    scenes: string[];
    renderers: string[];
    experiments: number;
  };
  experiments: Experiment[];
  results: ResultRow[];
  profiles: CameraProfile[];
  sequences: CaptureSequence[];
  hardwareComparison: HardwareComparisonRow[];
}

const COLORS = ["#77dcff", "#ffbb63", "#ff7199", "#77e0a3", "#b595ff"];
const RENDERER_COLORS: Record<string, string> = {
  DonutVisGBuffer: "#77dcff",
  DonutDeferred: "#b595ff",
  DonutDeferredPrepass: "#ffbb63",
};
const HARDWARE_COLORS = {
  "RTX 5060 Ti 16GB": "#77dcff",
  "RTX 5070": "#ffbb63",
};
const METRIC_LABELS: Record<MetricKey, string> = {
  avgMs: "Average",
  medianMs: "Median",
  p90Ms: "P90",
  p99Ms: "P99",
};

function formatMs(value: number | null | undefined, digits = 3) {
  return value === null || value === undefined
    ? "—"
    : `${value.toFixed(digits)} ms`;
}

function formatCompact(value: number | null | undefined) {
  if (value === null || value === undefined) return "—";
  return new Intl.NumberFormat("en", {
    notation: "compact",
    maximumFractionDigits: 2,
  }).format(value);
}

function average(values: number[]) {
  return values.length
    ? values.reduce((sum, value) => sum + value, 0) / values.length
    : 0;
}

function nearestSample(profile: CameraProfile | undefined, frame: number) {
  if (!profile?.samples.length) return undefined;
  return profile.samples.reduce((nearest, sample) =>
    Math.abs(sample.frame - frame) < Math.abs(nearest.frame - frame)
      ? sample
      : nearest,
  );
}

function tooltipStyle() {
  return {
    background: "#111820",
    border: "1px solid rgba(178, 196, 218, 0.22)",
    borderRadius: "9px",
    color: "#f4f7fb",
    fontSize: "11px",
  };
}

export function AtlasApp() {
  const [data, setData] = useState<AtlasData | null>(null);
  const [error, setError] = useState("");
  const [sequenceId, setSequenceId] = useState("sponza-pbr");
  const [frameIndex, setFrameIndex] = useState(0);
  const [playing, setPlaying] = useState(true);
  const [loop, setLoop] = useState(true);
  const [playbackFps, setPlaybackFps] = useState(8);
  const [tab, setTab] = useState<EvidenceTab>("camera");
  const [experimentConfig, setExperimentConfig] = useState(
    "30_final_selected_full_camera.json",
  );
  const [sceneFilter, setSceneFilter] = useState("All");
  const [rendererFilter, setRendererFilter] = useState("All");
  const [metric, setMetric] = useState<MetricKey>("avgMs");

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
          setError(
            reason instanceof Error ? reason.message : "Unknown data error",
          );
        }
      });
    return () => {
      live = false;
    };
  }, []);

  const sequence = useMemo(
    () => data?.sequences.find((item) => item.id === sequenceId),
    [data, sequenceId],
  );
  const frame = sequence?.frames[frameIndex];
  const profile = useMemo(
    () => data?.profiles.find((item) => item.id === sequence?.profileId),
    [data, sequence],
  );
  const sample = useMemo(
    () => nearestSample(profile, frame?.measurementFrame ?? 0),
    [profile, frame],
  );

  useEffect(() => {
    if (!playing || !sequence) return;
    const timer = window.setInterval(() => {
      setFrameIndex((current) => {
        if (current + 1 < sequence.frameCount) return current + 1;
        if (loop) return 0;
        setPlaying(false);
        return current;
      });
    }, 1000 / playbackFps);
    return () => window.clearInterval(timer);
  }, [loop, playbackFps, playing, sequence]);

  useEffect(() => {
    if (!sequence) return;
    for (const offset of [1, 2]) {
      const next = sequence.frames[(frameIndex + offset) % sequence.frameCount];
      if (next) {
        const image = new Image();
        image.src = next.src;
      }
    }
  }, [frameIndex, sequence]);

  const moveFrame = useCallback(
    (delta: number) => {
      if (!sequence) return;
      setFrameIndex((current) => {
        const next = current + delta;
        if (loop) {
          return (next + sequence.frameCount) % sequence.frameCount;
        }
        return Math.max(0, Math.min(sequence.frameCount - 1, next));
      });
    },
    [loop, sequence],
  );

  const onPlayerKey = useCallback(
    (event: React.KeyboardEvent<HTMLDivElement>) => {
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

  const changeSequence = useCallback(
    (nextId: string) => {
      if (!data || !sequence) return;
      const next = data.sequences.find((item) => item.id === nextId);
      if (!next) return;
      const progress =
        sequence.frameCount > 1 ? frameIndex / (sequence.frameCount - 1) : 0;
      setSequenceId(next.id);
      setFrameIndex(Math.round(progress * Math.max(0, next.frameCount - 1)));
    },
    [data, frameIndex, sequence],
  );

  const changeScene = useCallback(
    (scene: string) => {
      if (!data || !sequence) return;
      const matchingView = data.sequences.find(
        (item) => item.scene === scene && item.view === sequence.view,
      );
      const fallback = data.sequences.find(
        (item) => item.scene === scene && item.viewKind === "beauty",
      );
      const next = matchingView ?? fallback;
      if (next) changeSequence(next.id);
    },
    [changeSequence, data, sequence],
  );

  const sceneSequences = useMemo(
    () =>
      data?.sequences.filter((item) => item.scene === sequence?.scene) ?? [],
    [data, sequence],
  );

  const experiment = useMemo(
    () =>
      data?.experiments.find((item) => item.config === experimentConfig),
    [data, experimentConfig],
  );

  const filteredResults = useMemo(() => {
    if (!data) return [];
    return data.results.filter(
      (row) =>
        row.config === experimentConfig &&
        (sceneFilter === "All" || row.scene === sceneFilter) &&
        (rendererFilter === "All" || row.renderer === rendererFilter),
    );
  }, [data, experimentConfig, rendererFilter, sceneFilter]);

  const experimentSeries = useMemo(() => {
    if (!experiment) return [];
    const groups = new Map<
      string,
      Array<{ x: number; y: number; label: string }>
    >();
    for (const row of filteredResults) {
      const rawX = row.parameters[experiment.xKey] ?? row.runIndex;
      const x = typeof rawX === "number" ? rawX : row.runIndex;
      const key = `${row.renderer} · ${row.scene}`;
      const points = groups.get(key) ?? [];
      points.push({
        x,
        y: row.metrics[metric],
        label: `${row.scene} · ${row.renderer}`,
      });
      groups.set(key, points);
    }
    return [...groups.entries()].map(([name, points]) => ({
      name,
      points: points.sort((a, b) => a.x - b.x),
    }));
  }, [experiment, filteredResults, metric]);

  const rendererSummary = useMemo(() => {
    const values = new Map<string, number[]>();
    for (const row of filteredResults) {
      const list = values.get(row.renderer) ?? [];
      list.push(row.metrics[metric]);
      values.set(row.renderer, list);
    }
    return [...values.entries()].map(([renderer, samples]) => ({
      renderer,
      value: average(samples),
    }));
  }, [filteredResults, metric]);

  const percentileSummary = useMemo(() => {
    const metricValues = (key: MetricKey) =>
      average(filteredResults.map((row) => row.metrics[key]));
    return {
      avgMs: metricValues("avgMs"),
      medianMs: metricValues("medianMs"),
      p90Ms: metricValues("p90Ms"),
      p99Ms: metricValues("p99Ms"),
    };
  }, [filteredResults]);

  const passSummary = useMemo(() => {
    const values = new Map<string, number[]>();
    for (const row of filteredResults) {
      for (const pass of row.passes) {
        if (pass.name === "total") continue;
        const list = values.get(pass.name) ?? [];
        list.push(pass.timeAvgMs);
        values.set(pass.name, list);
      }
    }
    return [...values.entries()]
      .map(([name, samples]) => ({ name, value: average(samples) }))
      .sort((a, b) => b.value - a.value)
      .slice(0, 9);
  }, [filteredResults]);

  const hardwareComparisonChart = useMemo(() => {
    if (!data) return [];
    const grouped = new Map<
      string,
      {
        label: string;
        scene: string;
        renderer: string;
        "RTX 5060 Ti 16GB"?: number;
        "RTX 5070"?: number;
      }
    >();
    for (const row of data.hardwareComparison) {
      const key = `${row.scene}:${row.renderer}`;
      const entry = grouped.get(key) ?? {
        label: `${row.scene} · ${row.renderer.replace("Donut", "")}`,
        scene: row.scene,
        renderer: row.renderer,
      };
      entry[row.hardwareLabel as keyof typeof HARDWARE_COLORS] =
        row.metrics[metric];
      grouped.set(key, entry);
    }
    return [...grouped.values()];
  }, [data, metric]);

  const cameraTimeline = useMemo(() => {
    if (!data || !sequence) return [];
    const sceneProfiles = data.profiles.filter(
      (item) => item.scene === sequence.scene,
    );
    const frames = [
      ...new Set(
        sceneProfiles.flatMap((item) => item.samples.map((entry) => entry.frame)),
      ),
    ].sort((a, b) => a - b);
    return frames.map((profileFrame) => {
      const row: Record<string, number> = { frame: profileFrame };
      for (const item of sceneProfiles) {
        const profileSample = item.samples.find(
          (candidate) => candidate.frame === profileFrame,
        );
        if (profileSample) row[item.renderer] = profileSample.totalMs;
      }
      return row;
    });
  }, [data, sequence]);

  const activePasses = useMemo(() => {
    if (!sample) return [];
    return Object.entries(sample.passes)
      .filter(([, value]) => value !== null && value > 0)
      .map(([name, value]) => ({ name, value }))
      .sort((a, b) => b.value - a.value);
  }, [sample]);

  if (error) {
    return (
      <main className="error-shell">
        <section className="error-card">
          <p className="eyebrow">Data bundle unavailable</p>
          <h1>Could not open the experiment atlas.</h1>
          <p className="subtitle">{error}</p>
        </section>
      </main>
    );
  }

  if (!data || !sequence || !frame) {
    return (
      <main className="loading-shell">
        <section className="loading-card" role="status">
          <p className="eyebrow">TVB Performance Atlas</p>
          <h1>Loading the evidence bundle</h1>
          <p className="subtitle">
            Preparing 396 benchmark results and synchronized frame captures.
          </p>
          <div className="loading-track" />
        </section>
      </main>
    );
  }

  const scenes = [...new Set(data.sequences.map((item) => item.scene))];
  const profileRenderers = data.profiles
    .filter((item) => item.scene === sequence.scene)
    .map((item) => item.renderer);
  const maxPass = Math.max(...activePasses.map((item) => item.value), 0.001);
  const maxSummaryPass = Math.max(
    ...passSummary.map((item) => item.value),
    0.001,
  );

  return (
    <main className="atlas-shell">
      <header className="topbar">
        <div className="brand-block">
          <div className="brand-mark" aria-hidden="true">
            <ScanLine size={22} strokeWidth={1.8} />
          </div>
          <div>
            <p className="eyebrow">Triangle visibility buffer · research viewer</p>
            <h1>{data.title}</h1>
            <p className="subtitle">{data.subtitle}</p>
          </div>
        </div>
        <div className="campaign-status">
          <span className="status-pill">
            <span className="status-dot" />
            Campaign verified
          </span>
          <span className="status-copy">
            {data.provenance.campaignSuccessfulRuns}/
            {data.provenance.campaignExpectedRuns} success
            <br />
            {data.summary.captureFrames} captured frames · RTX 5060 Ti 16GB
          </span>
        </div>
      </header>

      <section className="hero-grid" aria-label="Synchronized frame viewer">
        <article
          className="viewer-card"
          tabIndex={0}
          onKeyDown={onPlayerKey}
          data-testid="frame-player"
        >
          <div className="viewer-stage">
            {/* Captures are already resized and content-addressed by the data pipeline. */}
            {/* eslint-disable-next-line @next/next/no-img-element */}
            <img
              className="viewer-frame"
              src={frame.src}
              alt={`${sequence.label}, frame ${frame.imageIndex + 1}`}
            />
            <div className="viewer-topline">
              <span className="frame-badge accent">
                <MonitorPlay size={13} />
                {sequence.scene}
              </span>
              <span className="frame-badge">{sequence.viewLabel}</span>
            </div>
            <div className="viewer-bottomline">
              <span className="frame-badge">{sequence.renderer}</span>
              <div className="live-readout">
                <div className="readout-cell">
                  <span className="readout-label">Camera window</span>
                  <span className="readout-value">
                    {frame.measurementFrame.toLocaleString()}
                  </span>
                </div>
                <div className="readout-cell">
                  <span className="readout-label">GPU total</span>
                  <span className="readout-value">
                    {formatMs(sample?.totalMs)}
                  </span>
                </div>
                <div className="readout-cell">
                  <span className="readout-label">Visible indices</span>
                  <span className="readout-value">
                    {formatCompact(sample?.indexCount)}
                  </span>
                </div>
              </div>
            </div>
          </div>
          <div className="viewer-footer">
            <div className="frame-controls">
              <div className="transport">
                <button
                  className="icon-button"
                  type="button"
                  aria-label="Previous frame"
                  onClick={() => moveFrame(-1)}
                >
                  <ChevronLeft size={17} />
                </button>
                <button
                  className="icon-button primary"
                  type="button"
                  aria-label={playing ? "Pause playback" : "Start playback"}
                  onClick={() => setPlaying((current) => !current)}
                >
                  {playing ? <Pause size={16} /> : <Play size={16} />}
                </button>
                <button
                  className="icon-button"
                  type="button"
                  aria-label="Next frame"
                  onClick={() => moveFrame(1)}
                >
                  <ChevronRight size={17} />
                </button>
                <button
                  className={`icon-button ${loop ? "is-active" : ""}`}
                  type="button"
                  aria-label="Toggle infinite loop"
                  aria-pressed={loop}
                  onClick={() => setLoop((current) => !current)}
                >
                  <RefreshCcw size={15} />
                </button>
              </div>
              <input
                className="frame-slider"
                aria-label="Select frame"
                type="range"
                min={0}
                max={sequence.frameCount - 1}
                value={frameIndex}
                onChange={(event) => {
                  setFrameIndex(Number(event.target.value));
                  setPlaying(false);
                }}
              />
              <span className="frame-index">
                {String(frameIndex + 1).padStart(2, "0")} /{" "}
                {String(sequence.frameCount).padStart(2, "0")}
              </span>
            </div>
            <div className="sequence-note">
              <span>{sequence.description}</span>
              <code>
                stride {sequence.source.captureStride} · 1280×720 source
              </code>
            </div>
          </div>
        </article>

        <aside className="control-card">
          <div>
            <div className="section-kicker">
              <h2>Playback laboratory</h2>
              <span>SPACE / ← / →</span>
            </div>
            <div className="field-grid">
              <label className="field-label">
                Scene
                <select
                  className="field-select"
                  aria-label="Capture scene"
                  value={sequence.scene}
                  onChange={(event) => changeScene(event.target.value)}
                >
                  {scenes.map((scene) => (
                    <option key={scene}>{scene}</option>
                  ))}
                </select>
              </label>
              <label className="field-label">
                Output / debug view
                <select
                  className="field-select"
                  aria-label="Capture output view"
                  value={sequence.id}
                  onChange={(event) => changeSequence(event.target.value)}
                >
                  {sceneSequences.map((item) => (
                    <option key={item.id} value={item.id}>
                      {item.viewLabel}
                    </option>
                  ))}
                </select>
              </label>
              <label className="field-label">
                Playback speed
                <select
                  className="field-select"
                  aria-label="Playback speed"
                  value={playbackFps}
                  onChange={(event) =>
                    setPlaybackFps(Number(event.target.value))
                  }
                >
                  <option value={2}>2 fps · inspect</option>
                  <option value={4}>4 fps · slow</option>
                  <option value={8}>8 fps · default</option>
                  <option value={12}>12 fps · capture rate</option>
                </select>
              </label>
              <div className="toggle-row">
                <label className="toggle">
                  Autoplay
                  <input
                    type="checkbox"
                    checked={playing}
                    onChange={(event) => setPlaying(event.target.checked)}
                  />
                </label>
                <label className="toggle">
                  Infinite loop
                  <input
                    type="checkbox"
                    checked={loop}
                    onChange={(event) => setLoop(event.target.checked)}
                  />
                </label>
              </div>
            </div>
          </div>
          <div>
            <p className="sequence-description">
              {sequence.viewKind === "debug"
                ? `Debug mode ${sequence.debugMode}. ${sequence.description} Timing below remains the matching validated VisBuf camera profile, not the debug renderer's capture overhead.`
                : `${sequence.description} The timing window below is synchronized by measurement-frame index.`}
            </p>
            <div className="mini-metrics">
              <div className="mini-metric">
                <span>Window total</span>
                <strong>{formatMs(sample?.totalMs)}</strong>
              </div>
              <div className="mini-metric">
                <span>Index workload</span>
                <strong>{formatCompact(sample?.indexCount)}</strong>
              </div>
              <div className="mini-metric">
                <span>Renderer variant</span>
                <strong>{sequence.rendererVariant}</strong>
              </div>
              <div className="mini-metric">
                <span>Measurement frame</span>
                <strong>{frame.measurementFrame}</strong>
              </div>
            </div>
          </div>
        </aside>
      </section>

      <section className="camera-strip" aria-label="Current camera metrics">
        <div className="camera-stat">
          <span>GPU total</span>
          <strong className="cyan">{formatMs(sample?.totalMs)}</strong>
        </div>
        <div className="camera-stat">
          <span>Visibility pass</span>
          <strong>{formatMs(sample?.passes.visibility)}</strong>
        </div>
        <div className="camera-stat">
          <span>G-buffer / geometry</span>
          <strong>
            {formatMs(
              sample?.passes.gbuffer ??
                sample?.passes.geometry ??
                sample?.passes.depth_prepass,
            )}
          </strong>
        </div>
        <div className="camera-stat">
          <span>Visible index count</span>
          <strong>{formatCompact(sample?.indexCount)}</strong>
        </div>
      </section>

      <section className="evidence-section" aria-labelledby="evidence-title">
        <div className="evidence-heading">
          <div>
            <p className="eyebrow">Measured evidence</p>
            <h2 id="evidence-title">The numbers beneath the image</h2>
            <p>
              Camera-window profiles and the complete validated material campaign.
            </p>
          </div>
          <span className="evidence-count">
            {data.summary.resultRows} rows · {data.summary.experiments} experiments ·{" "}
            {data.summary.scenes.length} scene classes · RTX 5060 Ti 16GB
          </span>
        </div>

        <div className="tab-list" role="tablist" aria-label="Evidence view">
          {[
            ["camera", "Camera timeline"],
            ["experiments", "Experiment explorer"],
            ["hardware", "GPU provenance"],
            ["runs", "Run ledger"],
          ].map(([value, label]) => (
            <button
              key={value}
              className={`tab-button ${tab === value ? "active" : ""}`}
              type="button"
              role="tab"
              aria-selected={tab === value}
              onClick={() => setTab(value as EvidenceTab)}
            >
              {label}
            </button>
          ))}
        </div>

        {tab === "camera" && (
          <div className="evidence-grid">
            <article className="evidence-card">
              <div className="section-kicker">
                <h3>{sequence.scene} camera-path GPU time</h3>
                <span>60-frame measurement windows</span>
              </div>
              <div className="chart-shell">
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart
                    data={cameraTimeline}
                    margin={{ top: 8, right: 18, left: -16, bottom: 2 }}
                  >
                    <CartesianGrid stroke="rgba(178,196,218,.10)" vertical={false} />
                    <XAxis
                      dataKey="frame"
                      stroke="#6f7b89"
                      tick={{ fill: "#8e9aaa", fontSize: 10 }}
                      tickLine={false}
                      axisLine={false}
                    />
                    <YAxis
                      stroke="#6f7b89"
                      tick={{ fill: "#8e9aaa", fontSize: 10 }}
                      tickLine={false}
                      axisLine={false}
                      tickFormatter={(value: number) => `${value.toFixed(1)}`}
                    />
                    <Tooltip
                      contentStyle={tooltipStyle()}
                      labelStyle={{ color: "#8e9aaa" }}
                    />
                    <Legend wrapperStyle={{ fontSize: "10px", color: "#8e9aaa" }} />
                    {profileRenderers.map((rendererName, index) => (
                      <Line
                        key={rendererName}
                        type="monotone"
                        dataKey={rendererName}
                        dot={false}
                        stroke={
                          RENDERER_COLORS[rendererName] ?? COLORS[index % COLORS.length]
                        }
                        strokeWidth={1.8}
                        connectNulls
                      />
                    ))}
                    <ReferenceLine
                      x={frame.measurementFrame}
                      stroke="#f4f7fb"
                      strokeDasharray="3 4"
                      strokeOpacity={0.62}
                    />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </article>
            <article className="evidence-card">
              <div className="section-kicker">
                <h3>Current pass anatomy</h3>
                <span>frame {sample?.frame ?? "—"}</span>
              </div>
              <div className="percentile-grid">
                <div className="percentile-cell">
                  <span>Total GPU</span>
                  <strong>{formatMs(sample?.totalMs)}</strong>
                </div>
                <div className="percentile-cell">
                  <span>Index count</span>
                  <strong>{formatCompact(sample?.indexCount)}</strong>
                </div>
              </div>
              <div className="pass-list">
                {activePasses.map((item) => (
                  <div className="pass-row" key={item.name}>
                    <span>{item.name}</span>
                    <div className="pass-track">
                      <div
                        className="pass-fill"
                        style={{ width: `${(item.value / maxPass) * 100}%` }}
                      />
                    </div>
                    <span className="pass-value">{item.value.toFixed(3)} ms</span>
                  </div>
                ))}
              </div>
            </article>
          </div>
        )}

        {(tab === "experiments" || tab === "runs") && (
          <div className="filter-bar">
            <label className="field-label">
              Experiment
              <select
                className="field-select"
                value={experimentConfig}
                onChange={(event) => setExperimentConfig(event.target.value)}
              >
                {data.experiments.map((item) => (
                  <option key={item.config} value={item.config}>
                    {item.title} · {item.rowCount}
                  </option>
                ))}
              </select>
            </label>
            <label className="field-label">
              Scene
              <select
                className="field-select"
                value={sceneFilter}
                onChange={(event) => setSceneFilter(event.target.value)}
              >
                <option>All</option>
                {data.summary.scenes.map((item) => (
                  <option key={item}>{item}</option>
                ))}
              </select>
            </label>
            <label className="field-label">
              Renderer
              <select
                className="field-select"
                value={rendererFilter}
                onChange={(event) => setRendererFilter(event.target.value)}
              >
                <option>All</option>
                {data.summary.renderers.map((item) => (
                  <option key={item}>{item}</option>
                ))}
              </select>
            </label>
            <label className="field-label">
              Metric
              <select
                className="field-select"
                value={metric}
                onChange={(event) => setMetric(event.target.value as MetricKey)}
              >
                {Object.entries(METRIC_LABELS).map(([value, label]) => (
                  <option key={value} value={value}>
                    {label}
                  </option>
                ))}
              </select>
            </label>
          </div>
        )}

        {tab === "experiments" && (
          <div className="evidence-grid">
            <article className="evidence-card">
              <div className="section-kicker">
                <h3>{experiment?.title}</h3>
                <span>
                  RTX 5060 Ti 16GB · x {experiment?.xLabel} / y{" "}
                  {METRIC_LABELS[metric]} GPU ms
                </span>
              </div>
              {experimentSeries.length ? (
                <div className="chart-shell">
                  <ResponsiveContainer width="100%" height="100%">
                    <ScatterChart
                      margin={{ top: 10, right: 18, left: -15, bottom: 4 }}
                    >
                      <CartesianGrid stroke="rgba(178,196,218,.10)" />
                      <XAxis
                        type="number"
                        dataKey="x"
                        name={experiment?.xLabel}
                        stroke="#6f7b89"
                        tick={{ fill: "#8e9aaa", fontSize: 10 }}
                        tickLine={false}
                      />
                      <YAxis
                        type="number"
                        dataKey="y"
                        name={`${METRIC_LABELS[metric]} (ms)`}
                        stroke="#6f7b89"
                        tick={{ fill: "#8e9aaa", fontSize: 10 }}
                        tickLine={false}
                      />
                      <ZAxis range={[34, 34]} />
                      <Tooltip
                        cursor={{ strokeDasharray: "3 3" }}
                        contentStyle={tooltipStyle()}
                      />
                      <Legend
                        wrapperStyle={{ fontSize: "10px", color: "#8e9aaa" }}
                      />
                      {experimentSeries.map((item, index) => (
                        <Scatter
                          key={item.name}
                          name={item.name}
                          data={item.points}
                          fill={COLORS[index % COLORS.length]}
                          line
                        />
                      ))}
                    </ScatterChart>
                  </ResponsiveContainer>
                </div>
              ) : (
                <div className="chart-empty">No measured rows match these filters.</div>
              )}
            </article>
            <article className="evidence-card">
              <div className="section-kicker">
                <h3>Renderer aggregate</h3>
                <span>{filteredResults.length} measured rows</span>
              </div>
              <div className="percentile-grid">
                {Object.entries(percentileSummary).map(([key, value]) => (
                  <div className="percentile-cell" key={key}>
                    <span>{METRIC_LABELS[key as MetricKey]}</span>
                    <strong>{formatMs(value)}</strong>
                  </div>
                ))}
              </div>
              <div className="chart-shell compact">
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart
                    data={rendererSummary}
                    layout="vertical"
                    margin={{ top: 0, right: 18, left: 22, bottom: 0 }}
                  >
                    <CartesianGrid
                      stroke="rgba(178,196,218,.10)"
                      horizontal={false}
                    />
                    <XAxis
                      type="number"
                      tick={{ fill: "#8e9aaa", fontSize: 10 }}
                      axisLine={false}
                      tickLine={false}
                    />
                    <YAxis
                      type="category"
                      dataKey="renderer"
                      width={116}
                      tick={{ fill: "#b7c1ce", fontSize: 10 }}
                      axisLine={false}
                      tickLine={false}
                    />
                    <Tooltip contentStyle={tooltipStyle()} />
                    <Bar dataKey="value" radius={[0, 5, 5, 0]}>
                      {rendererSummary.map((item, index) => (
                        <Cell
                          key={item.renderer}
                          fill={
                            RENDERER_COLORS[item.renderer] ??
                            COLORS[index % COLORS.length]
                          }
                        />
                      ))}
                    </Bar>
                  </BarChart>
                </ResponsiveContainer>
              </div>
            </article>
          </div>
        )}

        {tab === "hardware" && (
          <>
            <div className="hardware-filter">
              <div>
                <p className="eyebrow">Matched camera baselines</p>
                <p>
                  Same scene, renderer, resolution, camera length, textures, and VFC;
                  hardware and repository revision may differ.
                </p>
              </div>
              <label className="field-label">
                Metric
                <select
                  className="field-select"
                  value={metric}
                  onChange={(event) => setMetric(event.target.value as MetricKey)}
                >
                  {Object.entries(METRIC_LABELS).map(([value, label]) => (
                    <option key={value} value={value}>
                      {label}
                    </option>
                  ))}
                </select>
              </label>
            </div>
            <div className="evidence-grid">
              <article className="evidence-card">
                <div className="section-kicker">
                  <h3>Current vs previous GPU evidence</h3>
                  <span>{METRIC_LABELS[metric]} total GPU time · lower is better</span>
                </div>
                <div className="chart-shell hardware-chart">
                  <ResponsiveContainer width="100%" height="100%">
                    <BarChart
                      data={hardwareComparisonChart}
                      layout="vertical"
                      margin={{ top: 8, right: 20, left: 48, bottom: 4 }}
                    >
                      <CartesianGrid
                        stroke="rgba(178,196,218,.10)"
                        horizontal={false}
                      />
                      <XAxis
                        type="number"
                        unit=" ms"
                        tick={{ fill: "#8e9aaa", fontSize: 10 }}
                        axisLine={false}
                        tickLine={false}
                      />
                      <YAxis
                        type="category"
                        dataKey="label"
                        width={156}
                        tick={{ fill: "#b7c1ce", fontSize: 9 }}
                        axisLine={false}
                        tickLine={false}
                      />
                      <Tooltip
                        contentStyle={tooltipStyle()}
                        formatter={(value) => formatMs(Number(value))}
                      />
                      <Legend wrapperStyle={{ fontSize: "10px", color: "#8e9aaa" }} />
                      {Object.entries(HARDWARE_COLORS).map(([hardwareLabel, color]) => (
                        <Bar
                          key={hardwareLabel}
                          dataKey={hardwareLabel}
                          fill={color}
                          radius={[0, 4, 4, 0]}
                          barSize={8}
                        />
                      ))}
                    </BarChart>
                  </ResponsiveContainer>
                </div>
              </article>
              <article className="evidence-card dataset-card">
                <div className="section-kicker">
                  <h3>Dataset boundaries</h3>
                  <span>never pooled across GPUs</span>
                </div>
                <div className="dataset-list">
                  {data.datasets.map((dataset) => (
                    <div className="dataset-row" key={dataset.id}>
                      <span
                        className={`dataset-swatch ${dataset.role}`}
                        aria-hidden="true"
                      />
                      <div>
                        <strong>{dataset.label}</strong>
                        <p>{dataset.scope}</p>
                        {dataset.attributionNote && (
                          <small>{dataset.attributionNote}</small>
                        )}
                      </div>
                    </div>
                  ))}
                </div>
                <p className="comparison-caution">
                  This view preserves the recorded milliseconds. It does not normalize
                  one GPU to the other or claim an architectural speedup because the
                  archived run also predates today&apos;s code revision. Historical
                  profiles used 10-frame windows; today&apos;s campaign used 60-frame
                  windows, so percentile shapes are contextual rather than strict A/B
                  evidence.
                </p>
              </article>
            </div>
          </>
        )}

        {tab === "runs" && (
          <div className="evidence-grid">
            <article className="evidence-card">
              <div className="section-kicker">
                <h3>Measured run ledger</h3>
                <span>success rows only · no synthetic failures</span>
              </div>
              <div className="run-table-wrap">
                <table className="run-table">
                  <thead>
                    <tr>
                      <th>Run</th>
                      <th>GPU</th>
                      <th>Scene</th>
                      <th>Renderer</th>
                      <th>Average</th>
                      <th>Median</th>
                      <th>P90</th>
                      <th>P99</th>
                    </tr>
                  </thead>
                  <tbody>
                    {filteredResults.map((row) => (
                      <tr key={row.id}>
                        <td className="numeric">{row.runIndex}</td>
                        <td>RTX 5060 Ti 16GB</td>
                        <td>{row.scene}</td>
                        <td>{row.renderer}</td>
                        <td className="numeric">{formatMs(row.metrics.avgMs)}</td>
                        <td className="numeric">
                          {formatMs(row.metrics.medianMs)}
                        </td>
                        <td className="numeric">{formatMs(row.metrics.p90Ms)}</td>
                        <td className="numeric">{formatMs(row.metrics.p99Ms)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </article>
            <article className="evidence-card">
              <div className="section-kicker">
                <h3>Mean pass breakdown</h3>
                <span>filtered rows</span>
              </div>
              <div className="pass-list">
                {passSummary.map((item) => (
                  <div className="pass-row" key={item.name}>
                    <span>{item.name}</span>
                    <div className="pass-track">
                      <div
                        className="pass-fill"
                        style={{
                          width: `${(item.value / maxSummaryPass) * 100}%`,
                        }}
                      />
                    </div>
                    <span className="pass-value">{item.value.toFixed(3)} ms</span>
                  </div>
                ))}
              </div>
            </article>
          </div>
        )}
      </section>

      <section className="hardware-section" aria-label="Hardware and provenance">
        <article className="hardware-card">
          <div className="hardware-title">
            <Gauge size={15} />
            Today&apos;s measurement GPU
          </div>
          <div className="hardware-list">
            <div className="hardware-line">
              <span>Device</span>
              <strong>{data.hardware.gpu.name}</strong>
            </div>
            <div className="hardware-line">
              <span>VRAM</span>
              <strong>
                {(data.hardware.gpu.memoryMiB / 1024).toFixed(1)} GiB
              </strong>
            </div>
            <div className="hardware-line">
              <span>Power limit</span>
              <strong>{data.hardware.gpu.powerLimitW} W</strong>
            </div>
            <div className="hardware-line">
              <span>Max graphics clock</span>
              <strong>{data.hardware.gpu.maxGraphicsClockMHz} MHz</strong>
            </div>
            <div className="hardware-line">
              <span>Driver / compute</span>
              <strong>
                {data.hardware.gpu.driver} / {data.hardware.gpu.computeCapability}
              </strong>
            </div>
          </div>
        </article>
        <article className="hardware-card">
          <div className="hardware-title">
            <Cpu size={15} />
            Host
          </div>
          <div className="hardware-list">
            <div className="hardware-line">
              <span>CPU</span>
              <strong>{data.hardware.cpu.name}</strong>
            </div>
            <div className="hardware-line">
              <span>Cores / threads</span>
              <strong>
                {data.hardware.cpu.cores} /{" "}
                {data.hardware.cpu.logicalProcessors}
              </strong>
            </div>
            <div className="hardware-line">
              <span>System memory</span>
              <strong>
                {(data.hardware.system.memoryBytes / 2 ** 30).toFixed(1)} GiB
              </strong>
            </div>
            <div className="hardware-line">
              <span>OS build</span>
              <strong>{data.hardware.system.build}</strong>
            </div>
          </div>
        </article>
        <article className="hardware-card">
          <div className="hardware-title">
            <Boxes size={15} />
            Evidence provenance
          </div>
          <div className="hardware-list">
            <div className="hardware-line">
              <span>Current / previous</span>
              <strong>RTX 5060 Ti 16GB / RTX 5070</strong>
            </div>
            <div className="hardware-line">
              <span>Campaign</span>
              <strong>
                {data.provenance.campaignSuccessfulRuns}/
                {data.provenance.campaignExpectedRuns}
              </strong>
            </div>
            <div className="hardware-line">
              <span>Capture</span>
              <strong>
                {data.summary.captureSequences} paths /{" "}
                {data.summary.playableFrames} playable
              </strong>
            </div>
            <div className="hardware-line">
              <span>Build</span>
              <strong>{data.hardware.toolchain.build}</strong>
            </div>
            <div className="hardware-line">
              <span>Base commit</span>
              <strong>{data.provenance.baseCommit}</strong>
            </div>
          </div>
        </article>
      </section>
    </main>
  );
}
