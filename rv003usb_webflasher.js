/*
This WebFlasher is adapted from minichlink <https://github.com/cnlohr/ch32fun/tree/master/minichlink>
The adaptation was made by Sadale.

MIT License
Copyright (c) 2026 Wong Cho Ching <https://sadale.net>
Copyright (c) 2023-2024 CNLohr <lohr85@gmail.com>, et. al.
Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
Copyright (c) 2023-2024 E. Brombaugh
Copyright (c) 2023-2024 A. Mandera
Copyright (c) 2005-2020 Rich Felker, et al.
Copyright (c) 2013,2014 Michal Ludvig <michal@logix.cz>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
async function rv003usb_webflasher(uint8arraycontent, status_callback) {
	const USB_VID = 0x1209
	const USB_PID = 0xB003
	const FLASH_BASE = 0x08000000
	const FLASH_SIZE = 16384 // Use 63488 for CH32V006 support
	const SECTOR_SIZE = 64

	function dataview_to_uint8array(dataview) {
		let ret = new Uint8Array(dataview.byteLength);
		for(let i=0; i<dataview.byteLength; i++){
			ret[i] = dataview.getUint8(i)
		}
		return ret
	}

	function payload_obtain_uint8array(content) {
		const payload_size = 128;
		const blob_prefix = [0xaa, 0x00, 0x00, 0x00];
		const blob_suffix = [0xcd, 0xab, 0x34, 0x12];
		if(blob_prefix.length + blob_suffix.length + content.length > payload_size) {
			throw new Error("Payload content too long!");
		}

		let ret = new Uint8Array(payload_size);
		for(let i=0; i<blob_prefix.length; i++) {
			ret[i] = blob_prefix[i];
		}
		for(let i=0; i<blob_suffix.length; i++) {
			ret[ret.length-blob_suffix.length+i] = blob_suffix[i];
		}
		for(let i=0; i<content.length; i++) {
			ret[blob_prefix.length+i] = content[i]
		}
		return ret;
	}

	function uint32value_to_array(uint32value) {
		if(uint32value < 0 || uint32value > 2**32-1) {
			throw new Error("uint32value out of range");
		}
		ret = []
		// Convert to little endian
		for(let i=0; i<4; i++) {
			ret.push(uint32value & 0xFF)
			uint32value /= 256
		}
		return ret
	}

	function bulid_halt_wait_payload() {
		return payload_obtain_uint8array([0x81, 0x46, 0x94, 0xc1, 0xfd, 0x56, 0x14, 0xc1, 0x82, 0x80])
	}

	function bulid_read_payload(address, size) {
		// blob_word_read, size and address must be aligned by 4.
		let payload = [
			0x23, 0xa0, 0x05, 0x00, 0x13, 0x07, 0x45, 0x03, 0x0c, 0x43, 0x50, 0x43,
			0x2e, 0x96, 0x21, 0x07, 0x94, 0x41, 0x14, 0xc3, 0x91, 0x05, 0x11, 0x07,
			0xe3, 0xcc, 0xc5, 0xfe, 0x93, 0x06, 0xf0, 0xff, 0x14, 0xc1, 0x82, 0x80,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
		payload = payload.concat(uint32value_to_array(address))
		payload = payload.concat(uint32value_to_array(size))
		return payload_obtain_uint8array(payload)
	}

	function bulid_write_payload(address, size, content) {
		// blob_word_write, size and address must be aligned by 4.
		let payload = [
			0x23, 0xa0, 0x05, 0x00, 0x13, 0x07, 0x45, 0x03, 0x0c, 0x43, 0x50, 0x43,
			0x2e, 0x96, 0x21, 0x07, 0x14, 0x43, 0x94, 0xc1, 0x91, 0x05, 0x11, 0x07,
			0xe3, 0xcc, 0xc5, 0xfe, 0x93, 0x06, 0xf0, 0xff, 0x14, 0xc1, 0x82, 0x80, // NOTE: No readback!
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
		payload = payload.concat(uint32value_to_array(address))
		payload = payload.concat(uint32value_to_array(size))
		payload = payload.concat(content)
		return payload_obtain_uint8array(payload)
	}

	function bulid_write64_flash_payload(address, content) {
		// blob_write64_flash, size and address must be aligned by 4.
		let payload = [
			0x13, 0x07, 0x45, 0x03, 0x0c, 0x43, 0x13, 0x86, 0x05, 0x04, 0x5c, 0x43,
			0x8c, 0xc7, 0x14, 0x47, 0x94, 0xc1, 0xb7, 0x06, 0x05, 0x00, 0xd4, 0xc3,
			0x94, 0x41, 0x91, 0x05, 0x11, 0x07, 0xe3, 0xc8, 0xc5, 0xfe, 0xc1, 0x66,
			0x93, 0x86, 0x06, 0x04, 0xd4, 0xc3, 0xfd, 0x56, 0x14, 0xc1, 0x82, 0x80]
		payload = payload.concat(uint32value_to_array(address))
		payload = payload.concat(uint32value_to_array(0x4002200C)) // FLASH->STATR
		payload = payload.concat(...content)
		return payload_obtain_uint8array(payload)
	}

	function bulid_run_app_payload() {
		// blob_run_app
		let payload = [
			0xb7,0xf5,0xff,0x1f,  // li     a1,0x1FFFF000   - load offset to a1
			0x93,0x87,0xc5,0x77,  // addi   a5,a1,0x77C     - load absolute address of secret area to a5
			0x03,0xa7,0x07,0x00,  // lw     a4,0(a5)        - load reboot function offset + xor from secret to a4
			0x13,0x57,0x07,0x01,  // srli   a4,a4,16        - shift it to remove lower part (offset)
			0x83,0x96,0x07,0x00,  // lh     a3,0(a5)        - load offset part to a3
			0x93,0xc7,0xc6,0x77,  // xori   a5,a3,0x77C     - find current xor
			0x63,0x16,0xf7,0x00,  // bne    a4,a5,.L2       - if xor is valid
			0x33,0x87,0xb6,0x00,  // add    a4, a3, a1      - make absolute address of reboot function an jump
			0x67,0x00,0x07,0x00,  // jr     a4              - jump to it
			/* else - means that we didn't find a reboot function address
			and need to send the blob to do a reboot
		.L2:                                                - Same sequence as in "Run app blob (old)"*/
			0xb7,0x27,0x02,0x40,  // li     a5,1073881088
			0x93,0x87,0x87,0x02,  // addi   a5,a5,40
			0x37,0x07,0x67,0x45,  // li     a4,1164378112
			0x13,0x07,0x37,0x12,  // addi   a4,a4,291
			0x23,0xa0,0xe7,0x00,  // sw     a4,0(a5)
			0xb7,0x27,0x02,0x40,  // li     a5,1073881088
			0x93,0x87,0x87,0x02,  // addi   a5,a5,40
			0x37,0x97,0xef,0xcd,  // li     a4,-839938048
			0x13,0x07,0xb7,0x9a,  // addi   a4,a4,-1621
			0x23,0xa0,0xe7,0x00,  // sw     a4,0(a5)
			0xb7,0x27,0x02,0x40,  // li     a5,1073881088
			0x93,0x87,0xc7,0x00,  // addi   a5,a5,12
			0x23,0xa0,0x07,0x00,  // sw     zero,0(a5)
			0xb7,0x27,0x02,0x40,  // li     a5,1073881088
			0x93,0x87,0x07,0x01,  // addi   a5,a5,16
			0x13,0x07,0x00,0x08,  // li     a4,128
			0x23,0xa0,0xe7,0x00,  // sw     a4,0(a5)
			0xb7,0xf7,0x00,0xe0,  // li     a5,-536809472
			0x93,0x87,0x07,0xd1,  // addi   a5,a5,-752
			0x37,0x07,0x00,0x80,  // li     a4,-2147483648
			0x23,0xa0,0xe7,0x00,  // sw     a4,0(a5)
		]
		return payload_obtain_uint8array(payload)
	}

	async function communicate_usb(device, command, readback=true) {
		let retries = 0
		while(1) {
			try {
				await device.sendFeatureReport(command[0], command.slice(1));
				break
			} catch (error) {
				if(retries++ > 10) {
					console.error("sendFeatureReport retries exceeded!")
					return null
				}
			}
		}

		if(!readback) {
			return 0
		}

		retries = 0
		let timeout = 0
		let response = null
		while(1) {
			try {
				let dataview = await device.receiveFeatureReport(command[0])
				response = dataview_to_uint8array(dataview)
				if(response.byteLength == command.byteLength && response[1] == 0xff) {
					break;
				} else if(timeout++ > 20) {
					console.error("receiveFeatureReport timeout!")
					return null
				}
			} catch (error) {
				if(retries++ > 10) {
					console.error("receiveFeatureReport retries exceeded!")
					return null
				}
			}
		}
		return response;
	}

	async function communicate_halt_wait(device) {
		return await communicate_usb(device, bulid_halt_wait_payload())
	}

	async function communicate_read_word(device, address) {
		let result = await communicate_usb(device, bulid_read_payload(address, 4))
		if(result === null || result.length < 64) {
			return null
		}
		let ret = 0
		for(let i=0; i<4; i++) {
			ret += result[60+i] * (2**(i*8))
		}
		return ret
	}

	async function communicate_write_word(device, address, data) {
		let result = await communicate_usb(device, bulid_write_payload(address, 4, uint32value_to_array(data)))
		if(result === null || result.length < 64) {
			return null
		}
		let ret = 0
		for(let i=0; i<4; i++) {
			ret += result[60+i] * (2**(i*8))
		}
		return ret
	}

	async function communicate_verify64(device, address, expected_data) {
		let result = await communicate_usb(device, bulid_read_payload(address, 64))
		if(result === null || result.length < 60+64) {
			return null
		}
		for(let i=0; i<64; i++) {
			if(expected_data[i] != result[60+i]) {
				return false
			}
		}
		return true
	}

	async function communicate_flash64(device, address, data) {
		// Erase page
		if(await communicate_write_word(device, 0x40022010, 0x00020000) === null) { return null; }
		if(await communicate_write_word(device, 0x40022014, address) === null) { return null; }
		if(await communicate_write_word(device, 0x40022010, 0x00020040) === null) { return null; }
		// Wait for completion of page erase
		let result = 0x03
		let timeout = 0
		do {
			result = await communicate_read_word(device, 0x4002200C)
			if(result === null) {
				console.error("Flash wait communication error!")
				return null;
			} else if(timeout++ > 1000) {
				console.error("Warning: Flash erase timed out. STATR = " + result)
				return null;
			}
		} while(result & 0x03);

		if(result & 0x00000010) {
			console.error("Memory Protection Error")
			return null;
		}
		
		// FLASH->CTLR = CR_PAGE_PG
		if(await communicate_write_word(device, 0x40022010, 0x00010000) === null) {
			console.error("FLASH->CTLR = CR_PAGE_PG Error")
			return null;
		}
		// FLASH->CTLR = CR_PAGE_PG | CR_BUF_RST
		if(await communicate_write_word(device, 0x40022010, 0x00090000) === null) {
			console.error("FLASH->CTLR = CR_PAGE_PG | CR_BUF_RST Error")
			return null;
		}

		if(await communicate_usb(device, bulid_write64_flash_payload(address, data)) === null) {
			console.error("flash write64 error")
			return null;
		}
		
		return 0;
	}

	async function communicate_run_app(device) {
		return await communicate_usb(device, bulid_run_app_payload(), false)
	}

	async function communicate_flash_unlock(device) {
		let rw = await communicate_read_word(device, 0x40022010)
		if (rw === null) {
			console.error("Flash unlock status read error A");
			return null;
		}

		if(rw & 0x8080) {
			// FLASH->KEYR = 0x40022004
			if(await communicate_write_word(device, 0x40022004, 0x45670123) === null) { return null; }
			if(await communicate_write_word(device, 0x40022004, 0xCDEF89AB) === null) { return null; }

			// OBKEYR = 0x40022008  // For user word unlocking
			if(await communicate_write_word(device, 0x40022008, 0x45670123) === null) { return null; }
			if(await communicate_write_word(device, 0x40022008, 0xCDEF89AB) === null) { return null; }

			// MODEKEYR = 0x40022024
			if(await communicate_write_word(device, 0x40022024, 0x45670123) === null) { return null; }
			if(await communicate_write_word(device, 0x40022024, 0xCDEF89AB) === null) { return null; }

			let rw = await communicate_read_word(device, 0x40022010)
			if (rw === null || rw & 0x8080) {
				console.error("Flash unlock failure " + rw);
				return null;
			}
		}
		
		rw = await communicate_read_word(device, 0x4002201c)
		if(rw === null) {
			return null
		}
		if(rw & 2)
		{
			console.error("WARNING: Your part appears to have flash [read] locked.  Cannot program unless unlocked.")
			return null
		}
		return 0
	}

	function pad_rom_to_64(content) {
		let ret = new Uint8Array(Math.floor((content.byteLength+63)/64)*64);
		ret.fill(0xFF)
		ret.set(content, 0)
		return ret
	}

	let image_content = pad_rom_to_64(uint8arraycontent)
	status_callback({step: 0, offset:0, size:image_content.byteLength})
	if(image_content.byteLength > FLASH_SIZE) {
		console.error("ROM size too large!")
		return null
	}

	status_callback({step: 1, offset:0, size:image_content.byteLength})
	let device = null
	try {
		device = await navigator.hid.requestDevice({ filters: [{ vendorId: USB_VID, productId: USB_PID }] })
		device = device[0]
		await device.open()
	} catch (error) {
		console.error("device.open() failed")
		return null
	}

	status_callback({step: 2, offset:0, size:image_content.byteLength})
	if(await communicate_halt_wait(device) === null) {
		console.error("communicate_halt_wait() failed")
		return null
	}

	status_callback({step: 3, offset:0, size:image_content.byteLength})
	if(await communicate_flash_unlock(device) === null) {
		console.error("communicate_flash_unlock() failed")
		return null
	}

	let difference_found = true
	let retries = 0
	while(difference_found && retries++ < 5) {
		difference_found = false
		for(let i=0; i<image_content.byteLength; i+=SECTOR_SIZE) {
			let address = FLASH_BASE+i
			let image_chunk = image_content.slice(i, i+SECTOR_SIZE)
			status_callback({step: difference_found?4:5, offset:i, size:image_content.byteLength})
			if(!(await communicate_verify64(device, address, image_chunk))) {
				if(!difference_found) {
					difference_found = true
				}
				if(await communicate_flash64(device, address, image_chunk) === null) {
					console.error("Unable to write flash at offset " + (FLASH_BASE+i).toString(16))
					return null;
				}
			}
		}
	}

	if(difference_found) {
		console.error("Unable to write flash with correct content after multiple retries")
		return null;
	}

	status_callback({step: 6, offset:image_content.byteLength, size:image_content.byteLength})
	if(await communicate_run_app(device) === null) {
		console.error("communicate_run_app() failed")
		return null
	}
	status_callback({step: 7, offset:image_content.byteLength, size:image_content.byteLength})
	return true
}
