const { test } = require('node:test');
const assert = require('node:assert');
const { makeRng } = require('../sim/rng.js');

test('같은 시드 = 같은 수열 (재현성)', () => {
  const a = makeRng(42), b = makeRng(42);
  for (let i = 0; i < 100; i++) assert.strictEqual(a.next(), b.next());
});
test('fork는 부모와 독립, label별 결정적', () => {
  const r1 = makeRng(7).fork('star'), r2 = makeRng(7).fork('star'), r3 = makeRng(7).fork('gacha');
  assert.strictEqual(r1.next(), r2.next());
  assert.notStrictEqual(makeRng(7).fork('star').next(), r3.next());
});
test('int는 양끝 포함 균등', () => {
  const r = makeRng(1); const seen = new Set();
  for (let i = 0; i < 1000; i++) seen.add(r.int(1, 3));
  assert.deepStrictEqual([...seen].sort(), [1, 2, 3]);
});
