import asyncio
import csv
import datetime
import logging
import os
from bleak import BleakScanner, BleakClient

# --- Config ---
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
CHARACTERISTIC_UUID_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
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
        async with BleakClient(device.address) as client:  # use .address on Windows
            self.client = client
            self.connected = True
            logger.info("Connected!")

            self.setup_files()

            await client.start_notify(CHARACTERISTIC_UUID_TX, self.notification_handler)
            logger.info("Listening for notifications... Press Ctrl+C to stop.")

            try:
                while True:
                    await asyncio.sleep(1)  # keep running
            except asyncio.CancelledError:
                pass
            finally:
                await client.stop_notify(CHARACTERISTIC_UUID_TX)
                self.cleanup_files()
                logger.info("Disconnected cleanly.")

    def notification_handler(self, sender, data: bytearray):
        """Handle incoming BLE notifications (rebuild full lines)"""
        chunk = data.decode(errors="ignore")
        self.buffer += chunk

        while "\n" in self.buffer:
            line, self.buffer = self.buffer.split("\n", 1)
            line = line.strip()
            if not line:
                continue

            logger.info(f"Received: {line}")

            # Raw log
            self.raw_file.write(line + "\n")
            self.raw_file.flush()

            # CSV log – split by commas if present
            if "," in line:
                row = [field.strip() for field in line.split(",")]
                self.csv_writer.writerow(row)
            else:
                self.csv_writer.writerow([line])

            self.csv_file.flush()

    def setup_files(self):
        """Set up CSV and raw log files in logs/ folder with timestamp prefix"""
        os.makedirs("logs", exist_ok=True)  # ensure logs folder exists

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

    def cleanup_files(self):
        """Close files cleanly"""
        if self.csv_file:
            self.csv_file.close()
        if self.raw_file:
            self.raw_file.close()

# --- Main ---
async def main():
    receiver = BLEDataReceiver()
    device = await receiver.scan_for_device()
    if device:
        await receiver.connect_and_listen(device)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Stopped by user.")
