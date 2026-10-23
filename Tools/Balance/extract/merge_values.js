const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

// UE export_data_table_to_json_string 실측(5.4) = row 객체 배열, 각 row에 Name 포함(안전망: dict 형태도 대비)
function normalizeDtTable(table) {
  if (Array.isArray(table)) return table;
  return Object.entries(table).map(([name, fields]) => ({ Name: name, ...fields }));
}

function mergeValues(srcVals, editor) {
  const values = {};
  for (const [k, e] of Object.entries(srcVals)) values[k] = e;
  for (const [k, e] of Object.entries(editor.cdo ?? {})) {
    if (values[k]) console.warn(`[merge] ${k}: cdo(${e.v})가 src(${values[k].v})를 대체`);
    values[k] = { v: e.v, src: 'cdo', loc: e.loc };
  }
  const dt = {};
  for (const [name, table] of Object.entries(editor.dt ?? {})) {
    if (values[name]) throw new Error(`[merge] dt 테이블명 '${name}' 이 src/cdo 값 키와 충돌 — 네이밍 확인 필요`);
    dt[name] = normalizeDtTable(table);
  }
  return { values, dt };
}

function writeValuesJson(outPath, srcManifestPath, repoRoot, editorJsonPath) {
  const { extractSrc } = require('./extract_src.js');
  const srcVals = extractSrc(srcManifestPath, repoRoot);
  const editor = JSON.parse(fs.readFileSync(editorJsonPath, 'utf8'));
  const merged = mergeValues(srcVals, editor);
  merged.meta = {
    extractedAt: new Date().toISOString(),
    srcHash: crypto.createHash('sha1').update(JSON.stringify(srcVals)).digest('hex').slice(0, 12),
    editorHash: crypto.createHash('sha1').update(JSON.stringify(editor)).digest('hex').slice(0, 12),
  };
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  fs.writeFileSync(outPath, JSON.stringify(merged, null, 1));
  return merged;
}
module.exports = { mergeValues, writeValuesJson };
