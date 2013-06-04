#ifndef PIAPI_H
#define PIAPI_H

// The well-known powerinsight proxy ipaddr
#define PIAPI_PRXY_IPADDR   "10.101.4.201"

// The well-known powerinsight agent ipaddr
#define PIAPI_AGNT_IPADDR   "10.54.21.0"

// The well-known powerinsight agent saddr
#define PIAPI_AGNT_SADDR    0x0a361500

// The well-known powerinsight proxy port
#define PIAPI_PRXY_PORT     20203

// The well-known powerinsight agent port
#define PIAPI_AGNT_PORT     20202

typedef enum
{
	PIAPI_UNKNOWN = 0,
	PIAPI_CPU = 1,
	PIAPI_12V = 2,
	PIAPI_MEM = 3,
	PIAPI_5V = 4,
	PIAPI_3_3V = 5,
	PIAPI_HDD_12V = 6,
	PIAPI_HDD_5V = 7,
	PIAPI_ALL = 8
} piapi_port_t;

typedef void (*piapi_callback_t)( char *, unsigned int );

int
piapi_init( void **cntx, piapi_callback_t callback );

int
piapi_destroy( void *cntx );

int
piapi_start( void *cntx, piapi_port_t port );

int
piapi_stop( void *cntx, piapi_port_t port );

#endif
