#include <lwk/reboot.h>



void (*arm_pm_restart)(enum reboot_mode reboot_mode, const char *cmd);