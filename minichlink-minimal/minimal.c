// Adapted from https://github.com/cnlohr/ch32fun/tree/master/minichlink
// I extracted the relevant functions from the source to get a minimal implementation of a flasher

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "hidapi.h"

#define RAM_BASE (0x20000000)
#define RAM_SIZE (2048)
#define FLASH_BASE (0x08000000)
#define FLASH_SIZE (16384)
#define SECTOR_SIZE (64)

#define HID_BUFFER_SIZE (128)

static uint8_t hid_command[HID_BUFFER_SIZE];
static uint8_t hid_response[HID_BUFFER_SIZE];

static const uint8_t blob_prefix[] = {
	0xaa, 0x00, 0x00, 0x00
};

static const uint8_t blob_suffix[] = {
	0xcd, 0xab, 0x34, 0x12
};

// Prevents autoreboot after countdown
void bulid_halt_wait_payload(uint8_t buf[HID_BUFFER_SIZE]) {
	static const unsigned char blob_halt_wait[] = {
		0x81, 0x46, 0x94, 0xc1, 0xfd, 0x56, 0x14, 0xc1, 0x82, 0x80
	};
	memset(buf, 0, HID_BUFFER_SIZE);
	memcpy(buf, blob_prefix, sizeof(blob_prefix));
	memcpy(&buf[4], blob_halt_wait, sizeof(blob_halt_wait));
	memcpy(&buf[124], blob_suffix, sizeof(blob_suffix));
}

void bulid_read_payload(uint8_t buf[HID_BUFFER_SIZE], uint32_t address, uint32_t size) {
	static const uint8_t blob_word_read[] = { // size and address must be aligned by 4.
		0x23, 0xa0, 0x05, 0x00, 0x13, 0x07, 0x45, 0x03, 0x0c, 0x43, 0x50, 0x43,
		0x2e, 0x96, 0x21, 0x07, 0x94, 0x41, 0x14, 0xc3, 0x91, 0x05, 0x11, 0x07,
		0xe3, 0xcc, 0xc5, 0xfe, 0x93, 0x06, 0xf0, 0xff, 0x14, 0xc1, 0x82, 0x80,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	assert(address%4 == 0 && size%4 == 0);
	memset(buf, 0, HID_BUFFER_SIZE);
	memcpy(buf, blob_prefix, sizeof(blob_prefix));
	memcpy(&buf[4], blob_word_read, sizeof(blob_word_read));
	memcpy(&buf[52], &address, sizeof(address));
	memcpy(&buf[56], &size, sizeof(size));
	memcpy(&buf[124], blob_suffix, sizeof(blob_suffix));
}

void bulid_write_payload(uint8_t buf[HID_BUFFER_SIZE], uint32_t address, uint32_t size, void *content) {
	static const uint8_t blob_word_write[] = { // size and address must be aligned by 4.
		0x23, 0xa0, 0x05, 0x00, 0x13, 0x07, 0x45, 0x03, 0x0c, 0x43, 0x50, 0x43,
		0x2e, 0x96, 0x21, 0x07, 0x14, 0x43, 0x94, 0xc1, 0x91, 0x05, 0x11, 0x07,
		0xe3, 0xcc, 0xc5, 0xfe, 0x93, 0x06, 0xf0, 0xff, 0x14, 0xc1, 0x82, 0x80, // NOTE: No readback!
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	assert(address%4 == 0 && size%4 == 0 && size < 64);
	memset(buf, 0, HID_BUFFER_SIZE);
	memcpy(buf, blob_prefix, sizeof(blob_prefix));
	memcpy(&buf[4], blob_word_write, sizeof(blob_word_write));
	memcpy(&buf[52], &address, sizeof(address));
	memcpy(&buf[56], &size, sizeof(size));
	memcpy(&buf[60], content, size);
	memcpy(&buf[124], blob_suffix, sizeof(blob_suffix));
}

void bulid_write64_flash_payload(uint8_t buf[HID_BUFFER_SIZE], uint32_t address, void *content) {
	assert(address%64 == 0 && address >= FLASH_BASE && address < FLASH_BASE+FLASH_SIZE);\
	static const unsigned char blob_write64_flash[] = { // size and address must be aligned by 4.
		0x13, 0x07, 0x45, 0x03, 0x0c, 0x43, 0x13, 0x86, 0x05, 0x04, 0x5c, 0x43,
		0x8c, 0xc7, 0x14, 0x47, 0x94, 0xc1, 0xb7, 0x06, 0x05, 0x00, 0xd4, 0xc3,
		0x94, 0x41, 0x91, 0x05, 0x11, 0x07, 0xe3, 0xc8, 0xc5, 0xfe, 0xc1, 0x66,
		0x93, 0x86, 0x06, 0x04, 0xd4, 0xc3, 0xfd, 0x56, 0x14, 0xc1, 0x82, 0x80
	};
	memset(buf, 0, HID_BUFFER_SIZE);
	memcpy(buf, blob_prefix, sizeof(blob_prefix));
	memcpy(&buf[4], blob_write64_flash, sizeof(blob_write64_flash));
	memcpy(&buf[52], &address, sizeof(address));
	uint32_t flash_statr = 0x4002200C;
	memcpy(&buf[56], &flash_statr, sizeof(flash_statr));
	memcpy(&buf[60], content, 64);
	memcpy(&buf[124], blob_suffix, sizeof(blob_suffix));
}

void bulid_run_app_payload(uint8_t buf[HID_BUFFER_SIZE]) {
	static const unsigned char blob_run_app[] = {
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
	};

	memset(buf, 0, HID_BUFFER_SIZE);
	memcpy(buf, blob_prefix, sizeof(blob_prefix));
	memcpy(&buf[4], blob_run_app, sizeof(blob_run_app));
	memcpy(&buf[124], blob_suffix, sizeof(blob_suffix));
}


// Returns bytes written. If result is NULL, do not wait for response.
int communicate_usb(hid_device *device, uint8_t *command, size_t command_size, uint8_t *response, size_t response_size) {
	int ret = 0;
	#define RETRY_LIMIT (10)
	int retries = 0;
	while(1) {
		ret = hid_send_feature_report( device, command, command_size );
		if(ret >= 0) {
			break;
		} else if(retries++ > RETRY_LIMIT) {
			fprintf(stderr, "Error: retry limit exceeded. \n");
			return ret;
		}
		usleep(5000);
		fprintf( stderr, "Warning: Issue with hid_send_feature_report. Retrying: %d\n", retries );
	}
	
	if(!response_size) {
		return ret;
	}

	memset(response, 0, response_size);
	int timeout = 0;
	while(1) {
		response[0] = 0xaa;
		ret = hid_get_feature_report( device, response, response_size );
		if(ret < 0) {
			if(retries++ > RETRY_LIMIT) {
				fprintf(stderr, "Error: retry limit exceeded. \n");
				return ret;
			}
		} else if(response[1] == 0xff) {
			break;
		} else if(timeout++ > 20) {
			fprintf(stderr, "Error: Timed out waiting for stub to complete\n");
			return -99;
		}
	}
	return ret;
}

int communicate_halt_wait(hid_device *device) {
	bulid_halt_wait_payload(hid_command);
	int ret = communicate_usb(device, hid_command, HID_BUFFER_SIZE, hid_response, HID_BUFFER_SIZE);
	return (ret != HID_BUFFER_SIZE);
}

int communicate_read_word(hid_device *device, uint32_t address, uint32_t *result) {
	bulid_read_payload(hid_command, address, 4);
	int ret = communicate_usb(device, hid_command, HID_BUFFER_SIZE, hid_response, HID_BUFFER_SIZE);
	*result = *((uint32_t*)&hid_response[60]);
	return (ret != HID_BUFFER_SIZE);
}

int communicate_write_word(hid_device *device, uint32_t address, uint32_t data, uint32_t *result) {
	bulid_write_payload(hid_command, address, 4, &data);
	int ret = communicate_usb(device, hid_command, HID_BUFFER_SIZE, hid_response, HID_BUFFER_SIZE);
	if(result != NULL) {
		*result = *((uint32_t*)&hid_response[60]);
	}
	return (ret != HID_BUFFER_SIZE);
}

int communicate_verify64(hid_device *device, uint32_t address, void *expected) {
	bulid_read_payload(hid_command, address, 64);
	int ret = communicate_usb(device, hid_command, HID_BUFFER_SIZE, hid_response, HID_BUFFER_SIZE);
	if(ret != HID_BUFFER_SIZE) {
		return -1;
	}
	return memcmp(&hid_response[60], expected, 64) == 0 ? 0 : 1;
}

int communicate_flash64(hid_device *device, uint32_t address, void *data) {
	////////////////
	// Erase page //
	////////////////

	// FLASH->CTLR = CR_PAGE_ER
	if(communicate_write_word(device, 0x40022010, 0x00020000, NULL)) {
		return -1;
	}
	// FLASH->ADDR = address
	if(communicate_write_word(device, 0x40022014, address, NULL)) {
		return -2;
	}
	// FLASH->CTLR = CR_PAGE_ER | CR_STRT_Set
	if(communicate_write_word(device, 0x40022010, 0x00020000 | 0x00000040, NULL)) {
		return -3;
	}

	// Wait for completion of erasure
	uint32_t result = 0x03;
	int timeout = 0;
	do {
		result = 0;
		if(communicate_read_word(device, 0x4002200C, &result)) {
			fprintf( stderr, "Flash wait communication error\n" );
			return -4;
		}
		if( timeout++ > 1000 )
		{
			fprintf( stderr, "Warning: Flash erase timed out. STATR = %08x\n", result );
			return -5;
		}
	} while(result & 0x03);

	if(result & 0x00000010) {
		fprintf( stderr, "Memory Protection Error\n" );
		return -6;
	}

	////////////////
	// Flash page //
	////////////////

	// FLASH->CTLR = CR_PAGE_PG
	if(communicate_write_word(device, 0x40022010, 0x00010000, NULL)) {
		return -10;
	}

	// FLASH->CTLR = CR_PAGE_PG | CR_BUF_RST
	if(communicate_write_word(device, 0x40022010, 0x00010000 | 0x00080000, NULL)) {
		return -11;
	}

	bulid_write64_flash_payload(hid_command, address, data);
	int ret = communicate_usb(device, hid_command, HID_BUFFER_SIZE, hid_response, HID_BUFFER_SIZE);
	return (ret != HID_BUFFER_SIZE);
}

int communicate_run_app(hid_device *device) {
	bulid_run_app_payload(hid_command);
	int ret = communicate_usb(device, hid_command, HID_BUFFER_SIZE, NULL, 0);
	return (ret != HID_BUFFER_SIZE);
}

int flash_unlock(hid_device *device) {
	uint32_t rw;
	if(communicate_read_word(device, 0x40022010, &rw)) {
		return -1;
	}

	if(rw & 0x8080) {
		// FLASH->KEYR = 0x40022004
		if(communicate_write_word(device, 0x40022004, 0x45670123, NULL)) { return -2; }
		if(communicate_write_word(device, 0x40022004, 0xCDEF89AB, NULL)) { return -3; }
		// OBKEYR = 0x40022008  // For user word unlocking
		if(communicate_write_word(device, 0x40022008, 0x45670123, NULL)) { return -4; }
		if(communicate_write_word(device, 0x40022008, 0xCDEF89AB, NULL)) { return -5; }
		// MODEKEYR = 0x40022024
		if(communicate_write_word(device, 0x40022024, 0x45670123, NULL)) { return -6; }
		if(communicate_write_word(device, 0x40022024, 0xCDEF89AB, NULL)) { return -7; }

		if(communicate_read_word(device, 0x40022010, &rw)) { return -8; }

		if(rw & 0x8080) {
			fprintf( stderr, "Error: Flash is not unlocked (CTLR = %08x)\n", rw );
			return -1;
		}
	}

	//(FLASH_OBTKEYR)
	if(communicate_read_word(device, 0x4002201c, &rw)) {
		return -9;
	}
	if(rw & 2)
	{
		fprintf( stderr, "WARNING: Your part appears to have flash [read] locked.  Cannot program unless unlocked.\n" );
		return -10;
	}
	return 0;
}

int main(int argc, char **argv) {
	if(argc < 2) {
		fprintf(stderr, "Usage: %s <image.bin>\n", argv[0]);
		return -1;
	}

	FILE *fp = fopen(argv[1], "rb");
	
	if(fp == NULL) {
		fprintf(stderr, "ERROR: Failed opening the file\n");
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	size_t filesize = ftell(fp);
	size_t filesize_roundedup = (filesize+(SECTOR_SIZE-1))/SECTOR_SIZE*SECTOR_SIZE;
	if(filesize_roundedup > FLASH_SIZE) {
		fprintf(stderr, "ERROR: File size too large!\n");
		return -2;
	}
	fseek(fp, 0, SEEK_SET);

	uint8_t *image_content = malloc(filesize_roundedup);
	memset(image_content, 0xFF, filesize_roundedup);
	if(fread(image_content, 1, filesize, fp) != filesize) {
		fprintf(stderr, "ERROR: Failed reading the file\n");
		free(image_content);
		return -3;
	}
	fclose(fp);

	// for(int i=0; i<filesize; i++) {
	// 	printf("%02X ", image_content[i]);
	// 	if(i%16 == 15) {
	// 		printf("\n");
	// 	}
	// }

	hid_device *device = hid_open(0x1209, 0xB003, 0);
	if(!device) {
		fprintf(stderr, "ERROR: Device not found\n");
		return -10;
	}

	memset(hid_command, 0, sizeof(hid_command));
	memset(hid_response, 0, sizeof(hid_response));

	// Prevents autoreboot after timeout
	if(communicate_halt_wait(device)) {
		fprintf(stderr, "ERROR: Failed to halt reboot timeout\n");
		return -11;
	}

	if(flash_unlock(device)) {
		fprintf(stderr, "ERROR: Unable to unlock flash\n");
		return -12;
	}

	int difference_found = 1;
	int retries = 0;
	while(difference_found && retries++ < 5) {
		difference_found = 0;
		for(size_t i=0; i<filesize_roundedup; i+=SECTOR_SIZE) {
			printf("[%08lx] Verifying... ", FLASH_BASE+i);
			if(communicate_verify64(device, FLASH_BASE+i, &image_content[i])) {
				difference_found = 1;
				printf("Flashing... ");
				if(communicate_flash64(device, FLASH_BASE+i, &image_content[i])) {
					fprintf(stderr, "ERROR: Unable to write flash at offset %08lx\n", FLASH_BASE+i);
					return -13;
				}
			}
			printf("\n");
		}
	}
	
	if(difference_found) {
		fprintf(stderr, "ERROR: Unable to write flash with correct content after multiple retries\n");
		return -14;
	}

	if(communicate_run_app(device)) {
		fprintf(stderr, "ERROR: failed to run app\n");
		return -15;
	}

	printf("Success!\n");

	return 0;
}
