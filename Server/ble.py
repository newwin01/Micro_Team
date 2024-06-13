from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic import UARTService
import subprocess
import threading

# Function to read from Bluetooth device and send to subprocess
def read_from_device_and_send(device, process):
    while device and device.connected:
        data = device[UARTService].read(64)  # Adjust buffer size as needed

        if data:
            data = data.decode('utf-8')

            if (data == "Button1"):
                process.stdin.write(b"END\n")
                process.stdin.flush()
                process.stdin.write(b"END\n")
                process.stdin.flush()

                break

            if (data == "Up"):
                process.stdin.write(b"UP\n")
                process.stdin.flush()

            if (data == "Down"):
                process.stdin.write(b"DOWN\n")
                process.stdin.flush()
            
        print(data)

        # if data:
        #     process.stdin.write(data)
        #     process.stdin.flush()
        #     print(f"Sent data to subprocess: {data}")

# Function to run the subprocess
def run_subprocess():
    # Start the subprocess
    process = subprocess.Popen(['python', 'pong.py'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Start thread to read from Bluetooth device and send to subprocess
    read_thread = threading.Thread(target=read_from_device_and_send, args=(device, process))
    read_thread.start()

    stdout = process.poll()

    print(stdout)

    # Wait for the read thread to finish
    read_thread.join()

# Initialize BLE radio
radio = BLERadio()

print("Scanning for devices...")
found = set()

# Scan for devices
for entry in radio.start_scan(timeout=60, minimum_rssi=-80):
    addr = entry.address

    if addr not in found:
        print(entry.complete_name)

        # Replace with your device IDs
        if entry.complete_name in ["Jang", "Han"]:
            found.add(addr)
            device = radio.connect(entry)
            print("Device connected!")
            break

radio.stop_scan()

if device:
    # Start subprocess in a separate thread
    subprocess_thread = threading.Thread(target=run_subprocess)
    subprocess_thread.start()

    # Continue interacting with the Bluetooth device or perform other tasks as needed
    while device.connected:
        # Example: Read more data from device if necessary
        # data = device[UARTService].read(64)
        # Process or handle data as needed

        pass  


    subprocess_thread.join()

print("Disconnected from device or timeout reached. Exiting...")
