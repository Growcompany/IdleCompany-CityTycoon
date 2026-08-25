class LedgerError extends Error {}

class Ledger {
  constructor() {
    this.bal = new Map(); // currency → balance
    this.tx = [];          // all transactions
    this.agg = new Map();  // "currency|srcId" → total
    // 일별 유입/유출 — 인플레 판정(순수입 대 유효싱크 비율)이 시계열을 요구하는데 tx 전체를
    // RunResult 에 실으면 런당 수천 건이라, 하루×재화 단위로만 접어 둔다(verify/scan.js).
    this.days = new Map(); // "day|currency" → {in, out}
  }

  post(day, currency, amount, srcId) {
    const cur = this.bal.get(currency) ?? 0;
    if (cur + amount < 0) {
      throw new LedgerError(`${currency} 잔액 부족: ${cur} + ${amount} (src=${srcId}, day=${day})`);
    }
    if (!Number.isFinite(amount)) {
      throw new LedgerError(`비정상 금액 ${amount} (src=${srcId})`);
    }
    this.bal.set(currency, cur + amount);
    this.tx.push({ day, currency, amount, srcId });
    const k = `${currency}|${srcId}`;
    this.agg.set(k, (this.agg.get(k) ?? 0) + amount);
    const dk = `${day}|${currency}`;
    const d = this.days.get(dk) ?? { in: 0, out: 0 };
    if (amount >= 0) d.in += amount; else d.out += -amount;
    this.days.set(dk, d);
  }

  balance(c) {
    return this.bal.get(c) ?? 0;
  }

  totalBySrc(c, s) {
    return this.agg.get(`${c}|${s}`) ?? 0;
  }

  assertConsistent() {
    const sums = new Map();
    for (const t of this.tx) {
      sums.set(t.currency, (sums.get(t.currency) ?? 0) + t.amount);
    }
    for (const [c, v] of sums) {
      if (Math.abs(v - this.balance(c)) > 1e-6) {
        throw new LedgerError(`원장-잔액 불일치 ${c}: tx합 ${v} vs 잔액 ${this.balance(c)}`);
      }
    }
  }

  toJSON() {
    return {
      balances: Object.fromEntries(this.bal),
      bySrc: Object.fromEntries(this.agg),
      byDay: Object.fromEntries(this.days),
      txCount: this.tx.length
    };
  }
}

module.exports = { Ledger, LedgerError };
