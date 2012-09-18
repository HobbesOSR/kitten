/*
 * Copyright (c) 2006, 2007 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007, 2008 Mellanox Technologies. All rights reserved.
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

#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <linux/mlx4/cmd.h>
#include <linux/mlx4/driver.h>

#include "mlx4.h"

struct mlx4_steer_prio {
	struct list_head list;
	int hash;
	u32 num_high_prio;
};

static int mlx4_READ_ENTRY(struct mlx4_dev *dev, int index,
			   struct mlx4_cmd_mailbox *mailbox)
{
	return mlx4_cmd_box(dev, 0, mailbox->dma, index, 0,
			    MLX4_CMD_READ_MCG, MLX4_CMD_TIME_CLASS_A);
}

static int mlx4_WRITE_ENTRY(struct mlx4_dev *dev, int index,
			    struct mlx4_cmd_mailbox *mailbox)
{
	return mlx4_cmd(dev, mailbox->dma, index, 0,
			MLX4_CMD_WRITE_MCG, MLX4_CMD_TIME_CLASS_A);
}

static int mlx4_WRITE_PROMISC(struct mlx4_dev *dev, u8 vep_num, u8 port, u8 steer,
			      struct mlx4_cmd_mailbox *mailbox)
{
	u32 in_mod;

	in_mod = (u32) vep_num << 24 | (u32) port << 16 | steer << 1;
	return mlx4_cmd(dev, mailbox->dma, in_mod, 0x1,
			MLX4_CMD_WRITE_MCG, MLX4_CMD_TIME_CLASS_A);
}

static int mlx4_GID_HASH(struct mlx4_dev *dev, struct mlx4_cmd_mailbox *mailbox,
			 u16 *hash, u8 op_mod)
{
	u64 imm;
	int err;

	err = mlx4_cmd_imm(dev, mailbox->dma, &imm, 0, op_mod,
			   MLX4_CMD_MGID_HASH, MLX4_CMD_TIME_CLASS_A);

	if (!err)
		*hash = imm;

	return err;
}

/*
 * Helper functions to manage multifunction steering data structures.
 * Used only for Ethernet steering.
 */

static struct mlx4_promisc_qp *get_promisc_qp(struct mlx4_dev *dev, u8 pf_num,
					      enum mlx4_steer_type steer,
					      u32 qpn)
{
	struct mlx4_steer *s_steer = &mlx4_priv(dev)->steer[pf_num];
	struct mlx4_promisc_qp *pqp;

	list_for_each_entry(pqp, &s_steer->promisc_qps[steer], list) {
		if (pqp->qpn == qpn)
			return pqp;
	}
	/* not found */
	return NULL;
}

/*
 * Add new entry to steering data structure.
 * All promisc QPs should be added as well
 */
static int new_steering_entry(struct mlx4_dev *dev, u8 vep_num, u8 port,
			      enum mlx4_steer_type steer,
			      unsigned int index, u32 qpn)
{
	struct mlx4_steer *s_steer;
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_mgm *mgm;
	u32 members_count;
	struct mlx4_steer_index *new_entry;
	struct mlx4_promisc_qp *pqp;
	struct mlx4_promisc_qp *dqp;
	u32 prot;
	int err;
	u8 pf_num;

	pf_num = (dev->caps.num_ports == 1) ? vep_num : (vep_num << 1) | (port - 1);
	s_steer = &mlx4_priv(dev)->steer[pf_num];
	new_entry = kzalloc(sizeof *new_entry, GFP_KERNEL);
	if (!new_entry)
		return -ENOMEM;

	INIT_LIST_HEAD(&new_entry->duplicates);
	new_entry->index = index;
	list_add_tail(&new_entry->list, &s_steer->steer_entries[steer]);

	/* If the given qpn is also a promisc qp,
	 * it should be inserted to duplicates list
	 */
	pqp = get_promisc_qp(dev, pf_num, steer, qpn);
	if (pqp) {
		dqp = kmalloc(sizeof *dqp, GFP_KERNEL);
		if (!dqp) {
			err = -ENOMEM;
			goto out_alloc;
		}
		dqp->qpn = qpn;
		list_add_tail(&dqp->list, &new_entry->duplicates);
	}

	/* if no promisc qps for this vep, we are done */
	if (list_empty(&s_steer->promisc_qps[steer]))
		return 0;

	/* now need to add all the promisc qps to the new
	 * steering entry, as they should also receive the packets
	 * destined to this address */
	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox)) {
		err = -ENOMEM;
		goto out_alloc;
	}
	mgm = mailbox->buf;

	err = mlx4_READ_ENTRY(dev, index, mailbox);
	if (err)
		goto out_mailbox;

	members_count = be32_to_cpu(mgm->members_count) & 0xffffff;
	prot = be32_to_cpu(mgm->members_count) >> 30;
	list_for_each_entry(pqp, &s_steer->promisc_qps[steer], list) {
		/* don't add already existing qpn */
		if (pqp->qpn == qpn)
			continue;
		if (members_count == MLX4_QP_PER_MGM) {
			/* out of space */
			err = -ENOMEM;
			goto out_mailbox;
		}

		/* add the qpn */
		mgm->qp[members_count++] = cpu_to_be32(pqp->qpn & MGM_QPN_MASK);
	}
	/* update the qps count and update the entry with all the promisc qps*/
	mgm->members_count = cpu_to_be32(members_count | (prot << 30));
	err = mlx4_WRITE_ENTRY(dev, index, mailbox);

out_mailbox:
	mlx4_free_cmd_mailbox(dev, mailbox);
	if (!err)
		return 0;
out_alloc:
	if (dqp) {
		list_del(&dqp->list);
		kfree(&dqp);
	}
	list_del(&new_entry->list);
	kfree(new_entry);
	return err;
}

/* update the data structures with existing steering entry */
static int existing_steering_entry(struct mlx4_dev *dev, u8 vep_num, u8 port,
				   enum mlx4_steer_type steer,
				   unsigned int index, u32 qpn)
{
	struct mlx4_steer *s_steer;
	struct mlx4_steer_index *tmp_entry, *entry = NULL;
	struct mlx4_promisc_qp *pqp;
	struct mlx4_promisc_qp *dqp;
	u8 pf_num;

	pf_num = (dev->caps.num_ports == 1) ? vep_num : (vep_num << 1) | (port - 1);
	s_steer = &mlx4_priv(dev)->steer[pf_num];

	pqp = get_promisc_qp(dev, pf_num, steer, qpn);
	if (!pqp)
		return 0; /* nothing to do */

	list_for_each_entry(tmp_entry, &s_steer->steer_entries[steer], list) {
		if (tmp_entry->index == index) {
			entry = tmp_entry;
			break;
		}
	}
	if (unlikely(!entry)) {
		mlx4_warn(dev, "Steering entry at index %x is not registered\n", index);
		return -EINVAL;
	}

	/* the given qpn is listed as a promisc qpn
	 * we need to add it as a duplicate to this entry
	 * for future refernce */
	list_for_each_entry(dqp, &entry->duplicates, list) {
		if (qpn == dqp->qpn)
			return 0; /* qp is already duplicated */
	}

	/* add the qp as a duplicate on this index */
	dqp = kmalloc(sizeof *dqp, GFP_KERNEL);
	if (!dqp)
		return -ENOMEM;
	dqp->qpn = qpn;
	list_add_tail(&dqp->list, &entry->duplicates);

	return 0;
}

/* Check whether a qpn is a duplicate on steering entry
 * If so, it should not be removed from mgm */
static bool check_duplicate_entry(struct mlx4_dev *dev, u8 vep_num, u8 port,
				  enum mlx4_steer_type steer,
				  unsigned int index, u32 qpn)
{
	struct mlx4_steer *s_steer;
	struct mlx4_steer_index *tmp_entry, *entry = NULL;
	struct mlx4_promisc_qp *dqp, *tmp_dqp;
	u8 pf_num;

	pf_num = (dev->caps.num_ports == 1) ? vep_num : (vep_num << 1) | (port - 1);
	s_steer = &mlx4_priv(dev)->steer[pf_num];

	/* if qp is not promisc, it cannot be duplicated */
	if (!get_promisc_qp(dev, pf_num, steer, qpn))
		return false;

	/* The qp is promisc qp so it is a duplicate on this index
	 * Find the index entry, and remove the duplicate */
	list_for_each_entry(tmp_entry, &s_steer->steer_entries[steer], list) {
		if (tmp_entry->index == index) {
			entry = tmp_entry;
			break;
		}
	}
	if (unlikely(!entry)) {
		mlx4_warn(dev, "Steering entry for index %x is not registered\n", index);
		return false;
	}
	list_for_each_entry_safe(dqp, tmp_dqp, &entry->duplicates, list) {
		if (dqp->qpn == qpn) {
			list_del(&dqp->list);
			kfree(dqp);
		}
	}
	return true;
}

/* I a steering entry contains only promisc QPs, it can be removed. */
static bool can_remove_steering_entry(struct mlx4_dev *dev, u8 vep_num, u8 port,
				      enum mlx4_steer_type steer,
				      unsigned int index, u32 tqpn)
{
	struct mlx4_steer *s_steer;
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_mgm *mgm;
	struct mlx4_steer_index *entry = NULL, *tmp_entry;
	u32 qpn;
	u32 members_count;
	bool ret = false;
	int i;
	u8 pf_num;

	pf_num = (dev->caps.num_ports == 1) ? vep_num : (vep_num << 1) | (port - 1);
	s_steer = &mlx4_priv(dev)->steer[pf_num];

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return false;
	mgm = mailbox->buf;

	if (mlx4_READ_ENTRY(dev, index, mailbox))
		goto out;
	members_count = be32_to_cpu(mgm->members_count) & 0xffffff;
	for (i = 0;  i < members_count; i++) {
		qpn = be32_to_cpu(mgm->qp[i]) & MGM_QPN_MASK;
		if (!get_promisc_qp(dev, pf_num, steer, qpn) && qpn != tqpn) {
			/* the qp is not promisc, the entry can't be removed */
			goto out;
		}
	}
	 /* All the qps currently registered for this entry are promiscuous,
	  * Checking for duplicates */
	ret = true;
	list_for_each_entry_safe(entry, tmp_entry, &s_steer->steer_entries[steer], list) {
		if (entry->index == index) {
			if (list_empty(&entry->duplicates)) {
				list_del(&entry->list);
				kfree(entry);
			} else {
				/* This entry contains duplicates so it shouldn't be removed */
				ret = false;
				goto out;
			}
		}
	}

out:
	mlx4_free_cmd_mailbox(dev, mailbox);
	return ret;
}

/* Adjust the index of an existing steering entry*/
static void adjust_steering_entry(struct mlx4_dev *dev, u8 vep_num, u8 port,
                                  enum mlx4_steer_type steer,
                                  unsigned int old_index, unsigned int new_index)
{
	struct mlx4_steer *s_steer;
	u8 pf_num;
	struct mlx4_steer_index *entry;

	pf_num = (dev->caps.num_ports == 1) ? vep_num : (vep_num << 1) | (port - 1);
	s_steer = &mlx4_priv(dev)->steer[pf_num];
	list_for_each_entry(entry, &s_steer->steer_entries[steer], list) {
		if (entry->index == old_index) {
			entry->index = new_index;
			return;
		}
	}
}

static int add_promisc_qp(struct mlx4_dev *dev, u8 vep_num, u8 port,
			  enum mlx4_steer_type steer, u32 qpn)
{
	struct mlx4_steer *s_steer;
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_mgm *mgm;
	struct mlx4_steer_index *entry;
	struct mlx4_promisc_qp *pqp;
	struct mlx4_promisc_qp *dqp;
	u32 members_count;
	u32 prot;
	int i;
	bool found;
	int last_index;
	int err;
	u8 pf_num;
	struct mlx4_priv *priv = mlx4_priv(dev);
	pf_num = (dev->caps.num_ports == 1) ? vep_num : (vep_num << 1) | (port - 1);
	s_steer = &mlx4_priv(dev)->steer[pf_num];

	mutex_lock(&priv->mcg_table.mutex);

	if (get_promisc_qp(dev, pf_num, steer, qpn)){
		err = 0;  /* Noting to do, already exists */
		goto out_mutex;
	}

	pqp = kmalloc(sizeof *pqp, GFP_KERNEL);
	if (!pqp) {
		err = -ENOMEM;
    	goto out_mutex;
	}
	pqp->qpn = qpn;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox)) {
		err = -ENOMEM;
		goto out_alloc;
	}
	mgm = mailbox->buf;

	/* the promisc qp needs to be added for each one of the steering
	 * entries, if it already exists, needs to be added as a duplicate
	 * for this entry */
	list_for_each_entry(entry, &s_steer->steer_entries[steer], list) {
		err = mlx4_READ_ENTRY(dev, entry->index, mailbox);
		if (err)
			goto out_mailbox;

		members_count = be32_to_cpu(mgm->members_count) & 0xffffff;
		prot = be32_to_cpu(mgm->members_count) >> 30;
		found = false;
		for (i = 0; i < members_count; i++) {
			if ((be32_to_cpu(mgm->qp[i]) & MGM_QPN_MASK) == qpn) {
				/* Entry already exists, add to duplicates */
				dqp = kmalloc(sizeof *dqp, GFP_KERNEL);
				if (!dqp)
					goto out_mailbox;
				dqp->qpn = qpn;
				list_add_tail(&dqp->list, &entry->duplicates);
				found = true;
			}
		}
		if (!found) {
			/* Need to add the qpn to mgm */
			if (members_count == MLX4_QP_PER_MGM) {
				/* entry is full */
				err = -ENOMEM;
				goto out_mailbox;
			}
			mgm->qp[members_count++] = cpu_to_be32(qpn & MGM_QPN_MASK);
			mgm->members_count = cpu_to_be32(members_count | (prot << 30));
			err = mlx4_WRITE_ENTRY(dev, entry->index, mailbox);
			if (err)
				goto out_mailbox;
		}
		last_index = entry->index;
	}

	/* add the new qpn to list of promisc qps */
	list_add_tail(&pqp->list, &s_steer->promisc_qps[steer]);
	/* now need to add all the promisc qps to default entry */
	memset(mgm, 0, sizeof *mgm);
	members_count = 0;
	list_for_each_entry(dqp, &s_steer->promisc_qps[steer], list)
		mgm->qp[members_count++] = cpu_to_be32(dqp->qpn & MGM_QPN_MASK);
	mgm->members_count = cpu_to_be32(members_count | MLX4_PROT_ETH << 30);

	err = mlx4_WRITE_PROMISC(dev, vep_num, port, steer, mailbox);
	if (err)
		goto out_list;

	mlx4_free_cmd_mailbox(dev, mailbox);
	mutex_unlock(&priv->mcg_table.mutex);
	return 0;

out_list:
	list_del(&pqp->list);
out_mailbox:
	/* TODO: undo partial addition of promisc qps */
	mlx4_free_cmd_mailbox(dev, mailbox);
out_alloc:
	kfree(pqp);
out_mutex:
    mutex_unlock(&priv->mcg_table.mutex);
    return err;
}

static int remove_promisc_qp(struct mlx4_dev *dev, u8 vep_num, u8 port,
			     enum mlx4_steer_type steer, u32 qpn)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_steer *s_steer;
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_mgm *mgm;
	struct mlx4_steer_index *entry;
	struct mlx4_promisc_qp *pqp;
	struct mlx4_promisc_qp *dqp;
	u32 members_count;
	bool found;
	bool back_to_list = false;
	int loc, i;
	int err;
	u8 pf_num;

	pf_num = (dev->caps.num_ports == 1) ? vep_num : (vep_num << 1) | (port - 1);
	s_steer = &mlx4_priv(dev)->steer[pf_num];
	mutex_lock(&priv->mcg_table.mutex);

	pqp = get_promisc_qp(dev, pf_num, steer, qpn);
	if (unlikely(!pqp)) {
		mlx4_warn(dev, "QP %x is not promiscuous QP\n", qpn);
		/* nothing to do */
		err = 0;
		goto out_mutex;
	}

	/*remove from list of promisc qps */
	list_del(&pqp->list);
	kfree(pqp);

	/* set the default entry not to include the removed one */
	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox)) {
		err = -ENOMEM;
		back_to_list = true;
		goto out_list;
	}
	mgm = mailbox->buf;
	members_count = 0;
	list_for_each_entry(dqp, &s_steer->promisc_qps[steer], list)
		mgm->qp[members_count++] = cpu_to_be32(dqp->qpn & MGM_QPN_MASK);
	mgm->members_count = cpu_to_be32(members_count | MLX4_PROT_ETH << 30);

	err = mlx4_WRITE_PROMISC(dev, vep_num, port, steer, mailbox);
	if (err)
		goto out_mailbox;

	/* remove the qp from all the steering entries*/
	list_for_each_entry(entry, &s_steer->steer_entries[steer], list) {
		found = false;
		list_for_each_entry(dqp, &entry->duplicates, list) {
			if (dqp->qpn == qpn) {
				found = true;
				break;
			}
		}
		if (found) {
			/* a duplicate, no need to change the mgm,
			 * only update the duplicates list */
			list_del(&dqp->list);
			kfree(dqp);
		} else {
			err = mlx4_READ_ENTRY(dev, entry->index, mailbox);
				if (err)
					goto out_mailbox;
			members_count = be32_to_cpu(mgm->members_count) & 0xffffff;
			for (loc = -1, i = 0; i < members_count; ++i)
				if ((be32_to_cpu(mgm->qp[i]) & MGM_QPN_MASK) == qpn)
					loc = i;

			mgm->members_count = cpu_to_be32(--members_count |
							 (MLX4_PROT_ETH << 30));
			mgm->qp[loc] = mgm->qp[i - 1];
			mgm->qp[i - 1] = 0;

			err = mlx4_WRITE_ENTRY(dev, entry->index, mailbox);
				if (err)
					goto out_mailbox;
		}

	}

out_mailbox:
	mlx4_free_cmd_mailbox(dev, mailbox);
out_list:
	if (back_to_list)
		list_add_tail(&pqp->list, &s_steer->promisc_qps[steer]);
out_mutex:
	mutex_unlock(&priv->mcg_table.mutex);
	return err;
}

static int set_num_high_prio(struct mlx4_dev *dev, u8 port, int hash, int val,
			      enum mlx4_protocol prot)
{
	struct mlx4_steer_prio *high_prio, *prev_high_prio;
	struct list_head *high_prios;

	if (prot != MLX4_PROT_ETH || port > dev->caps.num_ports)
		return -EINVAL;

	high_prios = &mlx4_priv(dev)->steer[port - 1].high_prios;
	list_for_each_entry_safe(high_prio, prev_high_prio, high_prios, list) {
		if (high_prio->hash == hash) {
			high_prio->num_high_prio += val;
			if (!high_prio->num_high_prio) {
				list_del(&high_prio->list);
				kfree(high_prio);
			}
			return 0;
		}
	}
	high_prio = kmalloc(sizeof *high_prio, GFP_KERNEL);
	if (!high_prio)
		return -ENOMEM;
	high_prio->hash = hash;
	high_prio->num_high_prio = val;
	list_add_tail(&high_prio->list, high_prios);
	return 0;
}

static int inc_num_high_prio(struct mlx4_dev *dev, u8 port, int hash,
			      enum mlx4_protocol prot)
{
	return set_num_high_prio(dev, port, hash, 1, prot);
}

static int dec_num_high_prio(struct mlx4_dev *dev, u8 port, int hash,
			      enum mlx4_protocol prot)
{
	return set_num_high_prio(dev, port, hash, -1, prot);
}

static u32 get_num_high_prio(struct mlx4_dev *dev, u8 port, int hash,
			     enum mlx4_protocol prot)
{
	struct mlx4_steer_prio *high_prio;
	struct list_head *high_prios;

	if (prot != MLX4_PROT_ETH || port > dev->caps.num_ports)
		return 0;
	high_prios = &mlx4_priv(dev)->steer[port - 1].high_prios;
	list_for_each_entry(high_prio, high_prios, list) {
		if (high_prio->hash == hash)
			return high_prio->num_high_prio;
	}
	return 0;
}

/* Find and return the first MGM/AMGM entry which is not of high priority */
/* TODO merge with find_entry */
static int find_first_low_entry(struct mlx4_dev *dev, u8 port,
				u8 *gid, enum mlx4_protocol prot,
				struct mlx4_cmd_mailbox *mgm_mailbox,
				u16 *hash, int *prev, int *index)
{
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_mgm *mgm = mgm_mailbox->buf;
	u8 *mgid;
	int err;
	u8 op_mod = (prot == MLX4_PROT_ETH) ? !!(dev->caps.vep_mc_steering) : 0;
	int high_prio_left;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return -ENOMEM;
	mgid = mailbox->buf;

	memcpy(mgid, gid, 16);

	err = mlx4_GID_HASH(dev, mailbox, hash, op_mod);
	mlx4_free_cmd_mailbox(dev, mailbox);
	if (err)
		return err;

	*index = *hash;
	*prev  = -1;

	high_prio_left = get_num_high_prio(dev, port, *index, prot);

	do {
		err = mlx4_READ_ENTRY(dev, *index, mgm_mailbox);
		if (err)
			return err;

		if (!(be32_to_cpu(mgm->members_count) & 0xffffff)) {
			if (*index != *hash) {
				mlx4_err(dev, "Found zero MGID in AMGM.\n");
				err = -EINVAL;
			} else if (high_prio_left) {
				mlx4_err(dev, "Invalid high prio entries.\n");
				err = -EINVAL;
			}
			return err;
		}

		if (!high_prio_left)
			return err;

		*prev = *index;
		*index = be32_to_cpu(mgm->next_gid_index) >> 6;
		high_prio_left--;
	} while (*index);
	if (high_prio_left) {
		mlx4_err(dev, "Not enough high prio MGIDs in MGM.\n");
		return -EINVAL;
	}
	*index = -1;
	return err;
}

/*
 * Caller must hold MCG table semaphore.  gid and mgm parameters must
 * be properly aligned for command interface.
 *
 *  Returns 0 unless a firmware command error occurs.
 *
 * If GID is found in MGM or MGM is empty, *index = *hash, *prev = -1
 * and *mgm holds MGM entry.
 *
 * if GID is found in AMGM, *index = index in AMGM, *prev = index of
 * previous entry in hash chain and *mgm holds AMGM entry.
 *
 * If no AMGM exists for given gid, *index = -1, *prev = index of last
 * entry in hash chain and *mgm holds end of hash chain.
 */
static int find_entry(struct mlx4_dev *dev, u8 port,
		      u8 *gid, enum mlx4_protocol prot,
		      enum mlx4_steer_type steer, u8 high_prio,
		      struct mlx4_cmd_mailbox *mgm_mailbox,
		      u16 *hash, int *prev, int *index)
{
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_mgm *mgm = mgm_mailbox->buf;
	u8 *mgid;
	int err;
	u8 op_mod = (prot == MLX4_PROT_ETH) ? !!(dev->caps.vep_mc_steering) : 0;
	int high_prio_left = 0;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return -ENOMEM;
	mgid = mailbox->buf;

	memcpy(mgid, gid, 16);

	err = mlx4_GID_HASH(dev, mailbox, hash, op_mod);
	mlx4_free_cmd_mailbox(dev, mailbox);
	if (err)
		return err;

	if (0)
		mlx4_dbg(dev, "Hash for %pI6 is %04x\n", gid, *hash);

	*index = *hash;
	*prev  = -1;

	/* We distinguish low from high priority entries by keeping the high
	   entries before the low entries and saving their number */

	high_prio_left = get_num_high_prio(dev, port, *hash, prot);

	do {
		err = mlx4_READ_ENTRY(dev, *index, mgm_mailbox);
		if (err)
			return err;

		if (!(be32_to_cpu(mgm->members_count) & 0xffffff)) {
			if (*index != *hash) {
				mlx4_err(dev, "Found zero MGID in AMGM.\n");
				err = -EINVAL;
			}
			return err;
		}

		if (!memcmp(mgm->gid, gid, 16) &&
		    (prot == be32_to_cpu(mgm->members_count) >> 30) &&
		    ((high_prio && high_prio_left > 0) ||
		     (!high_prio && high_prio_left <= 0)))
			return err;

		*prev = *index;
		*index = be32_to_cpu(mgm->next_gid_index) >> 6;
		high_prio_left--;
	} while (*index);

	*index = -1;
	return err;
}

int mlx4_qp_attach_common(struct mlx4_dev *dev, struct mlx4_qp *qp, u8 gid[16],
			  int block_mcast_loopback, enum mlx4_protocol prot,
			  enum mlx4_steer_type steer, u8 high_prio)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_cmd_mailbox *mailbox, *low_mailbox = NULL;
	struct mlx4_mgm *mgm, *low_mgm;
	u32 members_count;
	u16 hash;
	int index, prev;
	int low_index = -1, low_prev;
	int link = 0;
	int i;
	int err;
	u8 port = gid[5];
	u8 new_entry = 0;
	u8 low_to_high = 0;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	mgm = mailbox->buf;

	mutex_lock(&priv->mcg_table.mutex);
	err = find_entry(dev, port, gid, prot, steer, high_prio,
			 mailbox, &hash, &prev, &index);
	if (err)
		goto out;
	if (index != -1) {
		if (!(be32_to_cpu(mgm->members_count) & 0xffffff)) {
			new_entry = 1;
			memcpy(mgm->gid, gid, 16);
		}
	} else {
		link = 1;
		new_entry = 1;

		index = mlx4_bitmap_alloc(&priv->mcg_table.bitmap);
		if (index == -1) {
			mlx4_warn(dev, "No AMGM entries left\n");
			err = -ENOMEM;
			goto out;
		}
		index += dev->caps.num_mgms;

		memset(mgm, 0, sizeof *mgm);
		memcpy(mgm->gid, gid, 16);

		/* if High priority attach was requested, we want to keep it
		   before low entries, so we take the first low priority entry
		   and swap it for our newly created entry (which is last) */
		if (high_prio) {
			mlx4_dbg(dev, "High priority steer was requested\n");
			low_mailbox = mlx4_alloc_cmd_mailbox(dev);
			if (IS_ERR(low_mailbox)) {
				err = PTR_ERR(low_mailbox);
				low_mailbox = NULL;
				goto out;
			}
			err = find_first_low_entry(dev, port, gid, prot,
						   low_mailbox, &hash,
						   &low_prev, &low_index);
			if (!err && low_index != -1) {
				low_to_high = 1;
				low_mgm = low_mailbox->buf;
				mlx4_dbg(dev, "Found a low prio steering entry. Switching entries\n");
				memcpy(mgm->gid, low_mgm->gid, 16);
				mgm->members_count = low_mgm->members_count;
				memcpy(mgm->qp, low_mgm->qp,
				       (be32_to_cpu(mgm->members_count) & 0xffffff) * sizeof low_mgm->qp[0]);
				low_mgm->members_count = 0;
				memcpy(low_mgm->gid, gid, 16);
				err = mlx4_WRITE_ENTRY(dev, index, mailbox);
				if (err)
					goto out;
				adjust_steering_entry(dev, 0, port, steer, low_index, index);
			}
		}
	}

	if (!low_to_high) {
		low_mgm = mgm;
		low_index = index;
		low_mailbox = mailbox;
	}

	members_count = be32_to_cpu(low_mgm->members_count) & 0xffffff;
	if (members_count == MLX4_QP_PER_MGM) {
		mlx4_err(dev, "MGM at index %x is full.\n", low_index);
		err = -ENOMEM;
		goto out;
	}

	for (i = 0; i < members_count; ++i)
		if ((be32_to_cpu(low_mgm->qp[i]) & MGM_QPN_MASK) == qp->qpn) {
			mlx4_dbg(dev, "QP %06x already a member of MGM\n", qp->qpn);
			err = 0;
			goto out;
		}

	low_mgm->qp[members_count++] = cpu_to_be32((qp->qpn & MGM_QPN_MASK) |
						   (!!mlx4_blck_lb << MGM_BLCK_LB_BIT));

	low_mgm->members_count = cpu_to_be32(members_count | ((u32) prot << 30));

	err = mlx4_WRITE_ENTRY(dev, low_index, low_mailbox);
	if (err)
		goto out;

	if (!link)
		goto out;

	err = mlx4_READ_ENTRY(dev, prev, mailbox);
	if (err)
		goto out;

	mgm->next_gid_index = cpu_to_be32(index << 6);

	err = mlx4_WRITE_ENTRY(dev, prev, mailbox);
	if (err)
		goto out;
	if (high_prio)
		err = inc_num_high_prio(dev, port, hash, prot);
	if (err)
		goto out;

out:
	if (prot == MLX4_PROT_ETH) {
		/* manage the steering entry for promisc mode */
		if (new_entry)
			new_steering_entry(dev, 0, port, steer, low_index, qp->qpn);
		else
			existing_steering_entry(dev, 0, port, steer,
						low_index, qp->qpn);
		/* TODO handle an error flow here, need to clean the MGMS */
	}
	if (err && link && index != -1) {
		if (index < dev->caps.num_mgms)
			mlx4_warn(dev, "Got AMGM index %d < %d",
				  index, dev->caps.num_mgms);
		else
			mlx4_bitmap_free(&priv->mcg_table.bitmap,
					 index - dev->caps.num_mgms);
	}
	mutex_unlock(&priv->mcg_table.mutex);

	mlx4_free_cmd_mailbox(dev, mailbox);
	if (low_to_high && low_mailbox)
		mlx4_free_cmd_mailbox(dev, low_mailbox);
	return err;
}

int mlx4_qp_detach_common(struct mlx4_dev *dev, struct mlx4_qp *qp, u8 gid[16],
			  enum mlx4_protocol prot, enum mlx4_steer_type steer,
			  u8 high_prio)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_mgm *mgm;
	u32 members_count;
	u16 hash;
	int prev, index;
	int i, loc;
	int err;
	u8 port = gid[5];
	bool removed_entry = false;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	mgm = mailbox->buf;

	mutex_lock(&priv->mcg_table.mutex);

	err = find_entry(dev, port, gid, prot, steer, high_prio,
			 mailbox, &hash, &prev, &index);
	if (err)
		goto out;

	if (index == -1) {
		mlx4_err(dev, "MGID %pI6 not found\n", gid);
		err = -EINVAL;
		goto out;
	}

	/* if this pq is also a promisc qp, it shouldn't be removed */
	if (prot == MLX4_PROT_ETH &&
	    check_duplicate_entry(dev, 0, port, steer, index, qp->qpn))
		goto out;

	members_count = be32_to_cpu(mgm->members_count) & 0xffffff;
	for (loc = -1, i = 0; i < members_count; ++i)
		if ((be32_to_cpu(mgm->qp[i]) & MGM_QPN_MASK) == qp->qpn)
			loc = i;

	if (loc == -1) {
		mlx4_err(dev, "QP %06x not found in MGM\n", qp->qpn);
		err = -EINVAL;
		goto out;
	}


	mgm->members_count = cpu_to_be32(--members_count | ((u32) prot << 30));
	mgm->qp[loc]       = mgm->qp[i - 1];
	mgm->qp[i - 1]     = 0;

	
	if (prot == MLX4_PROT_ETH)
		removed_entry = can_remove_steering_entry(dev, 0, port, steer, index, qp->qpn);
	if (i != 1 && (prot != MLX4_PROT_ETH || !removed_entry)) {
		err = mlx4_WRITE_ENTRY(dev, index, mailbox);
		goto out;
	}

	/* We are going to delete the entry, members count should be 0 */
	mgm->members_count = cpu_to_be32((u32) prot << 30);

	if (prev == -1) {
		/* Remove entry from MGM */
		int amgm_index = be32_to_cpu(mgm->next_gid_index) >> 6;
		if (amgm_index) {
			err = mlx4_READ_ENTRY(dev, amgm_index, mailbox);
			if (!memcmp(mgm->gid, gid, 16))
				adjust_steering_entry(dev, 0, port, steer, amgm_index, index);
			if (err)
				goto out;
		} else
			memset(mgm->gid, 0, 16);

		err = mlx4_WRITE_ENTRY(dev, index, mailbox);
		if (err)
			goto out;

		if (amgm_index) {
			if (amgm_index < dev->caps.num_mgms)
				mlx4_warn(dev, "MGM entry %d had AMGM index %d < %d",
					  index, amgm_index, dev->caps.num_mgms);
			else
				mlx4_bitmap_free(&priv->mcg_table.bitmap,
						 amgm_index - dev->caps.num_mgms);
		}
	} else {
		/* Remove entry from AMGM */
		int cur_next_index = be32_to_cpu(mgm->next_gid_index);
		err = mlx4_READ_ENTRY(dev, prev, mailbox);
		if (err)
			goto out;

		mgm->next_gid_index = cpu_to_be32(cur_next_index);

		err = mlx4_WRITE_ENTRY(dev, prev, mailbox);
		if (err)
			goto out;

		if (index < dev->caps.num_mgms)
			mlx4_warn(dev, "entry %d had next AMGM index %d < %d",
				  prev, index, dev->caps.num_mgms);
		else
			mlx4_bitmap_free(&priv->mcg_table.bitmap,
					 index - dev->caps.num_mgms);
	}
	if (high_prio)
		dec_num_high_prio(dev, port, hash, prot);
out:
	mutex_unlock(&priv->mcg_table.mutex);

	mlx4_free_cmd_mailbox(dev, mailbox);

	return err;
}



int mlx4_multicast_attach(struct mlx4_dev *dev, struct mlx4_qp *qp, u8 gid[16],
			  int block_mcast_loopback, enum mlx4_protocol prot,
			  u8 high_prio)
{
	enum mlx4_steer_type steer;

	steer = MLX4_UC_STEER;

	if (prot == MLX4_PROT_ETH && !dev->caps.vep_mc_steering)
		return 0;

	if (prot == MLX4_PROT_ETH)
		gid[7] |= (steer << 1);

	return mlx4_qp_attach_common(dev, qp, gid,
				     block_mcast_loopback, prot,
				     steer, high_prio);
}
EXPORT_SYMBOL_GPL(mlx4_multicast_attach);

int mlx4_multicast_detach(struct mlx4_dev *dev, struct mlx4_qp *qp, u8 gid[16],
			  enum mlx4_protocol prot, u8 high_prio)
{
	enum mlx4_steer_type steer;

	steer = MLX4_UC_STEER;

	if (prot == MLX4_PROT_ETH && !dev->caps.vep_mc_steering)
		return 0;

	if (prot == MLX4_PROT_ETH) {
		gid[7] |= (steer << 1);
	}

	return mlx4_qp_detach_common(dev, qp, gid, prot, steer, high_prio);
}
EXPORT_SYMBOL_GPL(mlx4_multicast_detach);


int mlx4_multicast_promisc_add(struct mlx4_dev *dev, u32 qpn, u8 port)
{
	if (!dev->caps.vep_mc_steering)
		return 0;


	return add_promisc_qp(dev, 0, port, MLX4_MC_STEER, qpn);
}
EXPORT_SYMBOL_GPL(mlx4_multicast_promisc_add);

int mlx4_multicast_promisc_remove(struct mlx4_dev *dev, u32 qpn, u8 port)
{
	if (!dev->caps.vep_mc_steering)
		return 0;


	return remove_promisc_qp(dev, 0, port, MLX4_MC_STEER, qpn);
}
EXPORT_SYMBOL_GPL(mlx4_multicast_promisc_remove);

int mlx4_unicast_promisc_add(struct mlx4_dev *dev, u32 qpn, u8 port)
{
	if (!dev->caps.vep_mc_steering)
		return 0;


	return add_promisc_qp(dev, 0, port, MLX4_UC_STEER, qpn);
}
EXPORT_SYMBOL_GPL(mlx4_unicast_promisc_add);

int mlx4_unicast_promisc_remove(struct mlx4_dev *dev, u32 qpn, u8 port)
{
	if (!dev->caps.vep_mc_steering)
		return 0;

	return remove_promisc_qp(dev, 0, port, MLX4_UC_STEER, qpn);
}
EXPORT_SYMBOL_GPL(mlx4_unicast_promisc_remove);

int mlx4_init_mcg_table(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int err;

	err = mlx4_bitmap_init(&priv->mcg_table.bitmap, dev->caps.num_amgms,
			       dev->caps.num_amgms - 1, 0, 0);
	if (err)
		return err;

	mutex_init(&priv->mcg_table.mutex);

	return 0;
}

void mlx4_cleanup_mcg_table(struct mlx4_dev *dev)
{
	mlx4_bitmap_cleanup(&mlx4_priv(dev)->mcg_table.bitmap);
}
