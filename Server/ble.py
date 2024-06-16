from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic import UARTService
import subprocess
import threading
import time

# Global variable to store the current process
current_process = None

# Function to read from Bluetooth device and send to subprocess
def read_from_device_and_send(device):
    global current_process

    while device and device.connected:
        # Read data from the Bluetooth device
        data = device[UARTService].read(20)  # Adjust buffer size as needed

        if data:
            data = data.decode('utf-8').strip()

            # Send the appropriate commands to the subprocess
            if data == "Button4":
                current_process.stdin.write(b"END\n")
                current_process.stdin.flush()
                current_process.stdin.write(b"END\n")
                current_process.stdin.flush()
                break

            elif data.find("Up") != -1:
                current_process.stdin.write(b"UP\n")
                current_process.stdin.flush()

            elif data.find("Down") != -1:
                current_process.stdin.write(b"DOWN\n")
                current_process.stdin.flush()

            elif data.find("Left") != -1:
                current_process.stdin.write(b"LEFT\n")
                current_process.stdin.flush()

            elif data.find("Right") != -1:
                current_process.stdin.write(b"RIGHT\n")
                current_process.stdin.flush()

            elif data == "Button2":
                start_new_game("blicker.py")

            elif data == "Button3":
                start_new_game("pong.py")

            elif data == "Button1":
                start_new_game("snake.py")

            print(data)

def start_new_game(game_script):
    global current_process

    if current_process:
        current_process.stdin.write(b"END\n")
        current_process.stdin.flush()
        current_process.terminate()
        current_process.wait()

    current_process = subprocess.Popen(['python3', game_script], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

# Function to run the subprocess
def run_subprocess():
    global current_process

    # Start the initial game
    start_new_game('snake.py')

    # Start thread to read from Bluetooth device and send to subprocess
    read_thread = threading.Thread(target=read_from_device_and_send, args=(device,))
    read_thread.start()

    # Wait for the read thread to finish
    read_thread.join()

# Initialize BLE radio
radio = BLERadio()

print("Scanning for devices...")
found = set()

# Scan for devices
for entry in radio.start_scan(timeout=1200, minimum_rssi=-80):
    addr = entry.address

    if addr not in found:
        print(entry.complete_name)

        # Replace with your device IDs
        if entry.complete_name in ["Han"]:
            found.add(addr)
            device = radio.connect(entry)
            print("Device connected!")
            break

radio.stop_scan()

if device:
    # Start subprocess in a separate thread
    subprocess_thread = threading.Thread(target=run_subprocess)
    subprocess_thread.start()

    while device.connected:
        pass

    subprocess_thread.join()

print("Disconnected from device or timeout reached. Exiting...")
