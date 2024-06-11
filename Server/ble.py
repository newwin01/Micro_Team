from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services import Service
from adafruit_ble.characteristics import Characteristic
from adafruit_ble.characteristics.int import Uint8Characteristic
from adafruit_ble.characteristics.stream import StreamIn
import adafruit_ble
from adafruit_ble.services.nordic import UARTService

# Define device IDs
ID1 = "2BE99CA4-1DAA-3A26-57F4-4D046484CDDF"
ID2 = "1EE3DDA6-D71B-D36B-F8D9-80A276B9D891"

device = None
count = 0

# Initialize BLE radio
radio = BLERadio()
print("Scanning for devices...")
found = set()

# Scan for devices
for entry in radio.start_scan(timeout=60, minimum_rssi=-80):
    addr = entry.address

    if addr not in found:
        print(entry.complete_name)

        if addr.string == ID1 or addr.string == ID2:
            found.add(addr)
            if count == 0:
                device = radio.connect(entry)
                print("Device connected!")
                count += 1
                # break
            else:
                device = radio.connect(entry)
                print("Device connected!")
                break

radio.stop_scan()

