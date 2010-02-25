
#include <linux/in.h>
#include <rdma/iw_cm.h>
#include <linux/lwk_stubs.h>

struct iw_cm_id *iw_create_cm_id(struct ib_device *device,
                 iw_cm_handler cm_handler, void *context)
{
	LINUX_DBG( TRUE, "\n" );	
 	return NULL;
}

void iw_destroy_cm_id(struct iw_cm_id *cm_id)
{
	LINUX_DBG( TRUE, "\n" );	
}

int iw_cm_listen(struct iw_cm_id *cm_id, int backlog)
{
	LINUX_DBG( TRUE, "\n" );	
	return 0;
}

int iw_cm_accept(struct iw_cm_id *cm_id, struct iw_cm_conn_param *iw_param)
{
	LINUX_DBG( TRUE, "\n" );	
	return 0;
}
int iw_cm_reject(struct iw_cm_id *cm_id, const void *private_data,
         u8 private_data_len)
{
	LINUX_DBG( TRUE, "\n" );	
	return 0;
}
int iw_cm_connect(struct iw_cm_id *cm_id, struct iw_cm_conn_param *iw_param)
{
	LINUX_DBG( TRUE, "\n" );	
	return 0;
}

int iw_cm_disconnect(struct iw_cm_id *cm_id, int abrupt)
{
	LINUX_DBG( TRUE, "\n" );	
	return 0;
}

int iw_cm_init_qp_attr(struct iw_cm_id *cm_id, struct ib_qp_attr *qp_attr,
               int *qp_attr_mask)
{
	LINUX_DBG( TRUE, "\n" );	
	return 0;
}
