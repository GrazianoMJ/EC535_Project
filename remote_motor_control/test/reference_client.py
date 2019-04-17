#!/usr/bin/env python
import bluetooth
from contextlib import closing

services = bluetooth.find_service(uuid='ce025ea4-00d6-44f3-ae1c-a5cba97381fd')
dmg_service = next(s for s in services if s['name'] == 'DMG Turret Control')
with closing(bluetooth.BluetoothSocket(bluetooth.RFCOMM)) as sock:
    sock.connect((dmg_service['host'], dmg_service['port']))
    sock.send('This is a test message.')
    sock.send('This is another test message.')
