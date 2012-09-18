/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

#include <linux/mlx4/cmd.h>

#include <linux/mlx4/device.h>

#include "mlx4.h"
#include "en_port.h"

int mlx4_ib_set_4k_mtu = 0;
module_param_named(set_4k_mtu, mlx4_ib_set_4k_mtu, int, 0444);
MODULE_PARM_DESC(set_4k_mtu, "attempt to set 4K MTU to all ConnectX ports");

#define MLX4_MAC_VALID		(1ull << 63)
#define MLX4_MAC_MASK		0x7fffffffffffffffULL

#define MLX4_VLAN_VALID		(1u << 31)
#define MLX4_VLAN_MASK		0xfff

int mlx4_SET_MCAST_FLTR(struct mlx4_dev *dev, u8 port,
			u64 mac, u64 clear, u8 mode)
{
	return mlx4_cmd(dev, (mac | (clear << 63)), port, mode,
			MLX4_CMD_SET_MCAST_FLTR, MLX4_CMD_TIME_CLASS_B);
}
EXPORT_SYMBOL_GPL(mlx4_SET_MCAST_FLTR);

#ifdef HAVE_NETDEV_VLAN_FEATURES
int mlx4_SET_VLAN_FLTR(struct mlx4_dev *dev, u8 port, struct vlan_group *grp)
{
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_set_vlan_fltr_mbox *filter;
	int i;
	int j;
	int index = 0;
	u32 entry;
	int err = 0;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);

	filter = mailbox->buf;
	if (grp) {
		memset(filter, 0, sizeof *filter);
		for (i = VLAN_FLTR_SIZE - 1; i >= 0; i--) {
			entry = 0;
			for (j = 0; j < 32; j++)
				if (vlan_group_get_device(grp, index++))
					entry |= 1 << j;
			filter->entry[i] = cpu_to_be32(entry);
		}
	} else {
		/* When no vlans are configured we block all vlans */
		memset(filter, 0, sizeof(*filter));
	}
	err = mlx4_cmd(dev, mailbox->dma, port, 0, MLX4_CMD_SET_VLAN_FLTR,
		       MLX4_CMD_TIME_CLASS_B);
	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}
EXPORT_SYMBOL_GPL(mlx4_SET_VLAN_FLTR);
#endif

void mlx4_init_mac_table(struct mlx4_dev *dev, struct mlx4_mac_table *table)
{
	int i;

	mutex_init(&table->mutex);
	for (i = 0; i < MLX4_MAX_MAC_NUM; i++) {
		table->entries[i] = 0;
		table->refs[i]	 = 0;
	}
	table->max   = 1 << dev->caps.log_num_macs;
	table->total = 0;
}

void mlx4_init_vlan_table(struct mlx4_dev *dev, struct mlx4_vlan_table *table)
{
	int i;

	mutex_init(&table->mutex);
	for (i = 0; i < MLX4_MAX_VLAN_NUM; i++) {
		table->entries[i] = 0;
		table->refs[i]	 = 0;
	}
	table->max   = 1 << dev->caps.log_num_vlans;
	table->total = 0;
}

static int mlx4_set_port_mac_table(struct mlx4_dev *dev, u8 port,
				   __be64 *entries)
{
	struct mlx4_cmd_mailbox *mailbox;
	u32 in_mod;
	int err;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);

	memcpy(mailbox->buf, entries, MLX4_MAC_TABLE_SIZE);

	in_mod = MLX4_SET_PORT_MAC_TABLE << 8 | port;
	err = mlx4_cmd(dev, mailbox->dma, in_mod, 1, MLX4_CMD_SET_PORT,
		       MLX4_CMD_TIME_CLASS_B);

	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}

static int mlx4_uc_steer_add(struct mlx4_dev *dev, u8 port,
			     u64 mac, int *qpn, u8 reserve)
{
	struct mlx4_qp qp;
	u8 gid[16] = {0};
	int err;

	if (reserve) {
		err = mlx4_qp_reserve_range(dev, 1, 1, qpn, 0xC0);
		if (err) {
			mlx4_err(dev, "Failed to reserve qp for mac registration\n");
			return err;
		}
	}
	qp.qpn = *qpn;

	mac &= 0xffffffffffffULL;
	mac = cpu_to_be64(mac << 16);
	memcpy(&gid[10], &mac, ETH_ALEN);
	gid[5] = port;
	gid[7] = MLX4_UC_STEER << 1;

	err = mlx4_qp_attach_common(dev, &qp, gid, 0,
				    MLX4_PROT_ETH, MLX4_UC_STEER, 0);
	if (err && reserve)
		mlx4_qp_release_range(dev, *qpn, 1);

	return err;
}

static void mlx4_uc_steer_release(struct mlx4_dev *dev, u8 port,
				  u64 mac, int qpn, u8 free)
{
	struct mlx4_qp qp;
	u8 gid[16] = {0};

	qp.qpn = qpn;
	mac &= 0xffffffffffffULL;
	mac = cpu_to_be64(mac << 16);
	memcpy(&gid[10], &mac, ETH_ALEN);
	gid[5] = port;
	gid[7] = MLX4_UC_STEER << 1;

	mlx4_qp_detach_common(dev, &qp, gid, MLX4_PROT_ETH, MLX4_UC_STEER, 0);
	if (free)
		mlx4_qp_release_range(dev, qpn, 1);
}

int mlx4_register_mac(struct mlx4_dev *dev, u8 port, u64 mac, int *qpn, u8 wrap)
{
	struct mlx4_port_info *info = &mlx4_priv(dev)->port[port];
	struct mlx4_mac_table *table = &info->mac_table;
	struct mlx4_mac_entry *entry;
	int i, err = 0;
	int free = -1;

	if (dev->caps.vep_uc_steering) {
		err = mlx4_uc_steer_add(dev, port, mac, qpn, 1);
		if (!err) {
			entry = kmalloc(sizeof *entry, GFP_KERNEL);
			if (!entry) {
				mlx4_uc_steer_release(dev, port, mac, *qpn, 1);
				return -ENOMEM;
			}
			entry->mac = mac;
			err = radix_tree_insert(&info->mac_tree, *qpn, entry);
			if (err) {
				mlx4_uc_steer_release(dev, port, mac, *qpn, 1);
				return err;
			}
		} else
			return err;
	}

	mlx4_dbg(dev, "Registering MAC: 0x%llx\n", (unsigned long long) mac);
	mutex_lock(&table->mutex);
	for (i = 0; i < MLX4_MAX_MAC_NUM; i++) {
		if (free < 0 && !table->entries[i]) {
			free = i;
			continue;
		}

		if (mac == (MLX4_MAC_MASK & be64_to_cpu(table->entries[i]))) {
			/* MAC already registered, Must not have duplicates */
			err = -EEXIST;
			goto out;
		}
	}

	mlx4_dbg(dev, "Free MAC index is %d\n", free);

	if (table->total == table->max) {
		/* No free mac entries */
		err = -ENOSPC;
		goto out;
	}

	/* Register new MAC */
	table->entries[free] = cpu_to_be64(mac | MLX4_MAC_VALID);

	err = mlx4_set_port_mac_table(dev, port, table->entries);
	if (unlikely(err)) {
		mlx4_err(dev, "Failed adding MAC: 0x%llx\n", (unsigned long long) mac);
		table->entries[free] = 0;
		goto out;
	}

	if (!dev->caps.vep_uc_steering)
		*qpn = info->base_qpn + free;
	++table->total;
out:
	mutex_unlock(&table->mutex);
	return err;
}
EXPORT_SYMBOL_GPL(mlx4_register_mac);

static int validate_index(struct mlx4_dev *dev,
			  struct mlx4_mac_table *table, int index)
{
	int err = 0;

	if (index < 0 || index >= table->max || !table->entries[index]) {
		mlx4_warn(dev, "No valid Mac entry for the given index\n");
		err = -EINVAL;
	}
	return err;
}

static int find_index(struct mlx4_dev *dev,
		      struct mlx4_mac_table *table, u64 mac)
{
	int i;
	for (i = 0; i < MLX4_MAX_MAC_NUM; i++) {
		if (mac == (MLX4_MAC_MASK & be64_to_cpu(table->entries[i])))
			return i;
	}
	/* Mac not found */
	return -EINVAL;
}

void mlx4_unregister_mac(struct mlx4_dev *dev, u8 port, int qpn)
{
	struct mlx4_port_info *info = &mlx4_priv(dev)->port[port];
	struct mlx4_mac_table *table = &info->mac_table;
	int index = qpn - info->base_qpn;
	struct mlx4_mac_entry *entry;

	if (dev->caps.vep_uc_steering) {
		entry = radix_tree_lookup(&info->mac_tree, qpn);
		if (entry) {
			mlx4_uc_steer_release(dev, port, entry->mac, qpn, 1);
			radix_tree_delete(&info->mac_tree, qpn);
			index = find_index(dev, table, entry->mac);
			kfree(entry);
		}
	}

	mutex_lock(&table->mutex);

	if (validate_index(dev, table, index))
		goto out;

	table->entries[index] = 0;
	mlx4_set_port_mac_table(dev, port, table->entries);
	--table->total;
out:
	mutex_unlock(&table->mutex);
}
EXPORT_SYMBOL_GPL(mlx4_unregister_mac);

int mlx4_replace_mac(struct mlx4_dev *dev, u8 port, int qpn, u64 new_mac, u8 wrap)
{
	struct mlx4_port_info *info = &mlx4_priv(dev)->port[port];
	struct mlx4_mac_table *table = &info->mac_table;
	int index = qpn - info->base_qpn;
	struct mlx4_mac_entry *entry;
	int err;

	if (dev->caps.vep_uc_steering) {
		entry = radix_tree_lookup(&info->mac_tree, qpn);
		if (!entry)
			return -EINVAL;
		index = find_index(dev, table, entry->mac);
		mlx4_uc_steer_release(dev, port, entry->mac, qpn, 0);
		entry->mac = new_mac;
		err = mlx4_uc_steer_add(dev, port, entry->mac, &qpn, 0);
		if (err || index < 0)
			return err;
	}

	mutex_lock(&table->mutex);

	err = validate_index(dev, table, index);
	if (err)
		goto out;

	table->entries[index] = cpu_to_be64(new_mac | MLX4_MAC_VALID);

	err = mlx4_set_port_mac_table(dev, port, table->entries);
	if (unlikely(err)) {
		mlx4_err(dev, "Failed adding MAC: 0x%llx\n", (unsigned long long) new_mac);
		table->entries[index] = 0;
	}
out:
	mutex_unlock(&table->mutex);
	return err;
}
EXPORT_SYMBOL_GPL(mlx4_replace_mac);

static int mlx4_set_port_vlan_table(struct mlx4_dev *dev, u8 port,
				    __be32 *entries)
{
	struct mlx4_cmd_mailbox *mailbox;
	u32 in_mod;
	int err;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);

	memcpy(mailbox->buf, entries, MLX4_VLAN_TABLE_SIZE);
	in_mod = MLX4_SET_PORT_VLAN_TABLE << 8 | port;
	err = mlx4_cmd(dev, mailbox->dma, in_mod, 1, MLX4_CMD_SET_PORT,
		       MLX4_CMD_TIME_CLASS_B);

	mlx4_free_cmd_mailbox(dev, mailbox);

	return err;
}

int mlx4_find_cached_vlan(struct mlx4_dev *dev, u8 port, u16 vid, int *idx)
{
	struct mlx4_vlan_table *table = &mlx4_priv(dev)->port[port].vlan_table;
	int i;

	for (i = 0; i < MLX4_MAX_VLAN_NUM; ++i) {
		if (table->refs[i] &&
		    (vid == (MLX4_VLAN_MASK &
			      be32_to_cpu(table->entries[i])))) {
			/* Vlan already registered, increase refernce count */
			*idx = i;
			return 0;
		}
	}

	return -ENOENT;
}
EXPORT_SYMBOL_GPL(mlx4_find_cached_vlan);

int mlx4_register_vlan(struct mlx4_dev *dev, u8 port, u16 vlan, int *index)
{
	struct mlx4_vlan_table *table = &mlx4_priv(dev)->port[port].vlan_table;
	int i, err = 0;
	int free = -1;

	mutex_lock(&table->mutex);
	for (i = MLX4_VLAN_REGULAR; i < MLX4_MAX_VLAN_NUM; i++) {
		if (free < 0 && (table->refs[i] == 0)) {
			free = i;
			continue;
		}

		if (table->refs[i] &&
		    (vlan == (MLX4_VLAN_MASK &
			      be32_to_cpu(table->entries[i])))) {
			/* Vlan already registered, increase refernce count */
			*index = i;
			++table->refs[i];
			goto out;
		}
	}

	if (table->total == table->max) {
		/* No free vlan entries */
		err = -ENOSPC;
		goto out;
	}

	/* Register new MAC */
	table->refs[free] = 1;
	table->entries[free] = cpu_to_be32(vlan | MLX4_VLAN_VALID);

	err = mlx4_set_port_vlan_table(dev, port, table->entries);
	if (unlikely(err)) {
		mlx4_warn(dev, "Failed adding vlan: %u\n", vlan);
		table->refs[free] = 0;
		table->entries[free] = 0;
		goto out;
	}

	*index = free;
	++table->total;
out:
	mutex_unlock(&table->mutex);
	return err;
}
EXPORT_SYMBOL_GPL(mlx4_register_vlan);

void mlx4_unregister_vlan(struct mlx4_dev *dev, u8 port, int index)
{
	struct mlx4_vlan_table *table = &mlx4_priv(dev)->port[port].vlan_table;

	if (index < MLX4_VLAN_REGULAR) {
		mlx4_warn(dev, "Trying to free special vlan index %d\n", index);
		return;
	}

	mutex_lock(&table->mutex);
	if (!table->refs[index]) {
		mlx4_warn(dev, "No vlan entry for index %d\n", index);
		goto out;
	}
	if (--table->refs[index]) {
		mlx4_dbg(dev, "Have more references for index %d,"
			 "no need to modify vlan table\n", index);
		goto out;
	}
	table->entries[index] = 0;
	mlx4_set_port_vlan_table(dev, port, table->entries);
	--table->total;
out:
	mutex_unlock(&table->mutex);
}
EXPORT_SYMBOL_GPL(mlx4_unregister_vlan);

int mlx4_get_port_ib_caps(struct mlx4_dev *dev, u8 port, __be32 *caps)
{
	struct mlx4_cmd_mailbox *inmailbox, *outmailbox;
	u8 *inbuf, *outbuf;
	int err;

	inmailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(inmailbox))
		return PTR_ERR(inmailbox);

	outmailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(outmailbox)) {
		mlx4_free_cmd_mailbox(dev, inmailbox);
		return PTR_ERR(outmailbox);
	}

	inbuf = inmailbox->buf;
	outbuf = outmailbox->buf;
	memset(inbuf, 0, 256);
	memset(outbuf, 0, 256);
	inbuf[0] = 1;
	inbuf[1] = 1;
	inbuf[2] = 1;
	inbuf[3] = 1;
	*(__be16 *) (&inbuf[16]) = cpu_to_be16(0x0015);
	*(__be32 *) (&inbuf[20]) = cpu_to_be32(port);

	err = mlx4_cmd_box(dev, inmailbox->dma, outmailbox->dma, port, 3,
			   MLX4_CMD_MAD_IFC, MLX4_CMD_TIME_CLASS_C);
	if (!err)
		*caps = *(__be32 *) (outbuf + 84);
	mlx4_free_cmd_mailbox(dev, inmailbox);
	mlx4_free_cmd_mailbox(dev, outmailbox);
	return err;
}

int mlx4_SET_PORT(struct mlx4_dev *dev, u8 port)
{
	struct mlx4_cmd_mailbox *mailbox;
	int err;

	if (dev->caps.port_type[port] != MLX4_PORT_TYPE_IB)
		return 0;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);

	memset(mailbox->buf, 0, 256);

	if (mlx4_ib_set_4k_mtu)
		((__be32 *) mailbox->buf)[0] |= cpu_to_be32((1 << 22) | (1 << 21) | (5 << 12) | (2 << 4));

	((__be32 *) mailbox->buf)[1] = dev->caps.ib_port_def_cap[port];
	err = mlx4_cmd(dev, mailbox->dma, port, 0, MLX4_CMD_SET_PORT,
		       MLX4_CMD_TIME_CLASS_B);

	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}

int mlx4_SET_PORT_general(struct mlx4_dev *dev, u8 port, int mtu,
			  u8 pptx, u8 pfctx, u8 pprx, u8 pfcrx)
{
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_set_port_general_context *context;
	int err;
	u32 in_mod;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	context = mailbox->buf;
	memset(context, 0, sizeof *context);

	context->flags = SET_PORT_GEN_ALL_VALID;
	context->mtu = cpu_to_be16(mtu);
	context->pptx = (pptx * (!pfctx)) << 7;
	context->pfctx = pfctx;
	context->pprx = (pprx * (!pfcrx)) << 7;
	context->pfcrx = pfcrx;

	in_mod = MLX4_SET_PORT_GENERAL << 8 | port;
	err = mlx4_cmd(dev, mailbox->dma, in_mod, 1, MLX4_CMD_SET_PORT,
		       MLX4_CMD_TIME_CLASS_B);

	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}
EXPORT_SYMBOL(mlx4_SET_PORT_general);

int mlx4_SET_PORT_qpn_calc(struct mlx4_dev *dev, u8 port, u32 base_qpn,
			   u8 promisc)
{
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_set_port_rqp_calc_context *context;
	int err;
	u32 in_mod;
	u32 m_promisc = (dev->caps.vep_mc_steering) ? MCAST_DIRECT : MCAST_DEFAULT;

	if (dev->caps.vep_mc_steering && dev->caps.vep_uc_steering)
		return 0;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	context = mailbox->buf;
	memset(context, 0, sizeof *context);

	context->base_qpn = cpu_to_be32(base_qpn);
	context->n_mac = 0x7;
	context->promisc = cpu_to_be32(promisc << SET_PORT_PROMISC_SHIFT |
				       base_qpn);
	context->mcast = cpu_to_be32(m_promisc << SET_PORT_MC_PROMISC_SHIFT |
				     base_qpn);
	context->intra_no_vlan = 0;
	context->no_vlan = MLX4_NO_VLAN_IDX;
	context->intra_vlan_miss = 0;
	context->vlan_miss = MLX4_VLAN_MISS_IDX;

	in_mod = MLX4_SET_PORT_RQP_CALC << 8 | port;
	err = mlx4_cmd(dev, mailbox->dma, in_mod, 1, MLX4_CMD_SET_PORT,
			       MLX4_CMD_TIME_CLASS_B);

	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}
EXPORT_SYMBOL(mlx4_SET_PORT_qpn_calc);
