

export function fileSize(sz) {
  const units = ['', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];
  let i = 0;
  while (sz > 1000) {
    sz = sz / 1000;
    i++;
  }

  // TODO: don't return 123.00B
  return `${sz.toFixed(2)}${units[i]}B`;
}

export function addZeros(n, desiredLen = 2) {
  let r = "" + n;
  while (r.length < desiredLen) {
    r = "0" + r;
  }
  return r;
}

export function formatTime(t, undefinedValue = '') {
  if (t === undefined) return undefinedValue;

  t = Math.round(t);
  const h = Math.floor(t / 3600);
  t = t % 3600;
  const m = Math.floor(t / 60);
  const s = t % 60;

  return `${addZeros(h, 3)}:${addZeros(m)}:${addZeros(s)}`;
}

export function u(v) {
  if (v === undefined || v === null) return '';
  return v;
}

export function calcPercent(s) {
  if (s['timeSpent'] && s['timeRemain']) {
    // Both need to be non-zero
    const t = s['timeSpent'] + s['timeRemain'];
    const p = Math.round(100 * s['timeSpent'] / t);
    return `${p}%`;
  }

  return '0%';
}
