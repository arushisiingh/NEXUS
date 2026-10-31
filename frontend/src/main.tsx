import { StrictMode, useEffect, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { Activity, Cpu, Database, Layers3, Play, Thermometer } from 'lucide-react';
import { ApiClientError, getHealth, solveThermal, type HealthResponse, type ThermalParameters, type ThermalResponse } from './api/client';
import GeometryStudio from './GeometryStudio';
import PerformanceStudio from './PerformanceStudio';
import NeuralStudio from './NeuralStudio';
import HardwareStudio from './HardwareStudio';
import './styles.css';

const modules = ['Overview', 'Geometry Lab', 'Thermal Studio', 'Performance Lab', 'Hardware Studio', 'Neural Lab', 'Reports & Exports', 'Settings'];
const defaultThermal: ThermalParameters = { length_m: 1, area_m2: 1e-4, conductivity_W_mK: 10, heat_generation_W_m3: 10000, left_temperature_C: 100, right_temperature_C: 20, element_count: 40 };

function Overview({ health, error, lastSuccessfulConnection, onOpenThermal, onOpenGeometry }: { health: HealthResponse | null; error: string; lastSuccessfulConnection: string | null; onOpenThermal: () => void; onOpenGeometry: () => void }) {
  const cards = [
    { icon: Layers3, label: 'Geometry files', value: '0', sub: 'Awaiting import' },
    { icon: Thermometer, label: 'Thermal studies', value: '0', sub: 'No runs recorded' },
    { icon: Database, label: 'Compute workers', value: '—', sub: 'Benchmark not started' },
    { icon: Activity, label: 'API service', value: health ? 'READY' : 'OFFLINE', sub: health ? health.api_status : 'Start nexus-api' },
  ];
  const connectionDetails = health ? `${health.api_status.toUpperCase()} · ${health.cpp_standard} · ${lastSuccessfulConnection ?? 'just now'}` : error || 'No successful connection recorded';
  return <>
    <header><div><p className="eyebrow">UNIFIED ENGINEERING WORKBENCH / OVERVIEW</p><h1>Systems workspace</h1></div><div className="connection"><span className={health ? 'dot online' : 'dot'} />{health ? 'BACKEND ONLINE' : 'BACKEND OFFLINE'}</div></header>
    <section className="hero"><div><p className="eyebrow">WORKSPACE STATUS</p><h2>Build, inspect, simulate.</h2><p className="muted">A single environment for geometry, thermal analysis, compute performance, hardware integration and neural experimentation.</p></div><div className="hero-state"><Activity size={18}/><span>{connectionDetails}</span></div></section>
    <div className="grid">{cards.map(({icon: Icon, label, value, sub}) => <article className="card" key={label}><Icon size={18}/><p>{label}</p><strong>{value}</strong><small>{sub}</small></article>)}</div>
    <section className="panel"><div><p className="eyebrow">MODULES</p><h3>Open a technical workspace</h3></div><div className="module-grid">{modules.slice(1, 6).map((m) => <button className="module" key={m} onClick={m === 'Thermal Studio' ? onOpenThermal : m === 'Geometry Lab' ? onOpenGeometry : undefined}><span>{m}</span><small>{m === 'Thermal Studio' ? 'Run a solver study' : m === 'Geometry Lab' ? 'Inspect CAD and results' : 'Foundation ready'} <b>→</b></small></button>)}</div></section>
  </>;
}

function LineChart({ result }: { result: ThermalResponse }) {
  const width = 760, height = 250, pad = 34;
  const allTemps = [...result.nodal_temperatures_C, ...result.analytical_temperatures_C];
  const min = Math.min(...allTemps) - 5, max = Math.max(...allTemps) + 5;
  const point = (x: number, y: number) => `${pad + x * (width - pad * 2)},${height - pad - ((y - min) / (max - min)) * (height - pad * 2)}`;
  const fem = result.nodal_positions_m.map((x, i) => point(x / result.input_parameters.length_m, result.nodal_temperatures_C[i])).join(' ');
  const exact = result.nodal_positions_m.map((x, i) => point(x / result.input_parameters.length_m, result.analytical_temperatures_C[i])).join(' ');
  return <svg className="chart" viewBox={`0 0 ${width} ${height}`} role="img" aria-label="FEM and analytical temperature comparison"><line x1={pad} y1={height-pad} x2={width-pad} y2={height-pad} stroke="#466273"/><line x1={pad} y1={pad} x2={pad} y2={height-pad} stroke="#466273"/><polyline points={fem} fill="none" stroke="#59c3df" strokeWidth="3"/><polyline points={exact} fill="none" stroke="#f1c76b" strokeWidth="2" strokeDasharray="7 5"/><text x={width-pad-110} y={pad+10} fill="#59c3df">FEM</text><text x={width-pad-110} y={pad+30} fill="#f1c76b">Analytical</text><text x={width/2-50} y={height-5} fill="#8fa6b4">Position x (m)</text></svg>;
}

function ThermalStudio() {
  const [parameters, setParameters] = useState(defaultThermal);
  const [result, setResult] = useState<ThermalResponse | null>(null);
  const [convergenceResults, setConvergenceResults] = useState<ThermalResponse[]>([]);
  const [error, setError] = useState('');
  const [running, setRunning] = useState(false);
  const update = (key: keyof ThermalParameters, value: string) => setParameters((current) => ({ ...current, [key]: key === 'element_count' ? Number.parseInt(value, 10) : Number.parseFloat(value) }));
  const run = async () => {
    setRunning(true); setError('');
    try {
      const allResults = await Promise.all([10, 20, 40, 80].map((element_count) => solveThermal({ ...parameters, element_count })));
      setConvergenceResults(allResults);
      setResult(allResults.find((entry) => entry.input_parameters.element_count === parameters.element_count) ?? allResults[0]);
    } catch (e: unknown) { setResult(null); setConvergenceResults([]); setError(e instanceof ApiClientError ? e.message : 'Unexpected thermal solver failure'); } finally { setRunning(false); }
  };
  const exportCsv = () => {
    if (!result) return;
    const rows = ['x_m,fem_temperature_C,analytical_temperature_C', ...result.nodal_positions_m.map((x, i) => `${x},${result.nodal_temperatures_C[i]},${result.analytical_temperatures_C[i]}`)];
    const url = URL.createObjectURL(new Blob([rows.join('\n')], { type: 'text/csv' })); const link = document.createElement('a'); link.href = url; link.download = 'nexus-thermal-result.csv'; link.click(); URL.revokeObjectURL(url);
  };
  return <><header><div><p className="eyebrow">THERMAL STUDIO / STEADY-STATE 1D FEM</p><h1>Thermal analysis</h1></div><div className="simulation-badge"><span className="dot online" />C++ SOLVER READY</div></header><div className="thermal-layout"><section className="panel inputs"><div className="panel-heading"><div><p className="eyebrow">MODEL INPUTS</p><h3>Uniform rod</h3></div><span className="unit-tag">SI / °C</span></div><div className="input-grid">{([['length_m','Length','m'],['area_m2','Area','m²'],['conductivity_W_mK','Conductivity','W/(m·K)'],['heat_generation_W_m3','Heat generation','W/m³'],['left_temperature_C','T left','°C'],['right_temperature_C','T right','°C']] as const).map(([key, label, unit]) => <label key={key}>{label}<div className="input-wrap"><input type="number" value={parameters[key]} onChange={(e) => update(key, e.target.value)} step="any"/><span>{unit}</span></div></label>)}</div><label className="mesh-label">Mesh elements<select value={parameters.element_count} onChange={(e) => update('element_count', e.target.value)}><option value={10}>10</option><option value={20}>20</option><option value={40}>40</option><option value={80}>80</option></select></label><button className="primary-action" onClick={run} disabled={running}><Play size={16}/>{running ? 'SOLVING…' : 'RUN SIMULATION'}</button>{error && <p className="error-text">{error}</p>}<p className="input-note">Defaults match the assignment benchmark. Results are computed by the C++17 finite-element engine.</p></section><section className="panel results-panel"><div className="panel-heading"><div><p className="eyebrow">RESULTS</p><h3>Temperature field</h3></div>{result && <button className="secondary-action" onClick={exportCsv}>EXPORT CSV</button>}</div>{!result && !running && <div className="empty-state"><Thermometer size={28}/><strong>Ready to solve</strong><span>Configure the rod and run a simulation to populate the scientific results.</span></div>}{running && <div className="empty-state"><Activity size={28}/><strong>Solving thermal field…</strong><span>Assembly, boundary conditions and linear system solution are executing in C++.</span></div>}{result && <><div className="result-summary"><div><small>MAX MIDPOINT ERROR</small><strong>{result.maximum_absolute_midpoint_error_C.toExponential(4)} °C</strong></div><div><small>ELEMENTS</small><strong>{result.input_parameters.element_count}</strong></div><div><small>EXECUTION</small><strong>{result.execution_duration_ms.toFixed(3)} ms</strong></div></div><LineChart result={result}/><div className="legend"><span><i className="legend-fem"/>FEM interpolation</span><span><i className="legend-exact"/>Analytical solution</span></div></>}</section></div>{result && <section className="panel convergence"><div className="panel-heading"><div><p className="eyebrow">VERIFICATION</p><h3>Mesh convergence</h3></div><span className="unit-tag">Actual C++ runs</span></div><table className="convergence-table"><thead><tr><th>Elements</th><th>Element length (m)</th><th>Max midpoint error (°C)</th><th>Solver time (ms)</th></tr></thead><tbody>{convergenceResults.map((entry) => <tr key={entry.input_parameters.element_count}><td>{entry.input_parameters.element_count}</td><td>{(entry.input_parameters.length_m / entry.input_parameters.element_count).toPrecision(5)}</td><td>{entry.maximum_absolute_midpoint_error_C.toExponential(5)}</td><td>{entry.execution_duration_ms.toFixed(3)}</td></tr>)}</tbody></table><p className="input-note">Each row is returned by an independent C++ solve using the same physical parameters.</p></section>}</>;
}

export default function App() {
  const [activeModule, setActiveModule] = useState('Overview');
  const [health, setHealth] = useState<HealthResponse | null>(null);
  const [error, setError] = useState('');
  const [lastSuccessfulConnection, setLastSuccessfulConnection] = useState<string | null>(null);
  useEffect(() => { getHealth().then((result) => { setHealth(result); setError(''); setLastSuccessfulConnection(new Date().toLocaleTimeString()); }).catch((e: unknown) => setError(e instanceof ApiClientError ? e.message : 'Unexpected backend health-check failure')); }, []);
  return <div className="app"><aside><div className="brand"><div className="mark">N</div><div><strong>NEXUS</strong><small>ENGINEERING WORKBENCH</small></div></div><nav>{modules.map((module) => <button className={activeModule === module ? 'active' : ''} key={module} onClick={() => setActiveModule(module)}>{module}</button>)}</nav><div className="side-foot">BUILD 0.1.0<br/><span>LOCAL WORKSPACE</span></div></aside><main>{activeModule === 'Thermal Studio' ? <ThermalStudio/> : activeModule === 'Geometry Lab' ? <GeometryStudio/> : activeModule === 'Performance Lab' ? <PerformanceStudio/> : activeModule === 'Neural Lab' ? <NeuralStudio/> : activeModule === 'Hardware Studio' ? <HardwareStudio/> : <Overview health={health} error={error} lastSuccessfulConnection={lastSuccessfulConnection} onOpenThermal={() => setActiveModule('Thermal Studio')} onOpenGeometry={() => setActiveModule('Geometry Lab')}/>}</main></div>;
}

const rootElement = document.getElementById('root');
if (!rootElement) throw new Error('NEXUS frontend root element was not found.');
createRoot(rootElement).render(<StrictMode><App /></StrictMode>);
