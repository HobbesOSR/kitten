#define ATA_CMD_READ_PIO         0x20
#define ATA_CMD_READ_PIO_EXT     0x24
#define ATA_CMD_READ_DMA         0xc8
#define ATA_CMD_READ_DMA_EXT     0x25
#define ATA_CMD_WRITE_PIO        0x30
#define ATA_CMD_WRITE_PIO_EXT    0x34
#define ATA_CMD_WRITE_DMA        0xca
#define ATA_CMD_WRITE_DMA_EXT    0x35
#define ATA_CMD_CACHE_FLUSH      0xe7
#define ATA_CMD_CACHE_FLUSH_EXT  0xea
#define ATA_CMD_PACKET           0xa0
#define ATA_CMD_IDENTIFY_PACKET  0xa1
#define ATA_CMD_IDENTIFY         0xec


#define ATA_STS_BUSY             0x80 /* device busy */
#define ATA_STS_RDY              0x40 /* device ready */
#define ATA_STS_DF               0x20 /* device fault */
#define ATA_STS_SKC              0x10 /* seek complete */
#define ATA_STS_DRQ              0x08 /* Data Request */
#define ATA_STS_CORR             0x04 /* Corrected */
#define ATA_STS_IDX              0x02 /* Index */
#define ATA_STS_ERR              0x01 /* Error (ATA) */



char * ata_get_model_str(u8 * id_data, char * dst_str);
char * ata_get_firmware_str(u8 * id_data, char * dst_str);
char * ata_get_serialno_str(u8 * id_data, char * dst_str);
