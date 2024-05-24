#ifndef SD_CARD_H
#define SD_CARD_H


#include <sys/stat.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "sd_card.h"
#include "esp_err.h"


#define MOUNT_POINT "/sdcard"


esp_err_t sd_card_init(void);
void sd_card_deinit(void);
esp_err_t sd_card_write_file(const char *path, const char *data);
esp_err_t sd_card_read_file(const char *path, char *buf, size_t max_len);

#endif // SD_CARD_H
