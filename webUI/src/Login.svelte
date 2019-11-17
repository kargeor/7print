<script>
  import { onMount } from 'svelte';
  import Button from './Button.svelte';

  let passwordInput;
  onMount(() => setTimeout(() => passwordInput.focus()));

  let loginError = false;
  function doLogin() {
    loginError = false;

    fetch('api/login', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
        'Accept': 'application/json'
      },
      body: 'pass=' + encodeURIComponent(passwordInput.value)
    })
    // TODO: don't reload on success
    .then(r => r.ok ? location.reload() : (loginError = true));
  }
</script>
<style>
  div {
    border: 1px solid black;
    margin: 16px 4px;
    padding: 4px;
  }
  input {
    width: 100%;
    margin: 12px 0px;
  }
  input.loginError {
    animation: shake 0.2s ease-in-out 0s 2;
    box-shadow: 0 0 0.5em red;
  }
  @keyframes shake {
    0% { margin-left: 0rem; }
    25% { margin-left: 0.5rem; }
    75% { margin-left: -0.5rem; }
    100% { margin-left: 0rem; }
  }
</style>

<div>
  Please Enter Your Password
  <input
    bind:this={passwordInput}
    on:keypress={e => e.keyCode === 13 ? doLogin() : null }
    type="password"
    class:loginError
  />
  <Button title="Login" on:click={doLogin} />
</div>
