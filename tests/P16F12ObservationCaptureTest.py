"""Synthetic collector qualification; not REAPER acquisition certification."""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/reaper'))
from observation_capture import EventCapture

EVENTS = [dict(boundary=b, ordinal=i//2, stable_order=i%2,
               track=i%2, host_sample=b+26, ppq=i/32)
          for i, b in enumerate([0, 0, 704, 704, 1376, 1376])]


class Qualification(unittest.TestCase):
    def capture(self, capacity=10):
        c = EventCapture('new-session', 'pid/instance/guid', EVENTS, capacity)
        c.start()
        return c

    def test_order_timestamps_count_repeat_and_callback_grouping(self):
        for repeat in range(2):
            for grouping in (1, 2, 3, 6):
                c = self.capture()
                for i in range(0, len(EVENTS), grouping):
                    end = min(len(EVENTS), i+grouping)
                    c.append(c.session, c.instance, end, EVENTS[i:end])
                    c.append(c.session, c.instance, end, EVENTS[i:end])
                result = c.stop(render_complete=True)
                self.assertEqual(result['status'], 'PASS')
                self.assertEqual(result['events'], EVENTS)

    def test_one_event_and_counter_gaps_never_pass(self):
        c = self.capture()
        c.append(c.session, c.instance, 6, EVENTS[:1])
        r = c.stop(render_complete=True)
        self.assertEqual(r['status'], 'BLOCKED')
        self.assertEqual(r['dropped_or_unobserved'], 5)

    def test_timeout_preserves_records_and_waits_for_render(self):
        for timeout, complete in ((True, False), (False, False), (True, True)):
            c = self.capture()
            c.append(c.session, c.instance, 6, EVENTS)
            r = c.stop(render_complete=complete, timed_out=timeout)
            self.assertEqual(r['status'], 'BLOCKED')
            self.assertEqual(r['events'], EVENTS)

    def test_identity_and_stale_session_reject(self):
        for session, instance in (('old-session', 'pid/instance/guid'),
                                  ('new-session', 'wrong-instance')):
            c = self.capture()
            c.append(session, instance, 6, EVENTS)
            self.assertEqual(c.stop(render_complete=True)['observed'], 0)

    def test_capacity_is_explicit_and_truncation_blocks(self):
        c = EventCapture('s', 'i', [], 2)
        c.start()
        c.append('s', 'i', 6, EVENTS)
        r = c.stop(render_complete=True)
        self.assertTrue(r['truncated'])
        self.assertEqual(r['observed'], 2)
        self.assertEqual(r['status'], 'BLOCKED')

    def test_reordered_events_do_not_pass(self):
        c = self.capture()
        c.append(c.session, c.instance, 6, list(reversed(EVENTS)))
        self.assertFalse(c.stop(render_complete=True)['oracle_match'])

    def test_reset_conflict_and_lifetime(self):
        c = self.capture()
        c.append(c.session, c.instance, 2, EVENTS[:2])
        c.append(c.session, c.instance, 0, [])
        conflicting = dict(EVENTS[0], track=6)
        c.append(c.session, c.instance, 2, [conflicting])
        r = c.stop(render_complete=True)
        self.assertEqual(len(r['errors']), 2)
        with self.assertRaises(ValueError):
            c.append(c.session, c.instance, 6, EVENTS)
        with self.assertRaises(ValueError):
            c.start()


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(Qualification)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if len(sys.argv) == 2:
        Path(sys.argv[1]).write_text(json.dumps(dict(
            status='PASS' if result.wasSuccessful() else 'FAIL',
            tests_run=result.testsRun, failures=len(result.failures),
            errors=len(result.errors), scope='Synthetic collector only; native '
            'acquisition and actual callback-size invariance remain unqualified'),
            indent=2))
    sys.exit(0 if result.wasSuccessful() else 1)
