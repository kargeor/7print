<script>
  import TempDisplay from './TempDisplay.svelte';
  import ValueDisplay from './ValueDisplay.svelte';
  import Button from './Button.svelte';
  import UploadedFile from './UploadedFile.svelte';
</script>

<style>
  h1 {
    color: blue;
    text-align: left;
    font-size: 26px;
  }
  @media (min-aspect-ratio: 8/7) {
    section {
      width: 46%;
      float: left;
      margin: 0% 2%;
    }
  }
</style>

<section>
  <h1>7print</h1>
  <ValueDisplay name="Status" value="Printing" />
  <ValueDisplay name="Progress" value="50%" />
  <ValueDisplay name="Print Time" value="0:00:00" />
  <ValueDisplay name="Remaining Time" value="0:00:00" />
  <ValueDisplay name="Z Position" value="12.30mm" />

  <TempDisplay name="Hotend" valueCurrent=20 valueSet=200 />
  <TempDisplay name="Bed" valueCurrent=20 valueSet=60 />

  <Button title="Temp Graph" />
  <Button title="Stop Print" />

  <h1>Control</h1>
  <Button title="Change Temp" />
  <Button title="Move" />
</section>

<section>
  <h1>Files</h1>
  <UploadedFile />
  <UploadedFile />
  <UploadedFile />
  <UploadedFile />
  <Button title="Upload" />
</section>
