#!/usr/bin/env python3

import gatt
import time
import sys

manager = gatt.DeviceManager(adapter_name='hci0')

class AnyDevice(gatt.Device):
    def connect_succeeded(self):
        super().connect_succeeded()
        print("[%s] Connected" % (self.mac_address))

    def connect_failed(self, error):
        super().connect_failed(error)
        print("[%s] Connection failed: %s" % (self.mac_address, str(error)))

    def disconnect_succeeded(self):
        super().disconnect_succeeded()
        print("[%s] Disconnected" % (self.mac_address))
        manager.stop()

    def services_resolved(self):
        super().services_resolved()

        batt_service = next(
            s for s in self.services
            if s.uuid == '00453ed2-f5e0-11ea-b224-00155df38b93')

        batt_characteristic = next(
            c for c in batt_service.characteristics
            if c.uuid == '00453ed2-f5e0-11ea-b224-00155df38b92')

        # actually using indicate rather than notify
        batt_characteristic.enable_notifications()

    def characteristic_enable_notifications_failed(self, characteristic):
        print("failed to setup notify")

    def characteristic_enable_notifications_succeeded(self, characteristic):
        print("notify enabled")

    def characteristic_value_updated(self, characteristic, value):
        adc = int.from_bytes(value, "little") 
        batt = adc*1.769/4096/0.49
        pct = max(min((batt/2-1.1)/0.00273,100),0)
        print("{:.1f} ADC: {} BATT: {:.3f} {:.1f}%".format(time.time(), adc, batt, pct))
        sys.stdout.flush()


device = AnyDevice(mac_address='c1:8f:48:c0:9c:cf', manager=manager)

while 1:
    print("starting")
    device.connect()
    manager.run()
    time.sleep(1)
