/*-
 * Public Domain 2008-2013 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "thread.h"

/*
 * stats
 *	Dump the database/file statistics.
 */
void
stats(void)
{
	WT_CURSOR *cursor;
	uint64_t v;
	const char *pval, *desc;
	WT_SESSION *session;
	FILE *fp;
	int ret;

	if ((ret = conn->open_session(conn, NULL, NULL, &session)) != 0)
		die("conn.session", ret);

	if ((fp = fopen(FNAME_STAT, "w")) == NULL)
		die("fopen " FNAME_STAT , errno);

	/* Connection statistics. */
	if ((ret = session->open_cursor(session,
	    "statistics:", NULL, NULL, &cursor)) != 0)
		die("session.open_cursor", ret);

	while ((ret = cursor->next(cursor)) == 0 &&
	    (ret = cursor->get_value(cursor, &desc, &pval, &v)) == 0)
		(void)fprintf(fp, "%s=%s\n", desc, pval);

	if (ret != WT_NOTFOUND)
		die("cursor.next", ret);
	if ((ret = cursor->close(cursor)) != 0)
		die("cursor.close", ret);

	/* File statistics. */
	if ((ret = session->open_cursor(session,
	    "statistics:" FNAME, NULL, NULL, &cursor)) != 0)
		die("session.open_cursor", ret);

	while ((ret = cursor->next(cursor)) == 0 &&
	    (ret = cursor->get_value(cursor, &desc, &pval, &v)) == 0)
		(void)fprintf(fp, "%s=%s\n", desc, pval);

	if (ret != WT_NOTFOUND)
		die("cursor.next", ret);
	if ((ret = cursor->close(cursor)) != 0)
		die("cursor.close", ret);

	if ((ret = session->close(session, NULL)) != 0)
		die("session.close", ret);

	(void)fclose(fp);
}
