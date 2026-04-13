import asyncio
import base64
import json
import cv2
import board
import busio
import adafruit_vl53l0x
import websockets

# --- Configuration ---
SERVER_IP   = "192.168.100.118"   # Change to PC IP!
SERVER_PORT = 8765
URI         = f"ws://{SERVER_IP}:{SERVER_PORT}/ws/raspberry"

# --- I2C / ToF sensor initialization ---
i2c  = busio.I2C(board.SCL, board.SDA)
vl53 = adafruit_vl53l0x.VL53L0X(i2c)

# --- Camera initialization ---
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

async def send_loop():
    """
    Main loop for capturing camera/sensor data and sending via WebSockets.
    Includes auto-reconnect logic if the server connection fails.
    """
    while True:
        try:
            print(f"[Pi] Connecting to server... {URI}")
            async with websockets.connect(URI) as ws:
                print("[Pi] Connected successfully!")

                while True:
                    # 1. Capture and encode camera frame
                    ret, frame = cap.read()
                    if ret:
                        # Encode frame as JPEG with 60% quality
                        _, encoded = cv2.imencode(
                            '.jpg', frame,
                            [cv2.IMWRITE_JPEG_QUALITY, 60]
                        )
                        # Convert to base64 string for JSON transmission
                        b64 = base64.b64encode(encoded.tobytes()).decode()
                        
                        await ws.send(json.dumps({
                            "type":  "frame",
                            "data":  b64,
                            "width": frame.shape[1],
                            "height": frame.shape[0],
                        }))

                    # 2. Read ToF sensor distance value
                    distance = vl53.range
                   
                    await ws.send(json.dumps({
                        "type":     "tof",
                        "distance": distance,
                    }))

                    # Control transmission rate (~20fps)
                    await asyncio.sleep(0.05)

        except (websockets.exceptions.ConnectionClosed, OSError) as e:
            # Handle server disconnection or connection refusal
            print(f"[Pi] Connection lost or server offline: {e}")
            print("[Pi] Retrying in 3 seconds...")
            await asyncio.sleep(3)
            
        except Exception as e:
            # Handle any other unexpected errors
            print(f"[Pi] Unexpected error: {e}")
            await asyncio.sleep(3)

if __name__ == "__main__":
    try:
        asyncio.run(send_loop())
    except KeyboardInterrupt:
        # Clean up resources on manual exit (Ctrl+C)
        print("[Pi] Interrupted by user. Closing...")
        cap.release()
