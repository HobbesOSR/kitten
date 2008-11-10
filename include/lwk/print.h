#ifndef _LWK_PRINT_H
#define _LWK_PRINT_H

#define KERN_EMERG	"<0>"	/* system is unusable                   */
#define KERN_ALERT	"<1>"	/* action must be taken immediately     */
#define KERN_CRIT	"<2>"	/* critical conditions                  */
#define KERN_ERR	"<3>"	/* error conditions                     */
#define KERN_WARNING	"<4>"	/* warning conditions                   */
#define KERN_NOTICE	"<5>"	/* normal but significant condition     */
#define KERN_INFO	"<6>"	/* informational                        */
#define KERN_DEBUG	"<7>"	/* debug-level messages                 */
#define KERN_USERMSG	"<8>"	/* message from user-space		*/
#define KERN_NORM	"<9>"	/* a "normal" message, nothing special	*/

#define USER_EMERG	"<0>"	/* system is unusable                   */
#define USER_ALERT	"<1>"	/* action must be taken immediately     */
#define USER_CRIT	"<2>"	/* critical conditions                  */
#define USER_ERR	"<3>"	/* error conditions                     */
#define USER_WARNING	"<4>"	/* warning conditions                   */
#define USER_NOTICE	"<5>"	/* normal but significant condition     */
#define USER_INFO	"<6>"	/* informational                        */
#define USER_DEBUG	"<7>"	/* debug-level messages                 */
#define USER_USERMSG	"<8>"	/* message from user-space		*/
#define USER_NORM	""	/* a "normal" message, nothing special	*/

#ifdef __KERNEL__
#define TYPE_EMERG	KERN_EMERG
#define TYPE_ALERT	KERN_ALERT
#define TYPE_CRIT	KERN_CRIT
#define TYPE_ERR	KERN_ERR
#define TYPE_WARNING	KERN_WARNING
#define TYPE_NOTICE	KERN_NOTICE
#define TYPE_INFO	KERN_INFO
#define TYPE_DEBUG	KERN_DEBUG
#define TYPE_USERMSG	KERN_USERMSG
#define TYPE_NORM	KERN_NORM
#else
#define TYPE_EMERG	USER_EMERG
#define TYPE_ALERT	USER_ALERT
#define TYPE_CRIT	USER_CRIT
#define TYPE_ERR	USER_ERR
#define TYPE_WARNING	USER_WARNING
#define TYPE_NOTICE	USER_NOTICE
#define TYPE_INFO	USER_INFO
#define TYPE_DEBUG	USER_DEBUG
#define TYPE_USERMSG	USER_USERMSG
#define TYPE_NORM	USER_NORM
#endif

#ifdef __KERNEL__
#include <stdarg.h>
#include <lwk/types.h>
extern int sprintf(char * buf, const char * fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
extern int vsprintf(char *buf, const char *, va_list)
	__attribute__ ((format (printf, 2, 0)));
extern int snprintf(char * buf, size_t size, const char * fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
	__attribute__ ((format (printf, 3, 0)));
extern int scnprintf(char * buf, size_t size, const char * fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
extern int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
	__attribute__ ((format (printf, 3, 0)));
extern int vprintk(const char *fmt, va_list args)
        __attribute__ ((format (printf, 1, 0)));
extern int printk(const char * fmt, ...)
        __attribute__ ((format (printf, 1, 2)));
#define print printk
#else
#include <stdio.h>
#include <stdarg.h>
#define print printf
#endif

extern const char *
hexdump(
	const void *		data,
	size_t			len
);

#endif
