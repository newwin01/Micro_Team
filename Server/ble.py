from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic import UARTService
import re

# ID = ['x20', 'x43', 'x6f', 'x6e', 'x6e', 'x65', 'x63', 'x74'] Using NRF Desktop BLE
ID = ['x75', 'x06', 'x09', 'x6e', 'x52', 'x46', 'x35', 'x78'] #Build using NRF Connect
device = None
message = None
buffer = 64

radio = BLERadio()
print("scanning")
found = set()
for entry in radio.start_scan(timeout=60, minimum_rssi=-80):
    addr = entry.address
    if addr not in found:
        print(entry)
        s = repr(entry)
        s = re.findall(r'"(.+?)"',s)
        s = s[0]
        s = s[-31:].split("\\")
        print(s)

        if s == ID:
            device = radio.connect(entry)
            print("device connected!") 
            break

radio.stop_scan()

while device and device.connected:
    print("Connected to device. Checking for UARTService...")

    try:
        # Check if the UARTService is available in the connected device
        if UARTService in device:
            uart_service = device[UARTService]
            print("UARTService found!")
            while device.connected:
                try:
                    # Read data from the UART service
                    data = uart_service.read(buffer_size)
                    if data:
                        message = data.decode('utf-8')
                        print("Received message:", message)
                except OSError as e:
                    print("Read error:", e)
                    device.disconnect()
                    device = None
                    break
        else:
            print("UARTService not found in the connected device.")
    except Exception as e:
        print(f"An error occurred: {e}")
else:
    print("Failed to connect to the device.")
