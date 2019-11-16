<script>
  import TempDisplay from './TempDisplay.svelte';
  import ValueDisplay from './ValueDisplay.svelte';
  import Button from './Button.svelte';
  import UploadedFile from './UploadedFile.svelte';
  import {formatTime, calcPercent, u} from './utils';

  let serverFiles = [];
  fetch('api-7print/listFiles').then(r => r.json()).then(json => (serverFiles = json));
  // TODO: sort serverFiles by time

  let serverState = {};
  const ws = new WebSocket(`${location.protocol.replace('http', 'ws')}//${location.host}/socket`, 'binary');
  ws.binaryType = 'arraybuffer';
  //
  const messageSize = 108;
  const FIELDS = [
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
  function decodeMessage(dv) {
    const result = {};
    let offset = 0;
    FIELDS.forEach(f => {
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
  //
  const SERVER_STATES = [
    '',
    'NO SERIAL',
    'READY',
    'BUSY',
    'PRINTING',
    'PAUSED',
    'ERROR',
  ];
  //
  ws.onmessage = ({data}) => {
    if (data.byteLength !== messageSize) {
      console.error('Data size is wrong');
    } else {
      const dv = new DataView(data);
      serverState = decodeMessage(dv);
    }
  };
</script>

<style>
  h1 {
    color: blue;
    text-align: left;
    font-size: 26px;
  }
  @media (min-aspect-ratio: 8/7) {
    section {
      width: 400px;
      margin: auto;
    }
  }
</style>

<section>
  <h1>7print (alpha version)</h1>
  <ValueDisplay name="Status" value={u(SERVER_STATES[serverState['state']])} />
{#if serverState['state'] === 4}
  <ValueDisplay name="Progress" value={calcPercent(serverState)} />
  <ValueDisplay name="Print Time" value={formatTime(serverState['timeSpent'])} />
  <ValueDisplay name="Remaining Time" value={formatTime(serverState['timeRemain'])} />
  <ValueDisplay name="Z Position" value={`${u(serverState['zposSent'] / 100)}mm`} />
{/if}

  <TempDisplay name="Hotend" valueCurrent={u(serverState['extrCurrent'])} valueSet={u(serverState['extrTarget'])} />
  <TempDisplay name="Bed" valueCurrent={u(serverState['bedCurrent'])} valueSet={u(serverState['bedTarget'])} />

  <Button title="Temp Graph" on:click={() => alert('Not implemented yet')} />
  <Button title="Stop Print" />

  <h1>Control</h1>
  <Button title="Change Temp" on:click={() => alert('Not implemented yet')} />
  <Button title="Move" on:click={() => alert('Not implemented yet')} />
</section>

<section>
  <h1>Files</h1>
{#each serverFiles as { name, stat }, i}
  <UploadedFile {name} {stat} />
{/each}
{#if serverFiles.length === 0}
  Loading...
{/if}

  <Button title="Upload" on:click={() => ws.send('7PRN\x01\x00XX\x01\x00\x00\x00openjscad_1_0.2mm_PLA_MK3S_19m.gcode\x001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901')} />
</section>
