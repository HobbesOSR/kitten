#include <lwk/driver.h>
#include <lwk/console.h>
#include <lwk/string.h>
#include <lwk/delay.h>
#include <rca/rca_l0.h>

/** Template event heder with static fields filled in. */
static rs_event_t ev_hdr = {0};

/** Set when L0 console has been initialized. */
static int initialized = 0;

/**
 * Writes a message to the Cray L0 console.
 */
static void l0_write(struct console *con, const char *str)
{
	int ret = 0;
	unsigned int n = strlen(str);

	while (n) {
		if ((ret =
			ch_send_data(L0RCA_CH_CON_UP, &ev_hdr, (void *)str, n))
		     <= 0)
		{
			/* either error or we are done */
			break;
		}

		if (n > ret) {
			/* some bytes were sent, point to the remaining data */
			str += (n - ret);
			n = ret;
		}

		/* busy wait if the buf is full */
                udelay(1000);
	}

	/* if error, give up and spin forever */
	if (ret < 0)
		while (1) {}

	return;
}

/**
 * Cray L0 console device.
 */
static struct console l0_console = {
	.name  = "Cray RCA L0 Console",
	.write = l0_write
};

/**
 * Initializes the Cray XT L0 console.
 */
int l0_console_init(void)
{
	if (initialized) {
		printk(KERN_ERR "RCA L0 console already initialized.\n");
		return -1;
	}

	/* Read the configuration information provided by the L0 */
	l0rca_init_config();

	/* Setup the event template to use for outgoing events */
	ev_hdr.ev_id		=	ec_console_log;
	ev_hdr.ev_gen		=	RCA_MKSVC(
						RCA_INST_ANY,
						RCA_SVCTYPE_CONS,
						l0rca_get_proc_id()
					);
	ev_hdr.ev_src		=	ev_hdr.ev_gen;
	ev_hdr.ev_priority	=	RCA_LOG_DEBUG;
	ev_hdr.ev_flag		=	0;
	/* Timestamp, len & data is filled at the time of sending event */
	
	/* Register with the Cray RCA subsystem */
	register_ch_up(L0RCA_CH_CON_UP, NULL, 0, -1);

	/* Register the L0 console with the LWK */
	console_register(&l0_console);
	initialized = 1;

	return 0;
}

driver_init("console", l0_console_init);

