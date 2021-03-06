#!/usr/bin/env python
#
# Public Domain 2008-2013 WiredTiger, Inc.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# test_bug006.py
#       Regression tests.

import wiredtiger, wttest
from helper import key_populate, value_populate

# Check that verify and salvage both raise exceptions if there is an open
# cursor.
class test_bug006(wttest.WiredTigerTestCase):
    name = 'test_bug006'
    scenarios = [
        ('file', dict(uri='file:')),
        ('table', dict(uri='table:')),
    ]

    def test_bug006(self):
        uri = self.uri + self.name
        self.session.create(uri, 'value_format=S,key_format=S')
        cursor = self.session.open_cursor(uri, None)
        for i in range(1, 1000):
            cursor.set_key(key_populate(cursor, i))
            cursor.set_value(value_populate(cursor, i))
            cursor.insert()

        # Table operations should fail, the cursor is closed.
        self.assertRaises(
            wiredtiger.WiredTigerError, lambda: self.session.drop(uri, None))
        self.assertRaises(
            wiredtiger.WiredTigerError,
            lambda: self.session.rename(uri, self.uri + "new", None))
        self.assertRaises(
            wiredtiger.WiredTigerError, lambda: self.session.salvage(uri, None))
        self.assertRaises(
            wiredtiger.WiredTigerError,
            lambda: self.session.truncate(uri, None, None, None))
        self.assertRaises(
            wiredtiger.WiredTigerError, lambda: self.session.upgrade(uri, None))
        self.assertRaises(
            wiredtiger.WiredTigerError, lambda: self.session.verify(uri, None))

        cursor.close()

        # Table operations should succeed, the cursor is closed.
        self.session.rename(uri, self.uri + "new", None)
        self.session.rename(self.uri + "new", uri, None)
        self.session.salvage(uri, None)
        self.session.truncate(uri, None, None, None)
        self.session.upgrade(uri, None)
        self.session.verify(uri, None)

        self.session.drop(uri, None)


if __name__ == '__main__':
    wttest.run()
