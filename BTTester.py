import asyncio
import csv
import datetime
import logging
import os
import time
from bleak import BleakScanner, BleakClient
from bleak.backends.characteristic import BleakGATTCharacteristic

# --- Config ---
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA8A"
CHARACTERISTIC_UUID_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA8A" 
CHARACTERISTIC_UUID_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA8A"
DEVICE_NAME = "BrainBallast2"

# --- Logging ---
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("BTBrain")

class SimpleBLELogger:
    def __init__(self):
        self.client = None
        self.csv_writer = None
        self.csv_file = None
        self.connected = False
        self.buffer = ""
        self.current_log_path = None
        self.download_mode = False
        self.download_content = ""
        self.command_mode = False
        self.command_response = ""
        
        # Connection monitoring
        self.last_data_received = time.time()
        self.last_ping_sent = time.time()
        self.connection_timeout = 20.0  # 20 seconds without data = check connection
        self.ping_interval = 10.0  # Send ping every 10 seconds
        self.monitor_task = None
        
        # Stats
        self.lines_logged = 0
        self.bytes_received = 0
        self.session_start = None
        self.reconnect_count = 0

    async def scan_for_device(self):
        """Scan for the BrainBallast device"""
        print("🔍 Scanning for BLE devices...")
        print("⚠️  If device not found, make sure it's NOT paired/connected in system Bluetooth!")
        try:
            devices = await BleakScanner.discover(timeout=10.0)
            
            print(f"Found {len(devices)} BLE devices:")
            for device in devices:
                print(f"  - {device.name or 'Unknown'} ({device.address})")
                if device.name == DEVICE_NAME:
                    print(f"✅ Found {DEVICE_NAME} at {device.address}")
                    return device
            
            print(f"\n❌ Device '{DEVICE_NAME}' not found!")
            print("💡 Tips:")
            print("   1. Make sure device is powered on")
            print("   2. Disconnect/remove it from system Bluetooth settings")
            print("   3. Try resetting the ESP32")
            return None
        except Exception as e:
            print(f"Scan error: {e}")
            return None

    async def connect(self, device):
        """Connect and start auto-logging"""
        print(f"🔗 Connecting to {device.address}...")
        try:
            self.client = BleakClient(device.address, timeout=20.0)
            await self.client.connect()
            
            # Verify connection is actually working
            if not self.client.is_connected:
                print("❌ Connection failed - client not connected")
                return False
            
            self.connected = True
            self.last_data_received = time.time()
            self.last_ping_sent = time.time()
            
            if self.session_start is None:
                self.session_start = time.time()
            
            print("✅ Connected!")

            # Start notifications
            await self.client.start_notify(CHARACTERISTIC_UUID_TX, self.notification_handler)
            
            # Start connection monitor
            if self.monitor_task:
                self.monitor_task.cancel()
            self.monitor_task = asyncio.create_task(self.connection_monitor())
            
            # Auto-start logging if not already logging
            if not self.csv_writer:
                self.start_logging()
                print(f"📊 Auto-logging started to: {self.current_log_path}")
            else:
                print(f"📊 Continuing logging to: {self.current_log_path}")
            
            return True
            
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            self.connected = False
            return False

    async def disconnect(self):
        """Disconnect gracefully"""
        if self.monitor_task:
            self.monitor_task.cancel()
            try:
                await self.monitor_task
            except asyncio.CancelledError:
                pass
            
        if self.client:
            try:
                if self.client.is_connected:
                    await self.client.stop_notify(CHARACTERISTIC_UUID_TX)
                    await self.client.disconnect()
            except Exception as e:
                print(f"Disconnect error: {e}")
            finally:
                self.client = None
                
        self.connected = False
        self.stop_logging()
        print("👋 Disconnected.")

    async def connection_monitor(self):
        """Monitor connection health based on data reception and active checks"""
        while True:
            try:
                await asyncio.sleep(2.0)
                
                if not self.connected:
                    break
                
                # Check if client is still actually connected
                if self.client and not self.client.is_connected:
                    print("💀 BLE client reports disconnected")
                    self.connected = False
                    print("🔄 Attempting to reconnect...")
                    await self.reconnect()
                    continue
                
                current_time = time.time()
                time_since_data = current_time - self.last_data_received
                time_since_ping = current_time - self.last_ping_sent
                
                # Send periodic ping to keep connection alive
                if time_since_ping > self.ping_interval and not self.command_mode:
                    try:
                        await self.send_command_raw("ping")
                        self.last_ping_sent = current_time
                    except Exception as e:
                        print(f"Ping failed: {e}")
                        self.connected = False
                        await self.reconnect()
                        continue
                
                # If no data for too long, connection is dead
                if time_since_data > self.connection_timeout:
                    print(f"💀 No data received for {time_since_data:.1f}s - connection appears dead")
                    self.connected = False
                    print("🔄 Attempting to reconnect...")
                    await self.reconnect()
                    continue
                
            except asyncio.CancelledError:
                break
            except Exception as e:
                print(f"Monitor error: {e}")
                await asyncio.sleep(2.0)

    async def reconnect(self):
        """Attempt to reconnect"""
        self.reconnect_count += 1
        print(f"🔄 Reconnection attempt #{self.reconnect_count}")
        
        try:
            # Full cleanup
            if self.client:
                try:
                    if self.client.is_connected:
                        await self.client.disconnect()
                except:
                    pass
                self.client = None
            
            self.connected = False
            await asyncio.sleep(2.0)
            
            # Scan and reconnect
            print("🔍 Scanning for device...")
            device = await self.scan_for_device()
            
            if device:
                success = await self.connect(device)
                if success:
                    print(f"✅ Reconnection #{self.reconnect_count} successful!")
                    return True
                else:
                    print(f"❌ Reconnection #{self.reconnect_count} failed")
            else:
                print("❌ Device not found during reconnection")
            
            return False
                
        except Exception as e:
            print(f"Reconnection error: {e}")
            return False

    def notification_handler(self, sender: BleakGATTCharacteristic, data: bytearray):
        """Handle incoming data"""
        self.last_data_received = time.time()
        self.bytes_received += len(data)
        
        try:
            chunk = data.decode('utf-8', errors='replace')
            self.buffer += chunk
            
            while "\n" in self.buffer:
                line, self.buffer = self.buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue

                self._process_line(line)
                
        except Exception as e:
            print(f"Handler error: {e}")

    def _process_line(self, line):
        """Process individual lines"""
        # Handle ping responses silently
        if line == "PONG":
            return
        
        # Handle download mode
        if self.download_mode:
            if "=== End of file ===" in line:
                self.download_mode = False
                self.command_mode = False
            elif not line.startswith("===") and not line.startswith("ACK:"):
                self.download_content += line + "\n"
            return

        # Handle command responses
        if self.command_mode:
            if line.startswith("ACK:"):
                return  # Skip acknowledgments
            
            self.command_response += line + "\n"
            print(line)
            
            # End markers for various commands
            end_markers = [
                "End of file list", "End of file", 
                "End of tail", "End of info",
                "Command completed",
                "ERROR:", "SUCCESS:", "PONG"
            ]
            
            if any(marker in line for marker in end_markers):
                self.command_mode = False
            return

        # Skip non-CSV data (acknowledgments, status messages, etc.)
        if line.startswith("ACK:") or line.startswith("ERROR:") or line.startswith("==="):
            return

        # Log sensor data (CSV format: pres,temp,x,y,z,timestamp)
        if self.csv_writer and "," in line:
            try:
                # Validate it's actually CSV data with 6 fields
                fields = line.split(",")
                if len(fields) == 6:
                    # Try to parse as floats to validate
                    float(fields[0])  # pressure
                    float(fields[1])  # temperature
                    float(fields[2])  # x
                    float(fields[3])  # y
                    float(fields[4])  # z
                    int(fields[5])    # timestamp
                    
                    self.csv_writer.writerow(fields)
                    self.csv_file.flush()
                    self.lines_logged += 1
                    
                    # Print progress every 100 lines
                    if self.lines_logged % 100 == 0:
                        elapsed = time.time() - self.session_start
                        rate = self.lines_logged / elapsed if elapsed > 0 else 0
                        print(f"📈 Logged {self.lines_logged} lines ({rate:.1f} lines/sec)")
                        
            except (ValueError, IndexError):
                # Not valid CSV data, skip it
                pass

    def start_logging(self):
        """Start logging to timestamped file"""
        os.makedirs("logs", exist_ok=True)
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.current_log_path = os.path.join("logs", f"sensor_data_{timestamp}.csv")

        try:
            self.csv_file = open(self.current_log_path, "w", newline="")
            self.csv_writer = csv.writer(self.csv_file)
            
            # Write header
            self.csv_writer.writerow(["pressure", "temperature", "x_accel", "y_accel", "z_accel", "timestamp_ms"])
            self.csv_file.flush()
            
            self.lines_logged = 0
            print(f"📝 Logging to: {self.current_log_path}")
        except Exception as e:
            print(f"Failed to start logging: {e}")

    def stop_logging(self):
        """Stop logging"""
        try:
            if self.csv_file:
                self.csv_file.close()
                self.csv_file = None
                self.csv_writer = None

            if self.current_log_path:
                file_size = os.path.getsize(self.current_log_path)
                elapsed = time.time() - self.session_start if self.session_start else 0
                print(f"🛑 Logging stopped")
                print(f"   File: {self.current_log_path}")
                print(f"   Lines: {self.lines_logged}")
                print(f"   Size: {file_size:,} bytes")
                print(f"   Duration: {elapsed:.1f}s")
                print(f"   Reconnections: {self.reconnect_count}")
                self.current_log_path = None
        except Exception as e:
            print(f"Error stopping logging: {e}")

    async def send_command_raw(self, command):
        """Send command without response handling (for pings)"""
        if not self.connected or not self.client:
            return False
        
        try:
            command_with_newline = command + "\n"
            await self.client.write_gatt_char(CHARACTERISTIC_UUID_RX, command_with_newline.encode())
            return True
        except Exception as e:
            print(f"Raw command send error: {e}")
            return False

    async def send_command(self, command, timeout=30.0):
        """Send command and wait for response"""
        if not self.connected:
            print("❌ Not connected!")
            return False

        self.command_response = ""
        self.command_mode = True
        
        # Special handling for download
        if command.startswith("download "):
            self.download_mode = True
            self.download_content = ""
            timeout = 60.0  # Longer timeout for downloads

        try:
            command_with_newline = command + "\n"
            await self.client.write_gatt_char(CHARACTERISTIC_UUID_RX, command_with_newline.encode())
            
            # Wait for response
            timeout_count = 0
            timeout_iterations = int(timeout * 20)
            
            while (self.command_mode or self.download_mode) and timeout_count < timeout_iterations:
                await asyncio.sleep(0.05)
                timeout_count += 1
            
            if timeout_count >= timeout_iterations:
                print(f"⏱️ Command timeout after {timeout}s!")
                self.command_mode = False
                self.download_mode = False
                return False
            
            return True
            
        except Exception as e:
            print(f"Command error: {e}")
            self.command_mode = False
            self.download_mode = False
            return False

    async def handle_download(self, command_parts):
        """Handle file download"""
        if len(command_parts) < 2:
            print("Usage: download <filename> [output_filename]")
            return

        device_filename = command_parts[1]
        output_filename = command_parts[2] if len(command_parts) >= 3 else device_filename
        
        os.makedirs("logs", exist_ok=True)
        full_output_path = os.path.join("logs", output_filename)

        print(f"📥 Downloading: {device_filename} -> {full_output_path}")
        
        success = await self.send_command(f"download {device_filename}")
        
        if not success:
            print("❌ Download failed")
            return
        
        if self.download_content:
            try:
                with open(full_output_path, 'w', encoding='utf-8') as f:
                    f.write(self.download_content)
                
                file_size = len(self.download_content.encode('utf-8'))
                print(f"✅ Saved: {full_output_path} ({file_size:,} bytes)")
                    
            except Exception as e:
                print(f"❌ Error saving file: {e}")
        else:
            print("❌ No content received")

    async def interactive_mode(self):
        """Interactive command mode while auto-logging"""
        print("\n" + "="*60)
        print("🧠 BrainBallast Auto-Logger")
        print("="*60)
        print("\n📊 Auto-logging sensor data in background")
        print(f"📝 Log file: {self.current_log_path}")
        print("\n💬 Commands:")
        print("  list              - List files on device")
        print("  download <file>   - Download file to logs/ folder")
        print("  tail <file> <n>   - Show last N lines of file")
        print("  size <file>       - Show file size")
        print("  delete <file>     - Delete file")
        print("  info              - SD card info")
        print("  stats             - Show session statistics")
        print("  quit              - Exit (saves log file)")
        print("\n" + "="*60 + "\n")

        async def get_input():
            loop = asyncio.get_event_loop()
            return await loop.run_in_executor(None, input, "> ")

        while True:
            try:
                if not self.connected:
                    print("⚠️  Connection lost. Waiting for reconnection...")
                    await asyncio.sleep(2.0)
                    continue
                
                command = await get_input()
                command = command.strip()
                
                if not command:
                    continue
                
                if command.lower() in ['quit', 'exit', 'q']:
                    break
                elif command.lower() == 'stats':
                    self._show_stats()
                elif command.startswith('download '):
                    command_parts = command.split()
                    await self.handle_download(command_parts)
                elif command:
                    await self.send_command(command)
                    
            except KeyboardInterrupt:
                print("\n👋 Exiting...")
                break
            except Exception as e:
                print(f"Error: {e}")
                await asyncio.sleep(1.0)

    def _show_stats(self):
        """Show session statistics"""
        elapsed = time.time() - self.session_start if self.session_start else 0
        rate = self.lines_logged / elapsed if elapsed > 0 else 0
        
        print(f"\n📊 Session Statistics:")
        print(f"   Connected: {elapsed:.1f}s")
        print(f"   Lines logged: {self.lines_logged:,}")
        print(f"   Bytes received: {self.bytes_received:,}")
        print(f"   Logging rate: {rate:.1f} lines/sec")
        print(f"   Reconnections: {self.reconnect_count}")
        print(f"   Log file: {self.current_log_path}")
        print()


async def main():
    """Main entry point"""
    logger = SimpleBLELogger()
    
    try:
        device = await logger.scan_for_device()
        if not device:
            return
            
        await logger.connect(device)
        await logger.interactive_mode()
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        await logger.disconnect()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 Stopped by user.")