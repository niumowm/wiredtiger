/*-
 * Copyright (c) 2008-2013 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

static void __free_page_modify(WT_SESSION_IMPL *, WT_PAGE *);
static void __free_page_col_int(WT_SESSION_IMPL *, WT_PAGE *);
static void __free_page_col_var(WT_SESSION_IMPL *, WT_PAGE *);
static void __free_page_row_int(WT_SESSION_IMPL *, WT_PAGE *);
static void __free_page_row_leaf(WT_SESSION_IMPL *, WT_PAGE *);
static void __free_skip_array(WT_SESSION_IMPL *, WT_INSERT_HEAD **, uint32_t);
static void __free_skip_list(WT_SESSION_IMPL *, WT_INSERT *);
static void __free_update(WT_SESSION_IMPL *, WT_UPDATE **, uint32_t);
static void __free_update_list(WT_SESSION_IMPL *, WT_UPDATE *);

/*
 * __wt_page_out --
 *	Discard an in-memory page, freeing all memory associated with it.
 */
void
__wt_page_out(WT_SESSION_IMPL *session, WT_PAGE **pagep)
{
	WT_PAGE *page;

	/*
	 * When a page is discarded, it's been disconnected from its parent and
	 * its parent's WT_REF structure may now point to a different page.
	 * Make sure we don't accidentally use the page itself or any other
	 * information.
	 */
	page = *pagep;
	*pagep = NULL;
	page->parent = NULL;
	page->ref = NULL;

	WT_ASSERT(session, !F_ISSET_ATOMIC(page, WT_PAGE_EVICT_LRU));

#ifdef HAVE_DIAGNOSTIC
	{
	WT_HAZARD *hp;
	if ((hp = __wt_page_hazard_check(session, page)) != NULL)
		__wt_errx(session,
		    "discarded page has hazard pointer: (%p: %s, line %d)",
		    hp->page, hp->file, hp->line);
	}
#endif
	/* Update the cache's information. */
	__wt_cache_page_evict(session, page);

	/* Free the page modification information. */
	if (page->modify != NULL)
		__free_page_modify(session, page);

	switch (page->type) {
	case WT_PAGE_COL_FIX:
		break;
	case WT_PAGE_COL_INT:
		__free_page_col_int(session, page);
		break;
	case WT_PAGE_COL_VAR:
		__free_page_col_var(session, page);
		break;
	case WT_PAGE_ROW_INT:
		__free_page_row_int(session, page);
		break;
	case WT_PAGE_ROW_LEAF:
		__free_page_row_leaf(session, page);
		break;
	}

	/* Free any allocated disk image. */
	if (!F_ISSET_ATOMIC(page, WT_PAGE_DISK_NOT_ALLOC))
		__wt_free(session, page->dsk);

	__wt_overwrite_and_free(session, page);
}

/*
 * __free_page_modify --
 *	Discard the page's associated modification structures.
 */
static void
__free_page_modify(WT_SESSION_IMPL *session, WT_PAGE *page)
{
	WT_INSERT_HEAD *append;
	WT_PAGE_MODIFY *mod;

	mod = page->modify;

	switch (F_ISSET(mod, WT_PM_REC_MASK)) {
	case WT_PM_REC_SPLIT:
		/*
		 * If the page split, there may one or more pages linked from
		 * the page; walk the list, discarding pages.
		 */
		__wt_page_out(session, &mod->u.split);
		break;
	case WT_PM_REC_REPLACE:
		/*
		 * Discard any replacement address: this memory is usually moved
		 * into the parent's WT_REF, but at the root that can't happen.
		 */
		__wt_free(session, mod->u.replace.addr);
		break;
	default:
		break;
	}

	/* Free the append array. */
	if ((append = WT_COL_APPEND(page)) != NULL) {
		__free_skip_list(session, WT_SKIP_FIRST(append));
		__wt_free(session, append);
		__wt_free(session, mod->append);
	}

	/* Free the insert/update array. */
	if (mod->update != NULL)
		__free_skip_array(session, mod->update,
		    page->type == WT_PAGE_COL_FIX ? 1 : page->entries);

	/* Discard any objects the page was tracking plus associated memory. */
	__wt_rec_track_discard(session, page);
	__wt_free(session, mod->track);
	__wt_free(session, page->modify);
}

/*
 * __free_page_col_int --
 *	Discard a WT_PAGE_COL_INT page.
 */
static void
__free_page_col_int(WT_SESSION_IMPL *session, WT_PAGE *page)
{
	WT_REF *ref;
	uint32_t i;

	/*
	 * For each referenced addr, see if the addr was an allocation, and if
	 * so, free it.
	 */
	WT_REF_FOREACH(page, ref, i)
		if (ref->addr != NULL &&
		    __wt_off_page(page, ref->addr)) {
			__wt_free(session, ((WT_ADDR *)ref->addr)->addr);
			__wt_free(session, ref->addr);
		}

	/* Free the subtree-reference array. */
	__wt_free(session, page->u.intl.t);
}

/*
 * __free_page_col_var --
 *	Discard a WT_PAGE_COL_VAR page.
 */
static void
__free_page_col_var(WT_SESSION_IMPL *session, WT_PAGE *page)
{
	/* Free the in-memory index array. */
	__wt_free(session, page->u.col_var.d);

	/* Free the RLE lookup array. */
	__wt_free(session, page->u.col_var.repeats);
}

/*
 * __free_page_row_int --
 *	Discard a WT_PAGE_ROW_INT page.
 */
static void
__free_page_row_int(WT_SESSION_IMPL *session, WT_PAGE *page)
{
	WT_IKEY *ikey;
	WT_REF *ref;
	uint32_t i;

	/*
	 * Free any allocated keys.
	 *
	 * For each referenced addr, see if the addr was an allocation, and if
	 * so, free it.
	 */
	WT_REF_FOREACH(page, ref, i) {
		if ((ikey = ref->u.key) != NULL)
			__wt_free(session, ikey);
		if (ref->addr != NULL &&
		    __wt_off_page(page, ref->addr)) {
			__wt_free(session, ((WT_ADDR *)ref->addr)->addr);
			__wt_free(session, ref->addr);
		}
	}

	/* Free the subtree-reference array. */
	__wt_free(session, page->u.intl.t);
}

/*
 * __free_page_row_leaf --
 *	Discard a WT_PAGE_ROW_LEAF page.
 */
static void
__free_page_row_leaf(WT_SESSION_IMPL *session, WT_PAGE *page)
{
	WT_IKEY *ikey;
	WT_ROW *rip;
	uint32_t i;

	/*
	 * Free the in-memory index array.
	 *
	 * For each entry, see if the key was an allocation (that is, if it
	 * points somewhere other than the original page), and if so, free
	 * the memory.
	 */
	WT_ROW_FOREACH(page, rip, i) {
		ikey = WT_ROW_KEY_COPY(rip);
		if (ikey != NULL && __wt_off_page(page, ikey))
			__wt_free(session, ikey);
	}
	__wt_free(session, page->u.row.d);

	/*
	 * Free the insert array.
	 *
	 * Row-store tables have one additional slot in the insert array (the
	 * insert array has an extra slot to hold keys that sort before keys
	 * found on the original page).
	 */
	if (page->u.row.ins != NULL)
		__free_skip_array(session, page->u.row.ins, page->entries + 1);

	/* Free the update array. */
	if (page->u.row.upd != NULL)
		__free_update(session, page->u.row.upd, page->entries);
}

/*
 * __free_skip_array --
 *	Discard an array of skip list headers.
 */
static void
__free_skip_array(
    WT_SESSION_IMPL *session, WT_INSERT_HEAD **head_arg, uint32_t entries)
{
	WT_INSERT_HEAD **head;

	/*
	 * For each non-NULL slot in the page's array of inserts, free the
	 * linked list anchored in that slot.
	 */
	for (head = head_arg; entries > 0; --entries, ++head)
		if (*head != NULL) {
			__free_skip_list(session, WT_SKIP_FIRST(*head));
			__wt_free(session, *head);
		}

	/* Free the header array. */
	__wt_free(session, head_arg);
}

/*
 * __free_skip_list --
 *	Walk a WT_INSERT forward-linked list and free the per-thread combination
 * of a WT_INSERT structure and its associated chain of WT_UPDATE structures.
 */
static void
__free_skip_list(WT_SESSION_IMPL *session, WT_INSERT *ins)
{
	WT_INSERT *next;

	do {
		__free_update_list(session, ins->upd);

		next = WT_SKIP_NEXT(ins);
		__wt_free(session, ins);
	} while ((ins = next) != NULL);
}

/*
 * __free_update --
 *	Discard the update array.
 */
static void
__free_update(
    WT_SESSION_IMPL *session, WT_UPDATE **update_head, uint32_t entries)
{
	WT_UPDATE **updp;

	/*
	 * For each non-NULL slot in the page's array of updates, free the
	 * linked list anchored in that slot.
	 */
	for (updp = update_head; entries > 0; --entries, ++updp)
		if (*updp != NULL)
			__free_update_list(session, *updp);

	/* Free the update array. */
	__wt_free(session, update_head);
}

/*
 * __free_update_list --
 *	Walk a WT_UPDATE forward-linked list and free the per-thread combination
 *	of a WT_UPDATE structure and its associated data.
 */
static void
__free_update_list(WT_SESSION_IMPL *session, WT_UPDATE *upd)
{
	WT_UPDATE *next;

	do {
		next = upd->next;
		__wt_free(session, upd);
	} while ((upd = next) != NULL);
}
