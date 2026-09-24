const status = document.querySelector('#status');
const installer = document.querySelector('#installer');
if (!window.isSecureContext) {
  status.textContent = 'HTTPS is required for browser flashing. Open this site at its trusted HTTPS address (or serve a local copy at http://localhost).';
} else if (!('serial' in navigator)) {
  status.textContent = 'This browser does not provide Web Serial. Open this page in desktop Chrome or Edge.';
} else {
  try {
    await import('./vendor/esp-web-tools/install-button.js');
    await customElements.whenDefined('esp-web-install-button');
    installer.hidden = false;
    status.textContent = '';
  } catch (error) {
    status.textContent = 'The installer could not load. Refresh the page or use the firmware download below.';
    console.error(error);
  }
}
