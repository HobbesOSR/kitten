/* ATA utility functions
 * (c) 2014, Jack Lange <jacklange@cs.pitt.edu>
*/


#include <lwk/driver.h>

/* 
 * Reads the model string from an ATA compatible Drive
 * Stores in pre-allocated dst_str (must be 40 + 1 bytes)
 */
char * 
ata_get_model_str(u8 * id_data, char * dst_str) {
	int i = 0;

	memset(dst_str, 0, 41);

	for (i = 0; i < 40; i += 2) {
		dst_str[i] = id_data[27*2 + i + 1];
		dst_str[i + 1] = id_data[27*2 + i];
	}

	return dst_str;
}


/* 
 * Reads the Firmware string from an ATA compatible Drive
 * Stores in pre-allocated dst_str (must be 8 + 1 bytes)
 */
char * 
ata_get_firmware_str(u8 * id_data, char * dst_str) {
	int i = 0;

	memset(dst_str, 0, 9);

	for (i = 0; i < 8; i += 2) {
		dst_str[i] = id_data[23*2 + i + 1];
		dst_str[i + 1] = id_data[23*2 + i];
	}

	return dst_str;
}

/* 
 * Reads the Serial Number string from an ATA compatible Drive
 * Stores in pre-allocated dst_str (must be 20 + 1 bytes)
 */
char * 
ata_get_serialno_str(u8 * id_data, char * dst_str) {
	int i = 0;

	memset(dst_str, 0, 21);

	for (i = 0; i < 20; i += 2) {
		dst_str[i] = id_data[10*2 + i + 1];
		dst_str[i + 1] = id_data[10*2 + i];
	}

	return dst_str;
}
