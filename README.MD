# rv003usb WebFlasher

WebFlasher for CH32V003 devices with [rv003usb bootloader](https://github.com/cnlohr/rv003usb/tree/master/bootloader) using WebHID API.

It supports Chrome-based browsers. It has read-back verification implemented. Probably also supports CH32V006 but I couldn't test.

I made it for [ilo nena](https://ilonena.sadale.net/) [[Github](https://github.com/SadaleNet/ilo-nena)] and figured out that someone else will find it of use so I'm releasing it.

Live Demo here: [Live Demo of rv003usb WebFlasher](https://sadale.net/80)

# What does it do?

`rv003usb_webflasher.js` provides a `rv003usb_webflasher(uint8arraycontent, status_callback)` function that accepts an Uint8Array with .bin content and a status callback.

Upon `rv003usb_webflasher()` is called, the following procedure would be performed:

1. Perform Initialization
2. Perform scanning for each of the sectors of the provided .bin file:
	* Verify the on-device flash content against the .bin chunk
	* If the sector has different content from the .bin file, perform flashing of that sector
	* After finished scanning/flashing all sectors, if any flashing had occurred, rescan the entire flash and reflash if mismatch is found again. Scanning retry limit: 5 times.
3. Exit the bootloader and launch the freshly-flashed app

# Usage

See `example.html`

# The folder `minichlink-minimal`

The folder `minichlink-minimal` contains the code that I manually extracted from the original minichlink.

After that, I manually ported the minimal.c to Javascript.

No AI were involved in this entire project.

# License

MIT License for anything outside of the `minichlink-minimal` folder.

For the content inside the `minichlink-minimal` folder, it contains GPL code and the minimal.c is adapted from the [original minichlink](https://github.com/cnlohr/ch32fun/tree/master/minichlink).

I personally do no care on what you're gonna use the files there for so if you wanna use that, please just ask the author of the source projects (libusb, hidapi, minichlink)

To be honest, I find the release of the original minichlink under MIT license questionable due to its reliance of GPL code. But I think it's ok to release the Javascript version under MIT license because it does not depend on the GPL code whatsoever.
