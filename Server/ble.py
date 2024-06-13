# from adafruit_ble import BLERadio
# from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
# from adafruit_ble.services.nordic import UARTService
# import time

# # Define device IDs
# ID1 = "2BE99CA4-1DAA-3A26-57F4-4D046484CDDF"
# ID2 = "1EE3DDA6-D71B-D36B-F8D9-80A276B9D891"

# device = None

# # Initialize BLE radio
# radio = BLERadio()
# print("Scanning for devices...")
# found = set()

# # Scan for devices
# for entry in radio.start_scan(timeout=60, minimum_rssi=-80):
#     addr = entry.address
#     if addr not in found:
#         print(entry.complete_name)
#         if addr.string == ID1 or addr.string == ID2:
#             found.add(addr)
#             device = radio.connect(entry)
#             print("Device connected!")
#             break

# radio.stop_scan()

# while(True):
#     continue

from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic import UARTService

# Define device IDs
ID1 = "Jang"
ID2 = "1EE3DDA6-D71B-D36B-F8D9-80A276B9D891"

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
            device = radio.connect(entry)
            print("Device connected!")
            break

radio.stop_scan()

# uart = UARTService()
buffer = device[UARTService]

print(buffer)

buffer = 64

while device and device.connected:

    try:
        message = device[UARTService].read(buffer)
        print(message)
    except OSError:
        try:
            uart_connection.disconnect()
        except:  # pylint: disable=bare-except
            pass
        uart_connection = None
        


# while (True):
#     continue