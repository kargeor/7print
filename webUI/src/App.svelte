<script>
  import TempDisplay from './TempDisplay.svelte';
  import ValueDisplay from './ValueDisplay.svelte';
  import Button from './Button.svelte';
  import Login from './Login.svelte';
  import UploadedFile from './UploadedFile.svelte';
  import {formatTime, calcPercent, u} from './utils';
  import {encodeMessage, decodeMessage} from './messages';

  let serverFiles = [];
  let loginNeeded = false;
  fetch('api/listFiles')
    .then(r => r.json())
    .then(json => (serverFiles = json))
    .catch(e => (loginNeeded = true));
  // TODO: catch error cases where login is not needed
  // TODO: sort serverFiles by time

  let serverState = {};
  let ws = null;
  const messageSize = 108;
  const apiKeyRe = /api_key=([0-9a-z]+)/.exec(document.cookie);
  if (apiKeyRe && apiKeyRe[1]) {
    ws = new WebSocket(`${location.protocol.replace('http', 'ws')}//${location.host}/socket`, 'binary');
    ws.binaryType = 'arraybuffer';

    ws.onmessage = ({data}) => {
      if (data.byteLength !== messageSize) {
        console.error('Data size is wrong');
      } else {
        const dv = new DataView(data);
        serverState = decodeMessage(dv);
      }
    };

    ws.onopen = () => {
      ws.send(apiKeyRe[1]);
    };
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
  function sendCommand(params) {
    ws.send(encodeMessage(Object.assign({
      magic0: 55, // 7
      magic1: 80, // P
      magic2: 82, // R
      magic3: 78, // N
      version: 1,
      reserved: 0,
    }, params)));
  }
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
{#if loginNeeded}
  <Login />
{/if}
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
  <Button title="Stop Print" on:click={() => alert('Not implemented yet')} />

  <h1>Control</h1>
  <Button title="Change Temp" on:click={() => alert('Not implemented yet')} />
  <Button title="Move" on:click={() => alert('Not implemented yet')} />
</section>

<section>
  <h1>Files</h1>
{#each serverFiles as { name, stat }, i}
  <UploadedFile {name} {stat} on:print={e => sendCommand({commandId: 1, file0: e.detail.name})} />
{/each}
{#if serverFiles.length === 0}
  Loading...
{/if}

  <Button title="Upload" on:click={() => sendCommand({commandId: 1, file0: 'Knob_0.2mm_PLA_MK3S_1h22m.gcode'})} />
</section>
