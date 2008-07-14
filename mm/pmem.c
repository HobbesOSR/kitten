#include <lwk/kernel.h>
#include <lwk/string.h>
#include <lwk/list.h>
#include <lwk/pmem.h>

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
	unsigned long size;

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
		*result = *rgn;
		calc_overlap(query, rgn, result);
		return 0;
	}

	return -ENOENT;
}

void
pmem_region_unset_all(struct pmem_region *rgn)
{
	rgn->type_is_set      = false;
	rgn->lgroup_is_set    = false;
	rgn->allocated_is_set = false;
	rgn->name_is_set      = false;
}

static const char *
pmem_type_to_string(int type)
{
	switch(type) {
		case PMEM_TYPE_BOOTMEM:  return "BOOTMEM"; break;
		case PMEM_TYPE_KMEM:     return "KERNEL"; break;
		case PMEM_TYPE_UMEM:     return "USER"; break;
	}
	return "UNKNOWN";
}

void
pmem_dump(void)
{
	struct pmem_list_entry *entry;
	struct pmem_region *rgn;

	printk(KERN_DEBUG "PHYSICAL MEMORY MAP:\n");
	list_for_each_entry(entry, &pmem_list, link) {
		rgn = &entry->rgn;
		printk(KERN_DEBUG
			"  [0x%016lx, 0x%016lx) %s\n",
			rgn->start, rgn->end,
			(rgn->type_is_set)
				? pmem_type_to_string(rgn->type) : "NOT SET"
		);
	}
}
