from collections import deque
import rp2pio
import time


class Loop(rp2pio.Loop):
    def __init__(self):
        self.queue = deque((), 10)
        self.stopped = False

    def create_future(self):
        return Future(self)

    def create_task(self, coro):
        task = Task(self, coro)
        self.call_soon(task.run)
        return task

    def call_soon(self, callback, *args):
        self.queue.append((callback, args))

    def run_forever(self):
        self.stopped = False
        while True:
            while len(self.queue) > 0:
                callback, args = self.queue.popleft()
                callback(*args)
                if self.stopped:
                    return
                self.poll_isr()
            print("sleep")
            while len(self.queue) == 0:
                self.poll_isr()

    def stop(self):
        self.stopped = True

    def run_until_complete(self, fut):
        # need iscoroutine function
        if not isinstance(fut, Future):
            fut = self.create_task(fut)
        fut.add_done_callback(lambda _: self.stop())
        self.run_forever()
        return fut.result()

    def run_one(self):
        self.poll_isr()
        if len(self.queue) == 0:
            return False
        callback, args = self.queue.popleft()
        callback(*args)
        return True


class Future:
    def __init__(self, loop):
        self.loop = loop
        self.callbacks = []

    def done(self):
        return hasattr(self, "_result") or hasattr(self, "_exception")

    def result(self):
        if hasattr(self, "_exception"):
            raise self._exception
        else:
            return self._result

    def set_result(self, value):
        assert not self.done()
        self._result = value
        for cb in self.callbacks:
            self.loop.call_soon(cb, self)
        self.callbacks.clear()

    def set_exception(self, exc):
        assert not self.done()
        self._exception = exc
        for cb in self.callbacks:
            self.loop.call_soon(cb, self)
        self.callbacks.clear()

    def add_done_callback(self, callback):
        if self.done():
            self.loop.call_soon(callback, self)
        else:
            self.callbacks.append(callback)

    def __await__(self):
        if not self.done():
            yield self
        return self.result()


class Task(Future):
    def __init__(self, loop, coro):
        Future.__init__(self, loop)
        self.coro = coro

    def run(self, fut=None):
        try:
            result = self.coro.send(None)
            if result is None:
                self.loop.call_soon(self.run)
            else:
                result.add_done_callback(self.run)
        except StopIteration as exc:
            self.set_result(exc.value)
        except Exception as exc:
            self.set_exception(exc)


def get_event_loop():
    loop = rp2pio.get_event_loop()
    if not loop:
        loop = Loop()
        rp2pio.set_event_loop(loop)
    return loop
