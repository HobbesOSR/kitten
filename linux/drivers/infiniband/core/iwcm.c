
#include <linux/in.h>
#include <rdma/iw_cm.h>
#include <linux/kernel.h>

struct iw_cm_id *iw_create_cm_id(struct ib_device *device,
                 iw_cm_handler cm_handler, void *context)
{
 	return NULL;
}

void iw_destroy_cm_id(struct iw_cm_id *cm_id)
{
}

int iw_cm_listen(struct iw_cm_id *cm_id, int backlog)
{
	return 0;
}

int iw_cm_accept(struct iw_cm_id *cm_id, struct iw_cm_conn_param *iw_param)
{
	return 0;
}
int iw_cm_reject(struct iw_cm_id *cm_id, const void *private_data,
         u8 private_data_len)
{
	return 0;
}
int iw_cm_connect(struct iw_cm_id *cm_id, struct iw_cm_conn_param *iw_param)
{
	return 0;
}

int iw_cm_disconnect(struct iw_cm_id *cm_id, int abrupt)
{
	return 0;
}

int iw_cm_init_qp_attr(struct iw_cm_id *cm_id, struct ib_qp_attr *qp_attr,
               int *qp_attr_mask)
{
	return 0;
}
