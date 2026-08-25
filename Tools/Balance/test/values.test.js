const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { loadValues, MissingValueError } = require('../sim/values.js');

function fixture() {
  const p = path.join(os.tmpdir(), `values_fixture_${process.pid}.json`);
  fs.writeFileSync(p, JSON.stringify({
    meta: { extractedAt: 'T', srcHash: 'abc' },
    values: { 'emp.outputUnit': { v: 5.2, src: 'src', loc: 'EmployeeTypes.cpp' } },
    dt: { DT_HQLevel: [{ Name: 'Lv2', MoneyCost: 1000 }] },
  }));
  return p;
}
test('get: 존재 키 반환, 누락 키 throw', () => {
  const V = loadValues(fixture());
  assert.strictEqual(V.get('emp.outputUnit'), 5.2);
  assert.throws(() => V.get('없는.키'), MissingValueError);
});
test('dt: 테이블 행 배열, 누락 테이블 throw', () => {
  const V = loadValues(fixture());
  assert.strictEqual(V.dt('DT_HQLevel')[0].MoneyCost, 1000);
  assert.throws(() => V.dt('DT_없음'), MissingValueError);
});
