const fs = require('node:fs');
class MissingValueError extends Error {}
function loadValues(path) {
  const raw = JSON.parse(fs.readFileSync(path, 'utf8'));
  return {
    hash: `${raw.meta?.srcHash ?? '?'}:${raw.meta?.editorHash ?? '?'}`,
    meta: raw.meta,
    get(key) {
      const e = raw.values?.[key];
      if (e === undefined) throw new MissingValueError(`values.json 누락 키: ${key}`);
      return e.v;
    },
    dt(name) {
      const t = raw.dt?.[name];
      if (t === undefined) throw new MissingValueError(`values.json 누락 DT: ${name}`);
      return t;
    },
  };
}
module.exports = { loadValues, MissingValueError };
