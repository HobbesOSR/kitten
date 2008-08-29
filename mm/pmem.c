/* Copyright (c) 2008, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/string.h>
#include <lwk/list.h>
#include <lwk/log2.h>
#include <lwk/pmem.h>
#include <arch/uaccess.h>

static LIST_HEAD(pmem_list);

struct pmem_list_entry {
	struct list_head	link;
	struct pmem_region	rgn;
};

static struct pmem_list_entry *
alloc_pmem_list_entry(void)
{
	return kmem_alloc(sizeof(struct pmem_list_entry));
}

static void
free_pmem_list_entry(struct pmem_list_entry *entry)
{
	kmem_free(entry);
}

static bool
calc_overlap(const struct pmem_region *a, const struct pmem_region *b,
             struct pmem_region *dst)
{
	if (!((a->start < b->end) && (a->end > b->start)))
		return false;

	if (dst) {
		dst->start = max(a->start, b->start);
		dst->end   = min(a->end, b->end);
	}

	return true;
}

static bool
regions_overlap(const struct pmem_region *a, const struct pmem_region *b)
{
	return calc_overlap(a, b, NULL);
}

static bool
region_is_unique(const struct pmem_region *rgn)
{
	struct pmem_list_entry *entry;

	list_for_each_entry(entry, &pmem_list, link) {
		if (regions_overlap(rgn, &entry->rgn))
			return false;
	}
	return true;
}

static bool
region_is_sane(const struct pmem_region *rgn)
{
	int i;

	if (!rgn)
		return false;

	if (rgn->end <= rgn->start)
		return false;

	for (i = 0; i < sizeof(rgn->name); i++)  {
		if (rgn->name[i] == '\0')
			break;
	}
	if (i == sizeof(rgn->name))
		return false;

	return true;
}

static bool
region_is_known(const struct pmem_region *rgn)
{
	struct pmem_list_entry *entry;
	struct pmem_region overlap;
	size_t size;

	size = rgn->end - rgn->start;
	list_for_each_entry(entry, &pmem_list, link) {
		if (!calc_overlap(rgn, &entry->rgn, &overlap))
			continue;

		size -= (overlap.end - overlap.start);
	}

	return (size == 0) ? true : false;
}

static void
insert_pmem_list_entry(struct pmem_list_entry *entry)
{
	struct list_head *pos;
	struct pmem_list_entry *cur;

	/* Locate the entry that the new entry should be inserted before */
	list_for_each(pos, &pmem_list) {
		cur = list_entry(pos, struct pmem_list_entry, link);
		if (cur->rgn.start > entry->rgn.start)
			break;
	}
	list_add_tail(&entry->link, pos);
}

static bool
regions_are_mergeable(const struct pmem_region *a, const struct pmem_region *b)
{
	if ((a->end != b->start) && (b->end != a->start))
		return false;

	if (a->type_is_set != b->type_is_set)
		return false;
	if (a->type_is_set && (a->type != b->type))
		return false;

	if (a->lgroup_is_set != b->lgroup_is_set)
		return false;
	if (a->lgroup_is_set && (a->lgroup != b->lgroup))
		return false;

	if (a->allocated_is_set != b->allocated_is_set)
		return false;
	if (a->allocated_is_set && (a->allocated != b->allocated))
		return false;

	if (a->name_is_set != b->name_is_set)
		return false;
	if (a->name_is_set && !strcmp(a->name, b->name))
		return false;

	return true;
}

static bool
region_matches(const struct pmem_region *query, const struct pmem_region *rgn)
{
	if (!regions_overlap(query, rgn))
		return false;

	if (query->type_is_set
	      && (!rgn->type_is_set || (rgn->type != query->type)))
		return false;

	if (query->lgroup_is_set
	      && (!rgn->lgroup_is_set || (rgn->lgroup != query->lgroup)))
		return false;

	if (query->allocated_is_set
	      && (!rgn->allocated_is_set || (rgn->allocated != query->allocated)))
		return false;

	if (query->name_is_set
	      && (!rgn->name_is_set || strcmp(rgn->name, query->name)))
		return false;

	return true;
}

static void
merge_pmem_list(void)
{
	struct pmem_list_entry *entry, *prev, *tmp;

	prev = NULL;
	list_for_each_entry_safe(entry, tmp, &pmem_list, link) {
		if (prev && regions_are_mergeable(&prev->rgn, &entry->rgn)) {
			prev->rgn.end = entry->rgn.end;
			list_del(&entry->link);
			free_pmem_list_entry(entry);
		} else {
			prev = entry;
		}
	}
}

static void
zero_pmem(const struct pmem_region *rgn)
{
	/* access pmem region via the kernel's identity map */
	memset(__va(rgn->start), 0, rgn->end - rgn->start);
}

int
pmem_add(const struct pmem_region *rgn)
{
	struct pmem_list_entry *entry;

	if (!region_is_sane(rgn))
		return -EINVAL;

	if (!region_is_unique(rgn))
		return -EEXIST;

	if (!(entry = alloc_pmem_list_entry()))
		return -ENOMEM;
	
	entry->rgn = *rgn;

	insert_pmem_list_entry(entry);
	merge_pmem_list();

	return 0;
}

int
sys_pmem_add(const struct pmem_region __user *rgn)
{
	struct pmem_region _rgn;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_rgn, rgn, sizeof(_rgn)))
		return -EINVAL;

	return pmem_add(&_rgn);
}

int
pmem_update(const struct pmem_region *update)
{
	struct pmem_list_entry *entry, *head, *tail;
	struct pmem_region overlap;

	if (!region_is_sane(update))
		return -EINVAL;

	if (!region_is_known(update))
		return -ENOENT;

	list_for_each_entry(entry, &pmem_list, link) {
		if (!calc_overlap(update, &entry->rgn, &overlap))
			continue;

		/* Handle head of entry non-overlap */
		if (entry->rgn.start < overlap.start) {
			if (!(head = alloc_pmem_list_entry()))
				return -ENOMEM;
			head->rgn = entry->rgn;
			head->rgn.end = overlap.start;
			list_add_tail(&head->link, &entry->link);
		}

		/* Handle tail of entry non-overlap */
		if (entry->rgn.end > overlap.end) {
			if (!(tail = alloc_pmem_list_entry()))
				return -ENOMEM;
			tail->rgn = entry->rgn;
			tail->rgn.start = overlap.end;
			list_add(&tail->link, &entry->link);
		}

		/* Update entry to reflect the overlap */
		entry->rgn = *update;
		entry->rgn.start = overlap.start;
		entry->rgn.end   = overlap.end;
	}

	merge_pmem_list();

	return 0;
}

int
sys_pmem_update(const struct pmem_region __user *update)
{
	struct pmem_region _update;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_update, update, sizeof(_update)))
		return -EINVAL;

	return pmem_update(&_update);
}

int
pmem_query(const struct pmem_region *query, struct pmem_region *result)
{
	struct pmem_list_entry *entry;
	struct pmem_region *rgn;

	if (!region_is_sane(query))
		return -EINVAL;

	list_for_each_entry(entry, &pmem_list, link) {
		rgn = &entry->rgn;
		if (!region_matches(query, rgn))
			continue;

		/* match found, update result */
		if (result) {
			*result = *rgn;
			calc_overlap(query, rgn, result);
		}
		return 0;
	}

	return -ENOENT;
}

int
sys_pmem_query(const struct pmem_region __user *query,
               struct pmem_region __user *result)
{
	struct pmem_region _query, _result;
	int status;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_query, query, sizeof(_query)))
		return -EINVAL;

	if ((status = pmem_query(&_query, &_result)) != 0)
		return status;

	if (result && copy_to_user(result, &_result, sizeof(*result)))
		return -EINVAL;

	return 0;
}

int
pmem_alloc(size_t size, size_t alignment,
           const struct pmem_region *constraint,
           struct pmem_region *result)
{
	int status;
	struct pmem_region query;
	struct pmem_region candidate;

	if (size == 0)
		return -EINVAL;

	if (alignment && !is_power_of_2(alignment))
		return -EINVAL;

	if (!region_is_sane(constraint))
		return -EINVAL;

	query = *constraint;

	while ((status = pmem_query(&query, &candidate)) == 0) {
		if (alignment) {
			candidate.start = round_up(candidate.start, alignment);
			if (candidate.start >= candidate.end)
				continue;
		}

		if ((candidate.end - candidate.start) >= size) {
			candidate.end = candidate.start + size;
			candidate.allocated_is_set = true;
			candidate.allocated = true;
			status = pmem_update(&candidate);
			BUG_ON(status);
			zero_pmem(&candidate);
			if (result)
				*result = candidate;
			return 0;
		}

		query.start = candidate.end;
	}
	BUG_ON(status != -ENOENT);

	return -ENOMEM;
}

int
sys_pmem_alloc(size_t size, size_t alignment,
               const struct pmem_region __user *constraint,
               struct pmem_region __user *result)
{
	struct pmem_region _constraint, _result;
	int status;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_constraint, constraint, sizeof(_constraint)))
		return -EINVAL;

	if ((status = pmem_alloc(size, alignment, &_constraint, &_result)) != 0)
		return status;

	if (result && copy_to_user(result, &_result, sizeof(*result)))
		return -EINVAL;

	return 0;
}
