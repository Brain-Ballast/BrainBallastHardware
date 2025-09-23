import asyncio
import csv
import datetime
import logging
import os
import time
import threading
from bleak import BleakScanner, BleakClient
from bleak.backends.characteristic import BleakGATTCharacteristic

# --- Config ---
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
CHARACTERISTIC_UUID_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" 
CHARACTERISTIC_UUID_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
DEVICE_NAME = "BrainBallast"

OUTPUT_FILE = "sensor_data.csv"

# Performance optimizations
MTU_SIZE = 512  # Request larger MTU for faster transfers
CONNECTION_INTERVAL_MIN = 7.5  # ms - faster connection interval
CONNECTION_INTERVAL_MAX = 15.0  # ms
LATENCY = 0  # No latency for max throughput
SUPERVISION_TIMEOUT = 4000  # ms

# --- Logging ---
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("BTBrain")

class OptimizedBLEDataReceiver:
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
        
        # Heartbeat monitoring
        self.last_heartbeat = time.time()
        self.heartbeat_interval = 5.0  # Send heartbeat every 5 seconds
        self.heartbeat_timeout = 15.0  # Consider dead if no response for 15 seconds
        self.heartbeat_task = None
        self.connection_dead = False
        
        # Performance monitoring
        self.bytes_received = 0
        self.transfer_start_time = None
        self.last_activity = time.time()
        
        # High-speed transfer settings
        self.max_chunk_size = 4096  # Larger chunks for BLE
        self.transfer_timeout = 60.0  # Longer timeout for large transfers
        
        # Command tracking
        self.current_command = None

    async def scan_for_device(self):
        """Scan for the BrainBallast device with timeout"""
        print("Scanning for BLE devices...")
        try:
            devices = await BleakScanner.discover(timeout=10.0)
            print("Found devices:")
            for device in devices:
                print(f"  {device.name or 'Unknown'} - {device.address}")
                if device.name == DEVICE_NAME:
                    print(f"*** Found target device {DEVICE_NAME} at {device.address} ***")
                    return device
            print(f"Device '{DEVICE_NAME}' not found!")
            return None
        except Exception as e:
            print(f"Scan error: {e}")
            return None

    async def connect(self, device):
        """Connect with optimized BLE parameters"""
        print(f"Connecting to {device.address}...")
        try:
            self.client = BleakClient(device.address)
            await self.client.connect()
            
            # Request larger MTU for better throughput
            try:
                await self.client.pair()  # Pair for better performance
            except:
                pass  # Pairing might not be supported
            
            self.connected = True
            self.last_heartbeat = time.time()
            self.last_activity = time.time()
            self.connection_dead = False
            print("Connected!")

            # Start notifications with high priority
            await self.client.start_notify(CHARACTERISTIC_UUID_TX, self.notification_handler)
            
            # Start heartbeat monitoring
            self.heartbeat_task = asyncio.create_task(self.heartbeat_monitor())
            
            # Auto-start logging
            await self.start_logging()
            
            print(f"Connection established with optimized parameters")
            
        except Exception as e:
            print(f"Connection failed: {e}")
            self.connected = False
            raise

    async def disconnect(self):
        """Disconnect gracefully"""
        if self.heartbeat_task:
            self.heartbeat_task.cancel()
            
        if self.client and self.connected:
            await self.stop_logging()
            try:
                await self.client.stop_notify(CHARACTERISTIC_UUID_TX)
                await self.client.disconnect()
            except:
                pass
            self.connected = False
            print("Disconnected.")

    async def heartbeat_monitor(self):
        """Monitor connection health with heartbeat"""
        reconnect_attempts = 0
        
        while self.connected:
            try:
                current_time = time.time()
                
                # Check if we haven't received any data recently
                if current_time - self.last_activity > self.heartbeat_timeout:
                    print("Connection appears dead - no activity detected")
                    self.connection_dead = True
                    
                    # Keep trying to reconnect indefinitely with delays
                    reconnect_attempts += 1
                    print(f"Attempting to reconnect (attempt {reconnect_attempts})...")
                    
                    success = await self.reconnect()
                    if success:
                        print("Reconnection successful!")
                        reconnect_attempts = 0  # Reset counter on success
                        self.connection_dead = False
                        self.last_activity = time.time()
                        continue
                    else:
                        print(f"Reconnection attempt {reconnect_attempts} failed")
                        # Non-blocking delay before next attempt
                        print("Waiting 10 seconds before next reconnection attempt...")
                        await asyncio.sleep(10.0)
                        continue
                
                # Send periodic heartbeat only if connection seems healthy
                if not self.connection_dead and current_time - self.last_heartbeat > self.heartbeat_interval:
                    if not self.waiting_for_response and not self.download_mode:
                        # Send a lightweight test command
                        await self.send_heartbeat()
                        self.last_heartbeat = current_time
                
                await asyncio.sleep(1.0)
                
            except asyncio.CancelledError:
                break
            except Exception as e:
                print(f"Heartbeat monitor error: {e}")
                await asyncio.sleep(2.0)

    async def send_heartbeat(self):
        """Send lightweight heartbeat command"""
        try:
            if self.client and self.connected and not self.connection_dead:
                # Use a simple test command as heartbeat
                await self.client.write_gatt_char(CHARACTERISTIC_UUID_RX, b"test\n")
                return True
        except Exception as e:
            print(f"Heartbeat send failed: {e}")
            self.connection_dead = True
            return False

    async def reconnect(self):
        """Attempt to reconnect when connection is dead"""
        try:
            # Clean up current connection
            if self.client:
                try:
                    await self.client.disconnect()
                except:
                    pass  # Ignore disconnect errors
            
            self.connected = False
            await asyncio.sleep(2.0)  # Wait before scanning
            
            # Re-scan and reconnect
            print("Scanning for device...")
            device = await self.scan_for_device()
            if device:
                await self.connect(device)
                return True
            else:
                print("Device not found during reconnection")
                return False
                
        except Exception as e:
            print(f"Reconnection error: {e}")
            return False

    def notification_handler(self, sender: BleakGATTCharacteristic, data: bytearray):
        """Optimized notification handler for high-speed data"""
        self.last_activity = time.time()
        self.bytes_received += len(data)
        
        try:
            # Decode with error handling for corrupted data
            chunk = data.decode('utf-8', errors='replace')
            self.buffer += chunk
            
            # Process complete lines
            while "\n" in self.buffer:
                line, self.buffer = self.buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue

                self._process_line(line)
                
        except Exception as e:
            print(f"Notification handler error: {e}")

    def _process_line(self, line):
        """Process individual lines efficiently"""
        # Handle download mode with high-speed processing
        if self.download_mode:
            if "End of file" in line or "=== End of file ===" in line:
                self.download_mode = False
                self.waiting_for_response = False
                if self.transfer_start_time:
                    elapsed = time.time() - self.transfer_start_time
                    speed = self.bytes_received / elapsed / 1024 if elapsed > 0 else 0
                    print(f"Transfer complete: {self.bytes_received} bytes in {elapsed:.1f}s ({speed:.1f} KB/s)")
            elif not line.startswith("===") and not line.startswith("Downloading"):
                self.download_content += line + "\n"
            return

        # Handle command responses
        if self.waiting_for_response:
            if line.startswith("Received:"):
                return
            
            self.command_response += line + "\n"
            print(line)
            
            # More specific end response detection for tail commands
            if self.current_command and self.current_command.startswith("tail"):
                # For tail commands, stop after we see the data section
                if line.startswith("===") and ("Last" in line or "lines of" in line):
                    # This is the header, keep going
                    return
                # If we see an empty line or another command marker, we're done
                if line.strip() == "" or line.startswith("===") or "Command completed" in line:
                    self.waiting_for_response = False
                    return
            
            # End response detection for other commands
            end_markers = [
                "End of file list", "End of file", "bytes)", "SD Card Info:",
                "Usage:", "Free:", "Unknown command:", "File not found:",
                "Deleted:", "Failed to delete:", "Invalid line count:", "Usage: tail",
                "Test response:", "Command completed"
            ]
            
            if any(marker in line for marker in end_markers):
                self.waiting_for_response = False
            return

        # Log sensor data efficiently
        if self.logging_active and self.csv_writer and "," in line:
            try:
                row = [field.strip() for field in line.split(",")]
                self.csv_writer.writerow(row)
                if hasattr(self.csv_file, 'flush'):
                    self.csv_file.flush()
            except Exception as e:
                print(f"CSV write error: {e}")

    async def send_command(self, command, timeout=None):
        """Send command with optimized timeout handling"""
        if not self.connected or self.connection_dead:
            print("Not connected or connection is dead!")
            return False

        # Store current command for processing logic
        self.current_command = command.strip()

        # Set timeout based on command type
        if timeout is None:
            if command.startswith("download"):
                timeout = self.transfer_timeout
            elif command.startswith("tail"):
                timeout = 20.0  # Shorter timeout for tail commands
            else:
                timeout = 10.0

        self.command_response = ""
        self.waiting_for_response = True
        
        # Handle download command specially
        if command.startswith("download "):
            self.download_mode = True
            self.download_content = ""
            self.bytes_received = 0
            self.transfer_start_time = time.time()
            parts = command.split()
            if len(parts) >= 2:
                self.download_filename = parts[1]

        try:
            command_with_newline = command + "\n"
            await self.client.write_gatt_char(CHARACTERISTIC_UUID_RX, command_with_newline.encode())
            
            # Wait for response with shorter intervals for better responsiveness
            timeout_count = 0
            timeout_iterations = int(timeout * 20)  # Check every 50ms instead of 100ms
            
            while self.waiting_for_response and timeout_count < timeout_iterations:
                await asyncio.sleep(0.05)  # 50ms intervals
                timeout_count += 1
                
                # Check for connection death during long operations
                if self.connection_dead:
                    print("Connection died during command execution")
                    return False
            
            if timeout_count >= timeout_iterations:
                print(f"Command timeout after {timeout}s!")
                self.waiting_for_response = False
                self.download_mode = False
                return False
            
            return True
            
        except Exception as e:
            print(f"Command send error: {e}")
            self.waiting_for_response = False
            self.download_mode = False
            return False
        finally:
            self.current_command = None

    async def start_logging(self):
        """Start logging with logs directory"""
        if self.logging_active:
            print("Logging already active!")
            return

        os.makedirs("logs", exist_ok=True)
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.current_log_path = os.path.join("logs", f"{timestamp}_{OUTPUT_FILE}")

        try:
            self.csv_file = open(self.current_log_path, "w", newline="")
            self.csv_writer = csv.writer(self.csv_file, quoting=csv.QUOTE_MINIMAL)
            
            self.logging_active = True
            print(f"📝 Started logging to: {self.current_log_path}")
        except Exception as e:
            print(f"Failed to start logging: {e}")

    async def stop_logging(self):
        """Stop logging safely"""
        if not self.logging_active:
            print("Logging not active!")
            return

        try:
            if self.csv_file:
                self.csv_file.close()
                self.csv_file = None
                self.csv_writer = None

            self.logging_active = False
            print(f"🛑 Stopped logging. File saved: {self.current_log_path}")
            self.current_log_path = None
        except Exception as e:
            print(f"Error stopping logging: {e}")

    async def handle_download_command(self, command_parts):
        """Handle download with logs directory and speed optimization"""
        if len(command_parts) < 2:
            print("Usage: download <filename> [output_filename]")
            return

        device_filename = command_parts[1]
        
        # Determine output filename - always save to logs directory
        if len(command_parts) >= 3:
            output_filename = command_parts[2]
        else:
            output_filename = device_filename
        
        # Ensure logs directory exists
        os.makedirs("logs", exist_ok=True)
        
        # Always save to logs directory
        full_output_path = os.path.join("logs", output_filename)

        print(f"📥 Starting download: {device_filename} -> {full_output_path}")
        
        # Send download command to device
        success = await self.send_command(f"download {device_filename}")
        
        if not success:
            print("❌ Download command failed")
            return
        
        # Save downloaded content to logs directory
        if self.download_content:
            try:
                with open(full_output_path, 'w', encoding='utf-8') as f:
                    f.write(self.download_content)
                
                file_size = len(self.download_content.encode('utf-8'))
                print(f"✅ File saved: {full_output_path} ({file_size} bytes)")
                
                # Show transfer statistics
                if self.transfer_start_time:
                    elapsed = time.time() - self.transfer_start_time
                    speed = file_size / elapsed / 1024 if elapsed > 0 else 0
                    print(f"📊 Transfer stats: {elapsed:.1f}s, {speed:.1f} KB/s")
                    
            except Exception as e:
                print(f"❌ Error saving file: {e}")
        else:
            print("❌ No content received or file not found on device")

    def is_connection_healthy(self):
        """Check if connection is healthy"""
        if not self.connected:
            return False
        
        current_time = time.time()
        return (current_time - self.last_activity) < self.heartbeat_timeout and not self.connection_dead

    async def interactive_mode(self):
        """Enhanced interactive mode with connection monitoring"""
        print("\n=== BrainBallast Data Logger & Commander (Optimized) ===")
        print("Connection Status: Connected with heartbeat monitoring")
        print("\nLogging Controls:")
        print("  log / listen - Start logging sensor data to timestamped file")
        print("  stop - Stop current logging session")
        print("\nDevice Commands:")
        print("  list - List all files on device")
        print("  download <filename> [output] - Download file to logs/ folder")
        print("  tail <filename> <lines> - Show last N lines of file")
        print("  size <filename> - Show file size")
        print("  delete <filename> - Delete file") 
        print("  info - Show SD card information")
        print("  status - Show connection status")
        print("  quit - Exit")
        print(f"\nAuto-started logging to: {self.current_log_path}")
        print("All downloads automatically saved to logs/ directory")
        print("Note: Connection will automatically reconnect if lost")
        print("Type commands and press Enter:")

        async def get_input():
            loop = asyncio.get_event_loop()
            return await loop.run_in_executor(None, input, "> ")

        while True:  # Keep running even if connection dies temporarily
            try:
                # Skip input if connection is dead and reconnecting
                if self.connection_dead:
                    print("Connection lost - waiting for reconnection...")
                    await asyncio.sleep(5.0)
                    continue
                
                if not self.connected:
                    print("Connection terminated. Use Ctrl+C to exit.")
                    await asyncio.sleep(2.0)
                    continue
                
                command = await get_input()
                command = command.strip()
                
                if command.lower() in ['quit', 'exit', 'q']:
                    break
                elif command.lower() in ['log', 'listen']:
                    await self.start_logging()
                elif command.lower() == 'stop':
                    await self.stop_logging()
                elif command.lower() == 'status':
                    self._show_connection_status()
                elif command.lower() == 'help':
                    self._show_help()
                elif command.startswith('download '):
                    if not self.connection_dead:
                        command_parts = command.split()
                        await self.handle_download_command(command_parts)
                    else:
                        print("Cannot download while connection is being restored")
                elif command:
                    # Send other commands to device
                    if not self.connection_dead:
                        await self.send_command(command)
                    else:
                        print("Cannot send commands while connection is being restored")
                    
            except KeyboardInterrupt:
                print("\nExiting...")
                break
            except Exception as e:
                print(f"Error in interactive mode: {e}")
                await asyncio.sleep(1.0)  # Brief pause before continuing

    def _show_connection_status(self):
        """Show detailed connection status"""
        current_time = time.time()
        last_activity_ago = current_time - self.last_activity
        
        print(f"\n📡 Connection Status:")
        print(f"  Connected: {'✅ Yes' if self.connected else '❌ No'}")
        print(f"  Health: {'✅ Good' if self.is_connection_healthy() else '⚠️ Poor'}")
        print(f"  Last activity: {last_activity_ago:.1f}s ago")
        print(f"  Bytes received: {self.bytes_received:,}")
        print(f"  Logging: {'✅ Active' if self.logging_active else '❌ Inactive'}")

    def _show_help(self):
        """Show help information"""
        print("\n📚 Available commands:")
        print("  log/listen - Start new logging session")
        print("  stop - Stop current logging") 
        print("  list - List files on device")
        print("  download <file> [output] - Download file to logs/ folder")
        print("  tail <file> <lines> - Show last N lines")
        print("  size <file> - Show file size")
        print("  delete <file> - Delete file")
        print("  info - SD card info")
        print("  status - Connection status")
        print("  quit - Exit")

# --- Arduino Firmware Optimizations ---
arduino_optimizations = '''
// Add these optimizations to your Connection.ino:

// In connectionSetup(), add these BLE optimizations:
void connectionSetup() {
    // ... existing setup code ...
    
    // Request optimal connection parameters for high throughput
    esp_ble_conn_update_params_t conn_params = {0};
    conn_params.min_int = 6;     // 7.5ms (6 * 1.25ms)
    conn_params.max_int = 12;    // 15ms (12 * 1.25ms)  
    conn_params.latency = 0;     // No latency for max throughput
    conn_params.timeout = 400;   // 4000ms (400 * 10ms)
    
    // Apply when client connects
    esp_ble_gap_update_conn_params(&conn_params);
    
    // Request larger MTU
    esp_ble_gatt_set_local_mtu(517); // Max BLE MTU
    
    // ... rest of setup ...
}

// Optimize btSendData for maximum throughput:
bool btSendData(const char* data) {
    if (!bleConnected || !bleInitialized) {
        return false;
    }
    
    int dataLen = strlen(data);
    const int maxChunk = 512;  // Larger chunks, no delays
    
    for (int i = 0; i < dataLen; i += maxChunk) {
        int chunkLen = min(maxChunk, dataLen - i);
        String chunk = String(data).substring(i, i + chunkLen);
        
        pTxCharacteristic->setValue(chunk.c_str());
        pTxCharacteristic->notify();
        
        // Only delay if we need to prevent buffer overflow
        if (chunkLen == maxChunk) {
            delayMicroseconds(100); // Minimal delay
        }
    }
    
    return true;
}

// Optimize storageDownloadFile for speed:
void storageDownloadFile(String filename) {
    // ... file opening code ...
    
    // Use much larger buffer for faster reads
    char buffer[2048];  // 2KB buffer instead of 512B
    int totalSent = 0;
    
    while (file.available()) {
        int bytesRead = file.read((uint8_t*)buffer, sizeof(buffer) - 1);
        buffer[bytesRead] = '\\0';
        
        // Send immediately, no delays
        btSendData(buffer);
        totalSent += bytesRead;
        
        // Only yield CPU occasionally  
        if (totalSent % 8192 == 0) {
            yield(); // Let other tasks run briefly
        }
    }
    
    // ... rest of function ...
}
'''

# --- Main Functions ---
async def connect_and_run_interactive():
    """Connect with optimized parameters and run interactive mode"""
    receiver = OptimizedBLEDataReceiver()
    
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
    """Connect and run single command"""
    receiver = OptimizedBLEDataReceiver()
    
    try:
        device = await receiver.scan_for_device()
        if not device:
            return
            
        await receiver.connect(device)
        
        if command.lower() in ['log', 'listen']:
            result = await receiver.start_logging()
        elif command.lower() == 'stop':
            result = await receiver.stop_logging() 
        elif command.startswith('download '):
            command_parts = command.split()
            await receiver.handle_download_command(command_parts)
            result = "Download completed"
        else:
            await receiver.send_command(command)
            result = "Command completed"
            
        print(f"Result: {result}")
        
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
            print("Running in log-only mode...")
            asyncio.run(connect_and_run_interactive())
        else:
            # Run single command
            command = " ".join(sys.argv[1:])
            asyncio.run(connect_and_run_command(command))
    else:
        # Run interactive mode
        try:
            asyncio.run(connect_and_run_interactive())
        except KeyboardInterrupt:
            print("Stopped by user.")