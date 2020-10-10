#!/usr/bin/env python3

import argparse
import gatt
import time
import sys
from influxdb import InfluxDBClient
import datetime
from struct import unpack

class AnyDevice(gatt.Device):
    def __init__(self, args, manager):
        super().__init__(args.mac[0], manager)
        if args.influx != None:
            self.influxdb = args.influx[0]
        else:
            self.influxdb = None
        self.measurements = []
        self.manager = manager

    def connect_succeeded(self):
        super().connect_succeeded()
        print("[%s] Connected" % (self.mac_address))

    def connect_failed(self, error):
        super().connect_failed(error)
        print("[%s] Connection failed: %s" % (self.mac_address, str(error)))

    def disconnect_succeeded(self):
        super().disconnect_succeeded()
        print("[%s] Disconnected" % (self.mac_address))
        self.manager.stop()

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

    def adc_to_voltage(self, adc):
        return adc*1.769/4096/0.49

    def voltage_to_percentage(self, voltage):
        return max(min((voltage/2-1.1)/0.00273,100),0)

    def new_battery_value(self, value):
        (adc, x, y, z) = unpack("<H3h", value)
        batt = self.adc_to_voltage(adc)
        pct = self.voltage_to_percentage(batt)
        print("{:.1f} ADC: {} BATT: {:.3f} {:.1f}% [{}, {}, {}]".format(time.time(), adc, batt, pct, x, y, z))
        sys.stdout.flush()
        self.add_measurement(adc, x, y, z)

    def add_measurement(self, adc, x, y, z):
        self.measurements.append(adc)
        if len(self.measurements) >= 60:
            avg = sum(self.measurements) / len(self.measurements)
            samples = len(self.measurements)
            self.measurements = []
            voltage = self.adc_to_voltage(avg)
            fields = {
                "adc": avg,
                "battery": voltage,
                "samples": samples,
                "x": x,
                "y": y,
                "z": z,
                "percent": self.voltage_to_percentage(voltage)
            }
            updateDatabase(self.influxdb, fields)

    def characteristic_value_updated(self, characteristic, value):
        if characteristic.uuid == "00453ed2-f5e0-11ea-b224-00155df38b92":
            self.new_battery_value(value)

def updateDatabase(hostname,fields):
    ts = datetime.datetime.utcnow().replace(tzinfo=datetime.timezone.utc).isoformat()
    json_body = [
        {
            "measurement": "voltage",
            "time": ts,
            "fields": fields,
        }
    ]
    if hostname != None:
        client = InfluxDBClient(hostname, 8086, '', '', 'nrf52')
        client.write_points(json_body)
        print("JSON: {}".format(json_body))

parser = argparse.ArgumentParser(description="receive bluetooth data")
parser.add_argument("--mac", nargs=1, required=True, dest="mac", type=str, help="bluetooth mac address")
parser.add_argument("--influx-host", nargs=1, required=False, dest="influx", type=str, help="influxdb hostname")
args = parser.parse_args()

manager = gatt.DeviceManager(adapter_name='hci0')
device = AnyDevice(args, manager=manager)

while 1:
    print("starting")
    device.connect()
    manager.run()
    time.sleep(1)
