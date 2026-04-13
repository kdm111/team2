# Raspberry Pi Camera & ToF Streaming (WebSocket)

---

# English Documentation

## Overview
This project captures camera frames using OpenCV and distance data from a ToF sensor on a Raspberry Pi, then streams both to a PC using WebSockets in real time.

This document includes full setup instructions, system design rationale, and development flow.

---

## System Design Rationale

### Why Raspberry Pi?

This project required stable sensor operation and real-time video streaming. Raspberry Pi was selected over STM32 for the following reasons:

1. Stable Power Supply for ToF Sensor  
The VL53L0X sensor requires stable 5V power for accurate measurements. Raspberry Pi provides reliable power compared to microcontroller environments.

2. Native Camera Support  
STM32 does not support USB webcams or high-level image processing. Raspberry Pi supports USB cameras and OpenCV.

3. Simultaneous Processing  
The system requires:
- ToF sensor measurement  
- Webcam streaming  
- WebSocket communication  

Raspberry Pi can handle all tasks simultaneously in a Linux environment.

---

## Development Flow

1. ToF Sensor Test  
   - Verify sensor operation  
   - Confirm I2C communication  

2. Webcam Test  
   - Detect camera  
   - Capture frames using OpenCV  

3. Integration  
   - Combine ToF and camera data  

4. Streaming  
   - Send data via WebSocket  

---

## 1. Raspberry Pi Initial Setup

### Enable VNC

```bash
sudo raspi-config
```

Interface Options → VNC → Enable

```bash
sudo reboot
```

---

### Connect via VNC Viewer
- Install VNC Viewer
- Enter Raspberry Pi IP
- Login

---

## 2. Network Setup

### Check Wi-Fi

```bash
iwgetid
ip a
```

---

### Check PC IP

```bash
ipconfig
```

---

### Test Connection

```bash
ping 192.168.xxx.xxx
```

---

## 3. Python Environment

### Create Virtual Environment

```bash
python3 -m venv tof_env
```

### Activate

```bash
source tof_env/bin/activate
```

### Install Libraries

```bash
pip install opencv-python-headless websockets adafruit-circuitpython-vl53l0x
```

---

## 4. Hardware Setup

### Camera

```bash
ls /dev/video*
```

---

### ToF Sensor

```bash
sudo raspi-config
```

Interface Options → I2C → Enable

```bash
i2cdetect -y 1
```

---

## 5. Step-by-Step Execution

### 5.1 ToF Sensor Test

```python
import board
import busio
import adafruit_vl53l0x

i2c = busio.I2C(board.SCL, board.SDA)
sensor = adafruit_vl53l0x.VL53L0X(i2c)

while True:
    print(sensor.range)
```

---

### 5.2 Webcam Test

```python
import cv2

cap = cv2.VideoCapture(0)

while True:
    ret, frame = cap.read()
    cv2.imshow("cam", frame)
    if cv2.waitKey(1) == 27:
        break
```

---

### 5.3 Integration

- Combine camera and ToF data in one loop

---

### 5.4 WebSocket Streaming

```
python
await ws.send(json.dumps({
    "type": "frame",
    "data": b64
}))

```

# 한국어 문서

## 개요
본 프로젝트는 라즈베리파이에서 OpenCV를 활용하여 카메라 영상을 캡처하고 ToF 센서 데이터를 수집한 뒤, WebSocket을 통해 PC로 실시간 전송하는 시스템입니다.

---

## 시스템 설계 배경

### 라즈베리파이를 선택한 이유

1. ToF 센서의 안정적인 전원 공급  
VL53L0X 센서는 정확한 측정을 위해 안정적인 5V 전원이 필요합니다. 라즈베리파이는 이를 안정적으로 제공합니다.

2. 웹캠 지원  
STM32는 웹캠 및 영상 처리를 지원하지 않지만, 라즈베리파이는 OpenCV와 함께 사용 가능합니다.

3. 동시 처리 가능  
- 거리 측정  
- 영상 처리  
- 네트워크 통신  

라즈베리파이는 위 작업을 동시에 수행할 수 있습니다.

---

## 개발 진행 과정

1. ToF 센서 테스트  
2. 웹캠 테스트  
3. 데이터 통합  
4. WebSocket 전송  

---

## 1. 초기 설정

```bash
sudo raspi-config
```

VNC 활성화 후 재부팅

---

## 2. 네트워크 확인

```bash
iwgetid
ip a
```

---

## 3. 가상환경 설정

```bash
python3 -m venv tof_env
source tof_env/bin/activate
```

---

## 4. 라이브러리 설치

```bash
pip install opencv-python-headless websockets adafruit-circuitpython-vl53l0x
```

---

## 5. 단계별 실행

### 5.1 ToF 센서 테스트

```python
print(sensor.range)
```

---

### 5.2 웹캠 테스트

```python
cv2.imshow("cam", frame)
```

---

### 5.3 통합

센서 + 영상 데이터를 하나의 루프로 결합

---

### 5.4 전송

WebSocket을 통해 데이터 송신

---