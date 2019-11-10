<script>
  export let name;
  export let value;
</script>

<style>
  div {
    display: flex;
    justify-content: space-between;
  }
  .name {
    color: red;
  }
  .value {
    font-family: Courier, monospace;
  }
</style>

<div>
  <span class="name">{name}</span>
  <span class="value">{value}</span>
</div>
