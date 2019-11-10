<script>
  export let name = '';
  export let stat = {};

  function fileSize(sz) {
    const units = ['', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];
    let i = 0;
    while (sz > 1000) {
      sz = sz / 1000;
      i++;
    }

    return `${sz.toFixed(2)}${units[i]}B`;
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
  .name {
    word-break: break-all;
  }
  .prop {
    word-break: break-all;
    color: gray;
    font-size: 20px;
  }
</style>

<button>
  <div class="name">{name}</div>
  <div class="prop">{new Date(stat.st_mtime * 1000).toDateString()}</div>
  <div class="prop">{fileSize(stat.st_size)}</div>
</button>

