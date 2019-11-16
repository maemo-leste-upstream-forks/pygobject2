import unittest
import sys

import glib


class TestGObject(unittest.TestCase):
    def test_glib_enum(self):
        '''GLib enum'''
        self.assertEqual(glib.IO_IN, 1)

    def test_glib_flag(self):
        '''GLib flag'''
        self.assertEqual(glib.IO_FLAG_IS_READABLE, 4)

    def test_method(self):
        '''GLib method call'''

        self.assertIn(glib.find_program_in_path('bash'), ('/bin/bash', '/usr/bin/bash'))


unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
