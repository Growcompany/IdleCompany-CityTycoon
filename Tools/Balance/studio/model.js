'use strict';
// Studio 의 읽기 모델 — "지금 게임이 들고 있는 값" 을 정직하게 보여주는 계층.
//
// 계산은 하지 않는다. tune.js 가 카탈로그/라우팅/값 파싱의 SOT 이고, 여기서는 그 결과에
// (1) 표/행/컬럼 allowlist, (2) CSV 권위 여부, (3) 시뮬 모델 범위, (4) stale 판정용 지문을 얹는다.
//
// 정직성 규칙: 없는 산출물·미완료 calibration·모델 밖 표를 성공처럼 칠하지 않는다.
// 그래야 "before/after 가 같다" 를 사용자가 "효과 없음" 으로 오독하지 않는다.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const tuneDefault = require('../tune.js');
const { PROJECT_CURVE_TARGETS } = require('../reseed_project_curve.js');

// 곡선 노브 → project_curve_params.json 의 파라미터 이름. 값이 아니라 "어느 노브가 어느 축을 미는가".
const CURVE_PARAM = Object.freeze({ reqCurve: 'ANCHOR', devCostCoef: 'COST_COEF' });

class ModelError extends Error {
  constructor(code, message, details = {}) {
    super(message);
    this.name = 'ModelError';
    this.code = code;
    this.details = details;
  }
}

const sha256 = buf => crypto.createHash('sha256').update(buf).digest('hex');
// 키 순서가 흔들리면 같은 상태가 다른 지문을 낸다 — stale 오판의 원인이라 정렬해서 직렬화한다.
function stableJson(value) {
  if (value === null || typeof value !== 'object') return JSON.stringify(value);
  if (Array.isArray(value)) return `[${value.map(stableJson).join(',')}]`;
  return `{${Object.keys(value).sort().map(k => `${JSON.stringify(k)}:${stableJson(value[k])}`).join(',')}}`;
}

const readJsonOrNull = p => {
  try {
    return JSON.parse(fs.readFileSync(p, 'utf8'));
  } catch {
    return null;
  }
};

function typeOfValue(v) {
  if (Array.isArray(v)) return 'array';
  if (v === null) return 'null';
  if (typeof v === 'object') return 'object';
  return typeof v; // number | string | boolean
}

function createStudioModel({ paths, tuneModule = tuneDefault, editorCheck = tuneDefault.editorProcessCount } = {}) {
  if (!paths) throw new Error('createStudioModel: paths 가 필요합니다.');

  let cache = null;

  function load() {
    if (cache) return cache;
    const catalog = tuneModule.buildCatalog({
      valuesPath: paths.valuesPath,
      scanPath: paths.scanPath,
      manifestSrcPath: paths.manifestSrcPath,
      manifestEditorPath: paths.manifestEditorPath,
      repoRoot: paths.repoRoot,
    });
    const doc = JSON.parse(fs.readFileSync(paths.valuesPath, 'utf8'));
    const consumption = tuneModule.simConsumption();
    cache = { catalog, doc, consumption, loadedAt: new Date().toISOString() };
    return cache;
  }

  const invalidate = () => { cache = null; };

  // ── 표 ────────────────────────────────────────────────────────────────────
  const csvPathOf = table => {
    const p = path.join(paths.csvDir, `${table}_Import.csv`);
    return fs.existsSync(p) ? p : null;
  };
  const relRepo = p => (p ? path.relative(paths.repoRoot, p).replace(/\\/g, '/') : null);

  function assertTable(name) {
    const { catalog, doc } = load();
    const assetPath = catalog.dtPaths[name];
    if (!assetPath || !Array.isArray(doc.dt[name])) {
      throw new ModelError('TABLE_NOT_FOUND', `알 수 없는 DataTable: ${name} — manifest_editor.json 과 values.json 에 있는 표만 열 수 있습니다.`, { table: name });
    }
    return { assetPath, rows: doc.dt[name] };
  }

  function columnMeta(rows, name, rowKeyColumn) {
    let type = null;
    let numeric = 0;
    let min = null;
    let max = null;
    for (const r of rows) {
      const v = r[name];
      if (v === undefined) continue;
      const t = typeOfValue(v);
      if (t === 'null') continue;
      type = type === null ? t : (type === t ? t : 'mixed');
      if (t === 'number') {
        numeric++;
        min = min === null ? v : Math.min(min, v);
        max = max === null ? v : Math.max(max, v);
      }
    }
    const editable = type === 'number' && numeric > 0 && name !== rowKeyColumn;
    return {
      name,
      type: type || 'empty',
      editable,
      numericRows: numeric,
      min,
      max,
      // 왜 못 고치는지 UI 가 말해줄 수 있어야 한다 — 그냥 회색으로 두면 버그처럼 보인다.
      readOnlyReason: editable ? null : (name === rowKeyColumn ? '행 이름' : `${type || 'empty'} 컬럼은 초기 버전에서 읽기 전용입니다`),
    };
  }

  function getTable(name) {
    const { assetPath, rows } = assertTable(name);
    const rowKeyColumn = 'Name';
    const colNames = [];
    for (const r of rows) for (const k of Object.keys(r)) if (!colNames.includes(k)) colNames.push(k);

    const columns = colNames.map(c => columnMeta(rows, c, rowKeyColumn));
    const csvPath = csvPathOf(name);
    const consumedTable = tuneModule.simConsumes(name, rowKeyColumn, load().consumption);
    return {
      name,
      assetPath,
      uassetPath: uassetPathOf(assetPath),
      csvPath,
      csvRelPath: relRepo(csvPath),
      source: csvPath ? 'csv' : 'uasset',
      rowKeyColumn,
      rowCount: rows.length,
      columns,
      rows: rows.map(r => ({ name: String(r[rowKeyColumn]), cells: r })),
      consumed: {
        table: consumedTable.table,
        uncertain: Boolean(consumedTable.uncertain),
        columns: Object.fromEntries(columns.map(c => [c.name, tuneModule.simConsumes(name, c.name, load().consumption).column])),
      },
    };
  }

  function listTables() {
    const { catalog, doc, consumption } = load();
    return Object.keys(catalog.dtPaths)
      .filter(name => Array.isArray(doc.dt[name]))
      .sort()
      .map(name => {
        const rows = doc.dt[name];
        const rowKeyColumn = 'Name';
        const colNames = [];
        for (const r of rows) for (const k of Object.keys(r)) if (!colNames.includes(k)) colNames.push(k);
        const numericColumns = colNames.filter(c => columnMeta(rows, c, rowKeyColumn).editable);
        const csvPath = csvPathOf(name);
        const c = tuneModule.simConsumes(name, rowKeyColumn, consumption);
        return {
          name,
          assetPath: catalog.dtPaths[name],
          csvPath,
          csvRelPath: relRepo(csvPath),
          source: csvPath ? 'csv' : 'uasset',
          rowCount: rows.length,
          columnCount: colNames.length,
          numericColumns,
          simConsumed: c.table,
          simUncertain: Boolean(c.uncertain),
        };
      });
  }

  // manifest 의 /Game/... 경로만 uasset 파일 경로로 편다 — 사용자 입력 경로는 절대 쓰지 않는다.
  function uassetPathOf(assetPath) {
    const m = /^\/Game\/(.+)$/.exec(assetPath || '');
    if (!m) return null;
    return path.join(paths.contentRoot, ...m[1].split('/')) + '.uasset';
  }

  // ── 변경 검증 ─────────────────────────────────────────────────────────────
  function validateChange({ knob, value, allRows = false } = {}) {
    const { catalog, doc } = load();
    const parsed = tuneModule.parseKnob(knob);
    const route = tuneModule.routeOf(parsed, doc);
    const catRow = catalog.rows.find(r => r.key === (parsed.kind === 'dt-cell' ? knob : parsed.name));

    if (route === 'dt-curve') {
      // ⚠ CLI 는 곡선 노브의 맨숫자를 배율로 읽지만, GUI 에서 "140" 은 거의 항상 "ANCHOR 를 140 으로" 라는 뜻이다.
      //    그대로 ×140 을 걸면 곡선이 통째로 무너지므로 Studio 는 배율 문법을 명시적으로 요구한다.
      const spec = tuneModule.parseValueSpec(value);
      if (!('mult' in spec)) {
        throw new ModelError('CURVE_MULT_ONLY', `${parsed.name} 은 배율 노브입니다 — x1.2 또는 +20% 형태로 주세요. (140 처럼 쓰면 ×140 이 됩니다)`, { knob });
      }
      const params = readJsonOrNull(paths.curveParamsPath) || {};
      const param = CURVE_PARAM[parsed.name];
      const current = Number.isFinite(params[param]) ? params[param] : null;
      return {
        knob, route, kind: parsed.kind, table: null, column: null, row: null,
        spec, allRows: false, appliable: true,
        curveParam: param,
        curveBefore: current,
        curveAfter: current === null ? null : Number((current * spec.mult).toPrecision(6)),
        source: { kind: 'curve', tables: PROJECT_CURVE_TARGETS.map(t => t.table), paramsPath: relRepo(paths.curveParamsPath) },
        consumed: { table: true, column: true, note: '시뮬은 Game 산업만 모델링합니다 — Finance/IT 효과는 미검증입니다.' },
        sens: catRow ? catRow.sens : { state: 'unscanned', effect: null },
        targets: [],
      };
    }

    if (route !== 'dt') {
      // src / cdo — 위치 안내만. 자동 수정하지 않는다.
      const spec = tuneModule.parseValueSpec(value);
      return {
        knob, route, kind: parsed.kind, table: null, column: null, row: null,
        spec, allRows: false, appliable: false,
        source: { kind: route, location: catRow ? catRow.how : null },
        consumed: { table: true, column: true },
        sens: catRow ? catRow.sens : { state: 'unscanned', effect: null },
        targets: [],
        current: doc.values[parsed.name] ? doc.values[parsed.name].v : null,
      };
    }

    const { assetPath, rows } = assertTable(parsed.table);
    const spec = tuneModule.parseValueSpec(value);
    const candidates = parsed.row === null ? rows : rows.filter(r => String(r.Name) === String(parsed.row));
    if (!candidates.length) {
      throw new ModelError('ROW_NOT_FOUND', `DT 행 없음: ${parsed.table}@${parsed.row} — 행 이름 오타?`, { table: parsed.table, row: parsed.row });
    }
    const numeric = candidates.filter(r => typeof r[parsed.column] === 'number');
    if (!numeric.length) {
      throw new ModelError('COLUMN_NOT_NUMERIC', `수치 셀 없음: ${parsed.table}.${parsed.column} — 컬럼명 오타이거나 수치 컬럼이 아닙니다.`, { table: parsed.table, column: parsed.column });
    }
    if (numeric.length > 1 && 'set' in spec && allRows !== true) {
      throw new ModelError(
        'ALL_ROWS_REQUIRED',
        `${parsed.table}.${parsed.column} 의 ${numeric.length}개 행에 같은 절대값(${spec.set})을 씁니다 — 곡선이 평평해집니다. 행을 지정하거나 배율(x1.2)을 쓰거나, 정말 전 행이면 '전체 행'을 확인하세요.`,
        { table: parsed.table, column: parsed.column, rows: numeric.length },
      );
    }

    const csvPath = csvPathOf(parsed.table);
    const consumed = tuneModule.simConsumes(parsed.table, parsed.column, load().consumption);
    return {
      knob, route, kind: parsed.kind,
      table: parsed.table, column: parsed.column, row: parsed.row,
      spec, allRows: allRows === true, appliable: true,
      source: {
        kind: csvPath ? 'csv' : 'uasset',
        csvPath, csvRelPath: relRepo(csvPath),
        assetPath, uassetPath: uassetPathOf(assetPath),
      },
      consumed,
      sens: catRow ? catRow.sens : { state: 'unscanned', effect: null },
      targets: numeric.map(r => ({
        row: String(r.Name),
        before: r[parsed.column],
        after: 'set' in spec ? spec.set : r[parsed.column] * spec.mult,
      })),
    };
  }

  // ── stale 판정 지문 ───────────────────────────────────────────────────────
  // "미리보기 이후 원본이 바뀌었는가" 만 답한다 — 변경 내용이 아니라 원본 상태만 넣는다.
  function fingerprintChange(change) {
    const { doc } = load();
    const meta = doc.meta || {};
    const parts = { values: `${meta.srcHash || '?'}:${meta.editorHash || '?'}` };

    if (change.route === 'dt') {
      const { csvPath, uassetPath } = change.source;
      if (csvPath && fs.existsSync(csvPath)) parts.csv = sha256(fs.readFileSync(csvPath));
      if (uassetPath && fs.existsSync(uassetPath)) {
        const st = fs.statSync(uassetPath);
        parts.uasset = `${st.size}:${st.mtimeMs}`;
      }
    } else if (change.route === 'dt-curve') {
      if (fs.existsSync(paths.curveParamsPath)) parts.params = sha256(fs.readFileSync(paths.curveParamsPath));
      parts.curveCsv = {};
      for (const t of PROJECT_CURVE_TARGETS) {
        const p = path.join(paths.csvDir, t.csvName);
        parts.curveCsv[t.table] = fs.existsSync(p) ? sha256(fs.readFileSync(p)) : null;
      }
    }
    return sha256(stableJson(parts));
  }

  // ── bootstrap ─────────────────────────────────────────────────────────────
  function bootstrap() {
    if (!fs.existsSync(paths.valuesPath)) {
      throw new ModelError('VALUES_MISSING', `values.json 이 없습니다: ${paths.valuesPath} — 에디터에서 값을 재추출하세요.`, { path: paths.valuesPath });
    }
    const { catalog, doc, consumption, loadedAt } = load();
    const tables = listTables();
    const missing = [];
    const track = (p, what) => { if (!fs.existsSync(p)) missing.push({ path: p, what }); };
    track(paths.scanPath, '감도 스캔(scan.json)');
    track(paths.calibrationPath, '보정(calibration.json)');
    track(paths.manifestEditorPath, '에디터 manifest');
    track(paths.manifestSrcPath, '소스 manifest');
    track(paths.curveParamsPath, '곡선 파라미터');

    const calibrationRaw = readJsonOrNull(paths.calibrationPath);
    const scanRaw = readJsonOrNull(paths.scanPath);
    const editorCount = Number(editorCheck()) || 0;

    const consumedTables = tables.filter(t => t.simConsumed);
    const industries = [...consumption.tables]
      .filter(t => /^DT_Project_/.test(t))
      .map(t => t.slice('DT_Project_'.length));

    const warnings = [];
    if (!calibrationRaw || calibrationRaw._gateOk !== true) {
      warnings.push({
        code: 'CALIBRATION_INCOMPLETE',
        message: 'calibration gate 가 통과 상태가 아닙니다 — 절대시간(인앱 h, 진입일) 지표는 미보정 값입니다. 상대 비교(before/after)만 신뢰하세요.',
      });
    }
    if (!catalog.scanned) {
      warnings.push({ code: 'SCAN_MISSING', message: '감도 스캔(out/scan.json)이 없어 모든 노브가 미스캔입니다 — 죽은 노브/절벽을 구분할 수 없습니다.' });
    }
    warnings.push({
      code: 'SIM_COVERAGE',
      message: `시뮬레이션이 실제로 읽는 표는 ${consumedTables.length}/${tables.length}종이고 프로젝트 시뮬은 ${industries.join('+') || '?'} 산업만 모델링합니다 — 나머지 표의 before/after 가 같은 것은 정상이며 "효과 없음" 을 뜻하지 않습니다.`,
    });
    if (editorCount === 0) {
      warnings.push({ code: 'EDITOR_NONE', message: 'UnrealEditor 가 실행 중이 아닙니다 — 미리보기는 가능하지만 실제 적용은 할 수 없습니다.' });
    } else if (editorCount >= 2) {
      warnings.push({ code: 'EDITOR_MULTIPLE', message: `UnrealEditor 가 ${editorCount}개 실행 중입니다 — 어느 에디터에 붙을지 보장되지 않고 uasset 저장이 실패합니다(Error Code 32). 적용 전에 여분을 종료하세요.` });
    }

    return {
      loadedAt,
      editorCount,
      project: { repoRoot: paths.repoRoot, name: path.basename(paths.repoRoot) },
      data: {
        valuesPath: paths.valuesPath,
        valuesRelPath: relRepo(paths.valuesPath),
        valuesMeta: doc.meta || {},
        scanGeneratedAt: scanRaw && scanRaw.meta ? scanRaw.meta.generatedAt : null,
      },
      catalog: { rows: catalog.rows, scanned: catalog.scanned, threshold: catalog.threshold },
      tables: {
        total: tables.length,
        csvBacked: tables.filter(t => t.source === 'csv').length,
        uassetOnly: tables.filter(t => t.source === 'uasset').length,
      },
      coverage: {
        totalTables: tables.length,
        consumedTables: consumedTables.length,
        consumed: consumedTables.map(t => t.name),
        uncertain: tables.filter(t => t.simUncertain).map(t => t.name),
        industry: industries.join('+') || null,
      },
      calibration: {
        gateOk: Boolean(calibrationRaw && calibrationRaw._gateOk),
        generatedAt: calibrationRaw ? calibrationRaw._generatedAt : null,
        assumptions: (calibrationRaw && calibrationRaw._assumptions) || [],
        notes: (calibrationRaw && calibrationRaw._notes) || [],
      },
      profiles: (scanRaw && scanRaw.profiles) || [],
      scan: scanRaw ? { rows: scanRaw.rows || [], dead: scanRaw.dead || [], cliffs: scanRaw.cliffs || [], baseline: scanRaw.baseline || null } : null,
      missing,
      warnings,
    };
  }

  return {
    bootstrap,
    listTables,
    getTable,
    validateChange,
    fingerprintChange,
    invalidate,
    // 표 → /Game 에셋 경로. 리임포트가 쓰는 진짜 allowlist 라 model 을 거쳐야 한다.
    dtPaths: () => ({ ...load().catalog.dtPaths }),
    uassetPathOf,
    editorCount: () => Number(editorCheck()) || 0,
    paths,
  };
}

module.exports = { createStudioModel, ModelError, CURVE_PARAM, stableJson, sha256 };
