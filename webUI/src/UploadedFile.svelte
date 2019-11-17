<script>
  import { createEventDispatcher } from 'svelte';
  import { slide } from 'svelte/transition';
  import {fileSize} from './utils';

  export let name = '';
  export let stat = {};

  let open = false;
  const dispatch = createEventDispatcher();

  function doCmd(cmdName) {
    dispatch(cmdName, {name});
    open = false;
  }
</script>
<style>
  button {
    background-color: papayawhip;
    display: block;
    width: 100%;
    padding: 8px 16px;
    margin: 4px 0px;
  }
  button.open {
    font-weight: bold;
  }
  button.sub {
    background-color: aliceblue;
    width: 75%;
    margin-left: 25%;
  }
  .name {
    word-break: break-all;
  }
  .prop {
    word-break: break-all;
    color: gray;
    font-size: 20px;
  }
</style>

<button on:click={() => open = !open} class:open>
  <div class="name">{name}</div>
  <div class="prop">{new Date(stat.st_mtime * 1000).toDateString()}</div>
  <div class="prop">{fileSize(stat.st_size)}</div>
</button>
{#if open}
  <button class='sub' transition:slide on:click={() => doCmd('print')}>Print</button>
  <button class='sub' transition:slide on:click={() => alert('Not implemented yet')}>Info</button>
  <button class='sub' transition:slide on:click={() => alert('Not implemented yet')}>Download</button>
  <button class='sub' transition:slide on:click={() => alert('Not implemented yet')}
    on:introend={e => e.target.scrollIntoView({behavior: "smooth"})}>Delete</button>
{/if}
