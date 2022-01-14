#!/usr/bin/env python3

import mido
import time

note = 37
port = mido.open_output(name="TEST", virtual=True)
on = mido.Message("note_on", note=note, velocity=127)
off = mido.Message("note_off", note=note, velocity=0)

while True:
    print(".", end="", flush=True)

    port.send(on)
    time.sleep(5 / 1000)
    port.send(off)

    time.sleep(1)
