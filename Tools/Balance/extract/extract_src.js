const fs = require('node:fs');
const path = require('node:path');
class ExtractError extends Error {}

function extractFromText(entry, text) {
  if (entry.mode === 'regex') {
    const m = text.match(new RegExp(entry.pattern));
    if (!m) throw new ExtractError(`[${entry.key}] 패턴 미매칭: ${entry.pattern}`);
    if (entry.type === 'product') {
      // 두 캡처그룹(m[1]*m[2])의 곱 — 소스가 "12.0f * 3600.0f" 처럼 상수식인 경우용 (임의 산술 eval 금지, 곱셈 1건만 지원)
      if (m[2] === undefined) throw new ExtractError(`[${entry.key}] type:product 는 캡처그룹 2개 필요`);
      return { v: parseFloat(m[1]) * parseFloat(m[2]), src: 'src', loc: entry.file ?? '(inline)' };
    }
    if (entry.type === 'max-int32-divisor') {
      const divisor = parseInt(m[1], 10);
      if (divisor <= 0) throw new ExtractError(`[${entry.key}] MAX_int32 제수는 양수여야 함`);
      return { v: Math.floor((2 ** 31 - 1) / divisor), src: 'src', loc: entry.file ?? '(inline)' };
    }
    const v = entry.type === 'int' ? parseInt(m[1], 10) : parseFloat(m[1]);
    return { v, src: 'src', loc: entry.file ?? '(inline)' };
  }
  if (entry.mode === 'anchored-window') {
    const idx = text.indexOf(entry.anchor);
    if (idx < 0) throw new ExtractError(`[${entry.key}] 앵커 미발견: ${entry.anchor}`);
    const win = text.slice(idx).split('\n').slice(0, 1 + entry.windowLines).join('\n');
    if (entry.intCount !== undefined) {
      // 부동소수점(1.5f 등)의 정수부/소수부를 오매칭하지 않도록 앞뒤에 숫자·점이 없는 정수만 매칭
      const ints = [...win.matchAll(/(?<![0-9.])-?[0-9]+(?![0-9.])/g)].map(m => parseInt(m[0], 10));
      if (ints.length !== entry.intCount)
        throw new ExtractError(`[${entry.key}] int ${entry.intCount}개 기대, ${ints.length}개 발견 — 코드 변경 의심`);
      return { v: ints, src: 'src', loc: entry.file ?? '(inline)' };
    }
    const floats = [...win.matchAll(/(-?[0-9]+\.[0-9]+)f?/g)].map(m => parseFloat(m[1]));
    if (floats.length !== entry.floatCount)
      throw new ExtractError(`[${entry.key}] float ${entry.floatCount}개 기대, ${floats.length}개 발견 — 코드 변경 의심`);
    return { v: floats, src: 'src', loc: entry.file ?? '(inline)' };
  }
  throw new ExtractError(`[${entry.key}] 알 수 없는 mode: ${entry.mode}`);
}

function extractSrc(manifestPath, repoRoot) {
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  const out = {};
  for (const e of manifest.entries) {
    const text = fs.readFileSync(path.join(repoRoot, e.file), 'utf8');
    out[e.key] = extractFromText(e, text);
    out[e.key].loc = e.file;
  }
  return out;
}
module.exports = { extractFromText, extractSrc, ExtractError };
