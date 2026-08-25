const { test } = require('node:test');
const assert = require('node:assert');
const { Ledger, LedgerError } = require('../sim/ledger.js');

test('입금/출금/잔액/소스별 집계', () => {
  const L = new Ledger();
  L.post(0, 'Money', 10000, 'S1');
  L.post(0, 'Money', -2500, 'K4');
  assert.strictEqual(L.balance('Money'), 7500);
  assert.strictEqual(L.totalBySrc('Money', 'K4'), -2500);
});
test('음수 잔액 = LedgerError', () => {
  const L = new Ledger();
  assert.throws(() => L.post(0, 'Money', -1, 'K1'), LedgerError);
});
test('assertConsistent: 트랜잭션 합 == 잔액', () => {
  const L = new Ledger();
  L.post(0, 'Money', 500, 'S4'); L.post(1, 'Money', -100, 'K3');
  L.assertConsistent(); // throw 없으면 통과
});
