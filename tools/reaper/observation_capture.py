"""Bounded, fail-closed evidence collection outside the audio process.

This consumes observations, not audio. It cannot recover a batch already erased
by the native scheduler. A successful collector test never qualifies acquisition.
Each new transport epoch needs a new collector and an independent event oracle.
"""
import copy


class EventCapture:
    def __init__(self, session, instance, expected, capacity=65536):
        if not session or not instance or not 0 < capacity <= 65536:
            raise ValueError('explicit session, instance and bounded capacity required')
        if len(expected) > capacity:
            raise ValueError('oracle exceeds capture capacity')
        self.session, self.instance = session, instance
        self.expected, self.capacity = copy.deepcopy(expected), capacity
        self.events, self.seen = [], set()
        self.last_emitted = 0
        self.errors = []
        self.truncated = False
        self.started = self.stopped = False

    def _error(self, reason):
        # There are only three error identities; repeated bad polls cannot grow
        # an unbounded diagnostic list.
        if reason not in self.errors:
            self.errors.append(reason)

    def start(self):
        if self.started:
            raise ValueError('capture cannot be restarted or reused')
        self.started = True

    def append(self, session, instance, emitted, events):
        if not self.started or self.stopped:
            raise ValueError('watcher must be running')
        if session != self.session or instance != self.instance:
            self._error('SESSION_OR_INSTANCE_MISMATCH')
            return
        if emitted < self.last_emitted:
            self._error('COUNTER_RESET_REQUIRES_NEW_EPOCH')
            return
        self.last_emitted = emitted
        for e in events:
            # stable_order distinguishes multiple same-track triggers at one time.
            key = tuple(e[k] for k in ('boundary', 'ordinal', 'stable_order'))
            if key in self.seen:
                if next(x for x in self.events if tuple(x[k] for k in
                        ('boundary', 'ordinal', 'stable_order')) == key) != e:
                    self._error('CONFLICTING_EVENT_IDENTITY')
                continue
            if len(self.events) == self.capacity:
                self.truncated = True
                continue
            self.seen.add(key)
            self.events.append(copy.deepcopy(e))

    def stop(self, *, render_complete, timed_out=False):
        if not self.started or self.stopped:
            raise ValueError('invalid watcher stop')
        self.stopped = True
        dropped = max(0, self.last_emitted - len(self.events))
        oracle_match = self.events == self.expected
        complete = (render_complete and not timed_out and not self.errors and
                    not self.truncated and dropped == 0 and
                    self.last_emitted == len(self.expected) and oracle_match and
                    len(self.events) > 1)
        return dict(status='PASS' if complete else 'BLOCKED',
                    session=self.session, instance=self.instance,
                    events=copy.deepcopy(self.events), observed=len(self.events),
                    expected=len(self.expected), emitted=self.last_emitted,
                    dropped_or_unobserved=dropped, truncated=self.truncated,
                    timed_out=timed_out, render_complete=render_complete,
                    oracle_match=oracle_match, errors=self.errors.copy())
