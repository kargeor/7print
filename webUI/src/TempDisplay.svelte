<script>
  export let name;
  export let valueCurrent;
  export let valueSet;
</script>

<style>
  div {
    display: flex;
    justify-content: space-between;
  }
  .name {
    color: red;
  }
</style>

<div>
  <span class="name">{name}</span>
  <span>
    <span class="value">{valueCurrent}&deg;C</span>
    /
    <span class="value">{valueSet}&deg;C</span>
  </span>
</div>
