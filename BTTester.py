import asyncio
import csv
import datetime
import logging
import os
from bleak import BleakScanner, BleakClient

# --- Config ---
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
CHARACTERISTIC_UUID_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" 
CHARACTERISTIC_UUID_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
DEVICE_NAME = "BrainBallast"

OUTPUT_FILE = "sensor_data.csv"

# --- Logging ---
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("BTBrain")

class BLEDataReceiver:
    def __init__(self):
        self.client = None
        self.csv_writer = None
        self.csv_file = None
        self.connected = False
        self.buffer = ""
        self.logging_active = False
        self.current_log_path = None
        self.command_response = ""
        self.waiting_for_response = False
        self.download_mode = False
        self.download_filename = ""
        self.download_content = ""

    async def scan_for_device(self):
        """Scan for the BrainBallast device"""
        print("Scanning for BLE devices...")
        devices = await BleakScanner.discover(timeout=10.0)

        print("Found devices:")
        for device in devices:
            print(f"  {device.name or 'Unknown'} - {device.address}")
            if device.name == DEVICE_NAME:
                print(f"*** Found target device {DEVICE_NAME} at {device.address} ***")
                return device

        print(f"Device '{DEVICE_NAME}' not found!")
        return None

    async def connect(self, device):
        """Connect to the BLE device"""
        print(f"Connecting to {device.address}...")
        self.client = BleakClient(device.address)
        await self.client.connect()
        self.connected = True
        print("Connected!")

        # Start listening for responses
        await self.client.start_notify(CHARACTERISTIC_UUID_TX, self.notification_handler)
        
        # Auto-start logging
        await self.start_logging()

    async def disconnect(self):
        """Disconnect from the BLE device"""
        if self.client and self.connected:
            await self.stop_logging()
            await self.client.stop_notify(CHARACTERISTIC_UUID_TX)
            await self.client.disconnect()
            self.connected = False
            print("Disconnected.")

    def notification_handler(self, sender, data: bytearray):
        """Handle incoming BLE notifications"""
        chunk = data.decode(errors="ignore")
        self.buffer += chunk
        
        while "\n" in self.buffer:
            line, self.buffer = self.buffer.split("\n", 1)
            line = line.strip()
            if not line:
                continue

            # Handle download mode
            if self.download_mode:
                if "End of file" in line:
                    self.download_mode = False
                    self.waiting_for_response = False
                elif not line.startswith("==="):
                    self.download_content += line + "\n"
                return

            # Handle command responses
            if self.waiting_for_response:
                # Skip acknowledgment lines
                if line.startswith("Received:"):
                    return
                
                self.command_response += line + "\n"
                print(line)
                
                # End response detection
                if ("End of file list" in line or 
                    "End of file" in line or
                    "bytes)" in line or
                    "SD Card Info:" in line or
                    "Usage:" in line or
                    "Free:" in line or
                    "Unknown command:" in line or
                    "File not found:" in line or
                    "Deleted:" in line or
                    "Failed to delete:" in line or
                    "Invalid line count:" in line or
                    "Usage: tail" in line):
                    self.waiting_for_response = False
                return

            # Log sensor data to file if logging is active
            if self.logging_active and self.csv_writer and "," in line:
                row = [field.strip() for field in line.split(",")]
                self.csv_writer.writerow(row)
                self.csv_file.flush()

    async def send_command(self, command):
        """Send a command to the device and wait for response"""
        if not self.connected:
            print("Not connected to device!")
            return False

        self.command_response = ""
        self.waiting_for_response = True
        
        # Handle download command specially
        if command.startswith("download "):
            self.download_mode = True
            self.download_content = ""
            parts = command.split()
            if len(parts) >= 2:
                self.download_filename = parts[1]

        command_with_newline = command + "\n"
        await self.client.write_gatt_char(CHARACTERISTIC_UUID_RX, command_with_newline.encode())
        
        # Wait for response with timeout
        timeout_seconds = 60 if command.startswith("download") else 5
        timeout_count = 0
        while self.waiting_for_response and timeout_count < (timeout_seconds * 10):
            await asyncio.sleep(0.1)
            timeout_count += 1
        
        if timeout_count >= (timeout_seconds * 10):
            print("Command timeout!")
            self.waiting_for_response = False
            self.download_mode = False
            return False
        
        return True

    async def start_logging(self):
        """Start logging sensor data to file"""
        if self.logging_active:
            print("Logging already active!")
            return

        os.makedirs("logs", exist_ok=True)
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.current_log_path = os.path.join("logs", f"{timestamp}_{OUTPUT_FILE}")

        self.csv_file = open(self.current_log_path, "w", newline="")
        self.csv_writer = csv.writer(self.csv_file, quoting=csv.QUOTE_MINIMAL)
        
        self.logging_active = True
        print(f"Started logging to: {self.current_log_path}")

    async def stop_logging(self):
        """Stop logging sensor data"""
        if not self.logging_active:
            print("Logging not active!")
            return

        if self.csv_file:
            self.csv_file.close()
            self.csv_file = None
            self.csv_writer = None

        self.logging_active = False
        print(f"Stopped logging. File saved: {self.current_log_path}")
        self.current_log_path = None

    async def handle_download_command(self, command_parts):
        """Handle download command with optional output filename"""
        if len(command_parts) < 2:
            print("Usage: download <filename> [output_filename]")
            return

        device_filename = command_parts[1]
        
        # Determine output filename
        if len(command_parts) >= 3:
            output_filename = command_parts[2]
        else:
            output_filename = device_filename

        # Send download command to device
        await self.send_command(f"download {device_filename}")
        
        # Save downloaded content to file
        if self.download_content:
            try:
                with open(output_filename, 'w') as f:
                    f.write(self.download_content)
                print(f"File saved as: {output_filename}")
            except Exception as e:
                print(f"Error saving file: {e}")
        else:
            print("No content received or file not found on device")

    async def interactive_mode(self):
        """Interactive command mode with logging controls"""
        print("\n=== BrainBallast Data Logger & Commander ===")
        print("Logging Controls:")
        print("  log / listen - Start logging sensor data to timestamped file")
        print("  stop - Stop current logging session")
        print("\nDevice Commands:")
        print("  list - List all files on device")
        print("  download <filename> [output] - Download file (optional output name)")
        print("  tail <filename> <lines> - Show last N lines of file")
        print("  size <filename> - Show file size")
        print("  delete <filename> - Delete file") 
        print("  info - Show SD card information")
        print("  test - Test basic communication")
        print("  debug - Show raw BT communication")
        print("  quit - Exit")
        print(f"\nAuto-started logging to: {self.current_log_path}")
        print("Type commands and press Enter:")

        # Create input task for async input handling
        async def get_input():
            loop = asyncio.get_event_loop()
            return await loop.run_in_executor(None, input, "> ")

        while self.connected:
            try:
                # Get user input asynchronously
                command = await get_input()
                command = command.strip()
                
                if command.lower() in ['quit', 'exit', 'q']:
                    break
                elif command.lower() in ['log', 'listen']:
                    await self.start_logging()
                elif command.lower() == 'stop':
                    await self.stop_logging()
                elif command.lower() == 'test':
                    await self.send_command("test")
                elif command.lower() == 'help':
                    print("\nAvailable commands:")
                    print("  log/listen - Start new logging session")
                    print("  stop - Stop current logging") 
                    print("  list - List files on device")
                    print("  download <file> [output] - Download file")
                    print("  tail <file> <lines> - Show last N lines")
                    print("  size <file> - Show file size")
                    print("  delete <file> - Delete file")
                    print("  info - SD card info")
                    print("  test - Test communication")
                    print("  quit - Exit")
                elif command.startswith('download '):
                    command_parts = command.split()
                    await self.handle_download_command(command_parts)
                elif command:
                    # Send other commands to device
                    await self.send_command(command)
                    
            except KeyboardInterrupt:
                print("\nExiting...")
                break
            except Exception as e:
                print(f"Error in interactive mode: {e}")
                break

    async def run_single_command(self, command):
        """Run a single command"""
        command_lower = command.lower()
        
        if command_lower in ['log', 'listen']:
            await self.start_logging()
            return f"Started logging to: {self.current_log_path}"
        elif command_lower == 'stop':
            await self.stop_logging()
            return "Stopped logging"
        elif command.startswith('download '):
            command_parts = command.split()
            await self.handle_download_command(command_parts)
            return "Download completed"
        else:
            # Send to device and wait for response
            await self.send_command(command)
            return "Command completed"

# --- Main Functions ---
async def connect_and_run_interactive():
    """Connect to device and run interactive mode with auto-logging"""
    receiver = BLEDataReceiver()
    
    try:
        device = await receiver.scan_for_device()
        if not device:
            return
            
        await receiver.connect(device)
        await receiver.interactive_mode()
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        await receiver.disconnect()

async def connect_and_run_command(command):
    """Connect to device and run single command"""
    receiver = BLEDataReceiver()
    
    try:
        device = await receiver.scan_for_device()
        if not device:
            return
            
        await receiver.connect(device)
        result = await receiver.run_single_command(command)
        print(f"Result: {result}")
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        await receiver.disconnect()

async def connect_and_just_log():
    """Connect and just log data (no interactive mode)"""
    receiver = BLEDataReceiver()
    
    try:
        device = await receiver.scan_for_device()
        if not device:
            return
            
        await receiver.connect(device)
        print(f"Logging started to: {receiver.current_log_path}")
        print("Press Ctrl+C to stop...")
        
        # Keep running until interrupted
        while True:
            await asyncio.sleep(1)
            
    except KeyboardInterrupt:
        print("\nStopping...")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        await receiver.disconnect()

# --- CLI Interface ---
if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        if sys.argv[1].lower() == '--log-only':
            # Just log, no interactive
            asyncio.run(connect_and_just_log())
        else:
            # Run single command
            command = " ".join(sys.argv[1:])
            asyncio.run(connect_and_run_command(command))
    else:
        # Run interactive mode (auto-starts logging)
        try:
            asyncio.run(connect_and_run_interactive())
        except KeyboardInterrupt:
            print("Stopped by user.")