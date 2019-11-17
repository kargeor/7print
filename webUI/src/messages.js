const IN_FIELDS = [
  ['magic0',       8],
  ['magic1',       8],
  ['magic2',       8],
  ['magic3',       8],
  ['version',     16],
  ['reserved',    16],
  ['bedTarget',   16],
  ['bedCurrent',  16],
  ['extrTarget',  16],
  ['extrCurrent', 16],
  ['currentFile', 's', 64],
  ['bytesSent',   32],
  ['bytesRemain', 32],
  ['zposSent',    32],
  ['zposRemain',  32],
  ['timeSpent',   32],
  ['timeRemain',  32],
  ['state',       32],
];

const OUT_FIELDS = [
  ['magic0',       8],
  ['magic1',       8],
  ['magic2',       8],
  ['magic3',       8],
  ['version',     16],
  ['reserved',    16],
  ['commandId',   32],
  ['file0', 's',  64],
  ['file1', 's',  64],
];

export function decodeMessage(dv) {
  const result = {};
  let offset = 0;
  IN_FIELDS.forEach(f => {
    if (f[1] === 8) {
      result[f[0]] = dv.getUint8(offset, true);
      offset += 1;
    } else if (f[1] === 16) {
      result[f[0]] = dv.getUint16(offset, true);
      offset += 2;
    } else if (f[1] === 32) {
      result[f[0]] = dv.getUint32(offset, true);
      offset += 4;
    } else if (f[1] === 's') {
      // TODO
      offset += f[2];
    } else {
      console.log("Cannot decode " + f);
    }
  });
  // console.log({decodedMessage: result});
  return result;
}

export function encodeMessage(d) {
  const result = new ArrayBuffer(140);
  const dv = new DataView(result);

  let offset = 0;
  OUT_FIELDS.forEach(f => {
    if (f[1] === 8) {
      dv.setUint8(offset, d[f[0]], true);
      offset += 1;
    } else if (f[1] === 16) {
      dv.setUint16(offset, d[f[0]], true);
      offset += 2;
    } else if (f[1] === 32) {
      dv.setUint32(offset, d[f[0]], true);
      offset += 4;
    } else if (f[1] === 's') {
      const s = d[f[0]] || '';
      for (let i = 0; i < f[2]; i++) {
        // charCodeAt returns NaN for out of bounds
        // this will fill the rest with 0
        // creating a null-terminated C-string
        dv.setUint8(offset, s.charCodeAt(i) || 0);
        offset++;
      }
    } else {
      console.log("Cannot encode " + f);
    }
  });

  return result;
}
