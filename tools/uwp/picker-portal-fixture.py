#!/usr/bin/env python3
"""Fake FileChooser on a private dbus-run-session bus; never owns the desktop bus."""
import os
from pathlib import Path
import sys
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

if os.environ.get('WINEGDK_PICKER_TEST_BUS') != '1':
    raise SystemExit('Run only via the private picker test harness')
DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus()
name = dbus.service.BusName('org.freedesktop.portal.Desktop', bus)
loop = GLib.MainLoop()
root = Path(sys.argv[1])
paths = [root / 'world with space.mcworld', root / 'mundo-ação.mcworld']
for path in paths:
    path.write_bytes(b'WineGDK picker test\n')
requests = []

class Request(dbus.service.Object):
    @dbus.service.method('org.freedesktop.portal.Request', in_signature='', out_signature='')
    def Close(self):
        (root / 'closed').touch()
        print('portal request closed', flush=True)

    @dbus.service.signal('org.freedesktop.portal.Request', signature='ua{sv}')
    def Response(self, code, result):
        pass

class Portal(dbus.service.Object):
    count = 0
    @dbus.service.method('org.freedesktop.portal.FileChooser', in_signature='ssa{sv}', out_signature='o')
    def OpenFile(self, parent, title, options):
        case = self.count
        self.count += 1
        assert bool(options.get('multiple', False)) == (case in (1, 3))
        assert any(str(pattern[1]) == '*.mcworld' for _, patterns in options['filters'] for pattern in patterns)
        path = '/org/freedesktop/portal/desktop/request/fixture/r' + str(case)
        request = Request(bus, path)
        requests.append(request)
        def respond():
            uris = [paths[0].as_uri()] if case == 0 else [p.as_uri() for p in paths]
            if case == 4: uris = ['https://example.invalid/not-a-local-file']
            if case == 5: uris = [(root / 'missing.mcworld').as_uri()]
            request.Response(1 if case in (2, 3) else 0,
                             {'uris': dbus.Array(uris, signature='s', variant_level=1)})
            print('portal scenario', case, flush=True)
            return False
        if case != 6: GLib.timeout_add(20, respond)
        return dbus.ObjectPath(path)

portal = Portal(bus, '/org/freedesktop/portal/desktop')
(root / 'ready').touch()
loop.run()
