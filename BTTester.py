import asyncio
import csv
import datetime
import logging
import os
from bleak import BleakScanner, BleakClient

# --- Config ---
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
CHARACTERISTIC_UUID_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
CHARACTERISTIC_UUID_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
DEVICE_NAME = "BrainBallast"

OUTPUT_FILE = "sensor_data.csv"
RAW_DATA_FILE = "raw_data.log"

# --- Logging ---
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("BTBrain")

class BLEDataReceiver:
    def __init__(self):
        self.client = None
        self.csv_writer = None
        self.csv_file = None
        self.raw_file = None
        self.connected = False
        self.buffer = ""  # buffer for partial BLE chunks
        self.command_mode = False
        self.command_response = ""
        self.waiting_for_response = False

    async def scan_for_device(self):
        """Scan for the BrainBallast device"""
        logger.info("Scanning for BLE devices...")
        devices = await BleakScanner.discover(timeout=10.0)

        logger.info("Found devices:")
        for device in devices:
            logger.info(f"  {device.name or 'Unknown'} - {device.address}")
            if device.name == DEVICE_NAME:
                logger.info(f"*** Found target device {DEVICE_NAME} at {device.address} ***")
                return device

        logger.error(f"Device '{DEVICE_NAME}' not found!")
        return None

    async def connect_and_listen(self, device):
        """Connect to the BLE device and listen for notifications"""
        logger.info(f"Connecting to {device.address}...")
        async with BleakClient(device.address) as client:
            self.client = client
            self.connected = True
            logger.info("Connected!")

            self.setup_files()

            await client.start_notify(CHARACTERISTIC_UUID_TX, self.notification_handler)
            logger.info("Listening for notifications...")
            logger.info("Commands available: 'list', 'download <file>', 'tail <file> [lines]', 'size <file>', 'delete <file>', 'info', 'help'")
            logger.info("Type 'quit' to exit, or just press Enter to continue receiving sensor data")

            try:
                # Start the input handler task
                input_task = asyncio.create_task(self.handle_user_input())
                
                # Keep running until cancelled
                await asyncio.gather(input_task)
                
            except asyncio.CancelledError:
                pass
            except KeyboardInterrupt:
                logger.info("Interrupted by user")
            finally:
                await client.stop_notify(CHARACTERISTIC_UUID_TX)
                self.cleanup_files()
                logger.info("Disconnected cleanly.")

    async def handle_user_input(self):
        """Handle user commands from console"""
        while self.connected:
            try:
                # Non-blocking input check
                user_input = await asyncio.get_event_loop().run_in_executor(
                    None, lambda: input("Command (or Enter for sensor data): ").strip()
                )
                
                if user_input.lower() == 'quit':
                    break
                elif user_input == '':
                    # Just continue receiving sensor data
                    continue
                else:
                    await self.send_command(user_input)
                    
            except EOFError:
                break
            except Exception as e:
                logger.error(f"Input error: {e}")
                break

    async def send_command(self, command):
        """Send a command to the ESP32"""
        if not self.connected or not self.client:
            logger.error("Not connected to device")
            return
            
        try:
            self.waiting_for_response = True
            self.command_response = ""
            
            logger.info(f"Sending command: {command}")
            await self.client.write_gatt_char(CHARACTERISTIC_UUID_RX, command.encode())
            
            # Wait for response with timeout
            timeout = 30  # 30 seconds timeout for large file operations
            start_time = asyncio.get_event_loop().time()
            
            while self.waiting_for_response and (asyncio.get_event_loop().time() - start_time) < timeout:
                await asyncio.sleep(0.1)
                
            if self.waiting_for_response:
                logger.warning("Command timed out")
                self.waiting_for_response = False
                
        except Exception as e:
            logger.error(f"Failed to send command: {e}")
            self.waiting_for_response = False

    def notification_handler(self, sender, data: bytearray):
        """Handle incoming BLE notifications"""
        chunk = data.decode(errors="ignore")
        self.buffer += chunk

        while "\n" in self.buffer:
            line, self.buffer = self.buffer.split("\n", 1)
            line = line.strip()
            if not line:
                continue

            # Check if this is a command response
            if self.is_command_response(line):
                self.handle_command_response(line)
            else:
                # Normal sensor data
                self.handle_sensor_data(line)

    def is_command_response(self, line):
        """Determine if a line is a command response or sensor data"""
        command_indicators = [
            "FILES:", "END_FILES", "ERROR:", "SUCCESS:", 
            "FILE_START:", "FILE_END", "TAIL_START:", "TAIL_END",
            "SIZE:", "SD_INFO:", "COMMANDS:", "Total:", "Used:", "Free:", "Type:"
        ]
        
        # If we're waiting for a response, treat everything as command response
        if self.waiting_for_response:
            return True
            
        # Check for command response indicators
        return any(indicator in line for indicator in command_indicators)

    def handle_command_response(self, line):
        """Handle command response data"""
        logger.info(f"CMD Response: {line}")
        
        # Add to command response buffer
        self.command_response += line + "\n"
        
        # Check for end markers
        if any(end_marker in line for end_marker in ["END_FILES", "FILE_END", "TAIL_END", "COMMANDS:", "bytes"]):
            self.waiting_for_response = False
            
        # For single-line responses
        if line.startswith(("ERROR:", "SUCCESS:", "SIZE:", "SD_INFO:")):
            self.waiting_for_response = False

    def handle_sensor_data(self, line):
        """Handle normal sensor data"""
        logger.info(f"Sensor: {line}")

        # Raw log
        self.raw_file.write(line + "\n")
        self.raw_file.flush()

        # CSV log - split by commas if present
        if "," in line and not line.startswith(("ERROR:", "SUCCESS:", "FILES:")):
            # This looks like CSV sensor data
            row = [field.strip() for field in line.split(",")]
            self.csv_writer.writerow(row)
        else:
            # Single value or non-CSV data
            self.csv_writer.writerow([line])

        self.csv_file.flush()

    def setup_files(self):
        """Set up CSV and raw log files in logs/ folder with timestamp prefix"""
        os.makedirs("logs", exist_ok=True)

        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        csv_path = os.path.join("logs", f"{timestamp}_{OUTPUT_FILE}")
        raw_path = os.path.join("logs", f"{timestamp}_{RAW_DATA_FILE}")

        self.csv_file = open(csv_path, "w", newline="")
        self.raw_file = open(raw_path, "w")

        # Configure CSV writer with minimal quoting
        self.csv_writer = csv.writer(
            self.csv_file,
            quoting=csv.QUOTE_MINIMAL
        )

        logger.info(f"Logging to: {csv_path}")
        logger.info(f"Raw log: {raw_path}")

    def cleanup_files(self):
        """Close files cleanly"""
        if self.csv_file:
            self.csv_file.close()
        if self.raw_file:
            self.raw_file.close()


# --- Enhanced version with file saving for downloads ---
class EnhancedBLEDataReceiver(BLEDataReceiver):
    def __init__(self):
        super().__init__()
        self.downloading_file = False
        self.download_filename = ""
        self.download_content = ""
        self.download_size = 0

    def handle_command_response(self, line):
        """Enhanced command response handling with file download support"""
        logger.info(f"CMD Response: {line}")
        
        # Handle file download start
        if line.startswith("FILE_START:"):
            parts = line.split(":")
            if len(parts) >= 3:
                self.download_filename = parts[1].replace("/", "")  # Remove leading slash
                self.download_size = int(parts[2].split()[0])  # Extract size
                self.downloading_file = True
                self.download_content = ""
                logger.info(f"Starting download: {self.download_filename} ({self.download_size} bytes)")
            return
            
        # Handle file download end
        if line == "FILE_END":
            if self.downloading_file:
                self.save_downloaded_file()
                self.downloading_file = False
            self.waiting_for_response = False
            return
            
        # Handle file content during download
        if self.downloading_file:
            self.download_content += line + "\n"
            return
            
        # Normal command response handling
        super().handle_command_response(line)

    def save_downloaded_file(self):
        """Save downloaded file content to disk"""
        os.makedirs("downloads", exist_ok=True)
        
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"downloads/{timestamp}_{self.download_filename}"
        
        try:
            with open(filename, "w") as f:
                f.write(self.download_content)
            logger.info(f"File saved: {filename} ({len(self.download_content)} chars)")
        except Exception as e:
            logger.error(f"Failed to save file: {e}")


# --- Main ---
async def main():
    print("BrainBallast BLE Interface")
    print("=" * 40)
    print("Available commands:")
    print("  list          - List files on SD card")
    print("  download file - Download entire file")
    print("  tail file 20  - Get last 20 lines of file")
    print("  size file     - Get file size")
    print("  delete file   - Delete file")
    print("  info          - SD card information")
    print("  help          - Show device help")
    print("  quit          - Exit program")
    print("=" * 40)
    
    # Use enhanced receiver for file download support
    receiver = EnhancedBLEDataReceiver()
    device = await receiver.scan_for_device()
    if device:
        await receiver.connect_and_listen(device)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Stopped by user.")