

#include "cpu.h"
#include "uart_pty.h"

#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_hex.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


uart_pty_t uart_pty;
avr_t * avr = NULL;

struct avr_flash {
	char avr_flash_path[1024];
	int avr_flash_fd;
};

// avr special flash initalization
// here: open and map a file to enable a persistent storage for the flash memory
void avr_special_init( avr_t * avr, void * data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;

	printf("%s\n", __func__);
	// open the file
	flash_data->avr_flash_fd = open(flash_data->avr_flash_path,
									O_RDWR|O_CREAT, 0644);
	if (flash_data->avr_flash_fd < 0) {
		perror(flash_data->avr_flash_path);
		exit(1);
	}
	// resize and map the file the file
	(void)ftruncate(flash_data->avr_flash_fd, avr->flashend + 1);
	ssize_t r = read(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
		exit(1);
	}
}

// avr special flash deinitalization
// here: cleanup the persistent storage
void avr_special_deinit( avr_t* avr, void * data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;

	printf("%s\n", __func__);
	lseek(flash_data->avr_flash_fd, SEEK_SET, 0);
	ssize_t r = write(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
	}
	close(flash_data->avr_flash_fd);
	uart_pty_stop(&uart_pty);
}

avr_t* init_cpu(const char* firmware_path)
{
	struct avr_flash flash_data;
	//char boot_path[1024] = "/home/snietfeld/ATmegaBOOT_168_atmega328.ihex";
	char * mmcu = "atmega328p";
	uint32_t freq = 16000000;

	// for (int i = 1; i < argc; i++) {
	// 	if (!strcmp(argv[i] + strlen(argv[i]) - 4, ".hex"))
	// 		strncpy(boot_path, argv[i], sizeof(boot_path));
	// 	else if (!strcmp(argv[i], "-d"))
	// 		debug++;
	// 	else if (!strcmp(argv[i], "-v"))
	// 		verbose++;
	// 	else {
	// 		fprintf(stderr, "%s: invalid argument %s\n", argv[0], argv[i]);
	// 		exit(1);
	// 	}
	// }

	elf_firmware_t f = {{0}};
	uint32_t loadBase = AVR_SEGMENT_OFFSET_FLASH;

	// Note: Call to sim_setup_firmware will exit() if the .hex file can't be found.
	// We don't want this, so return null if file does not exist
	if (access(firmware_path, F_OK) == 0) {
	  // file exists
	  sim_setup_firmware(firmware_path, loadBase, &f, "simavr-server");
	} else {
	  // file doesn't exist
	  printf("Error: .hex file does not exist: %s\n", firmware_path);
	  return NULL;
	}
	
	
	avr = avr_make_mcu_by_name(mmcu);
	if (!avr) {
		fprintf(stderr, "Error creating the AVR core\n");
		exit(1);
	}


	snprintf(flash_data.avr_flash_path, sizeof(flash_data.avr_flash_path),
			"simduino_%s_flash.bin", mmcu);
	flash_data.avr_flash_fd = 0;
	// register our own functions
	avr->custom.init = avr_special_init;
	avr->custom.deinit = avr_special_deinit;
	avr->custom.data = &flash_data;
	avr_init(avr);
	avr_load_firmware(avr, &f);
	avr->frequency = freq;

	uart_pty_init(avr, &uart_pty);
	uart_pty_connect(&uart_pty, '0');

	return avr;
}
