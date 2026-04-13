# 🏎️ GestDrive: WebSocket 기반 RC카 원격 제어 시스템

본 프로젝트는 **ESP32**와 **STM32F411RE**를 연동하여, 웹 환경에서 저지연(Low-latency)으로 실시간 조종이 가능한 1/10 스케일 RC카 제어 시스템입니다.

---

## ✨ 주요 특징 (Key Features)

* **저지연 실시간 제어**: WebSocket 프로토콜을 사용하여 웹 대시보드와 하드웨어 간의 응답 속도를 극대화했습니다.
* **이중화 MCU 아키텍처**: 통신 전담(ESP32)과 제어 전담(STM32) 보드를 분리하여 시스템 안정성과 실시간성을 확보했습니다.
* **안전 기동 로직**: ESC Arming 시퀀스와 타각 제한(Clamp) 로직을 통해 하드웨어 파손 및 오작동을 방지합니다.

---

## 🏗️ 시스템 아키텍처 (System Architecture)

### 1. HW 구조도
전체적인 하드웨어 구조도 입니다.
<img width="7981" height="5880" alt="hw_architecture" src="https://github.com/user-attachments/assets/e8614fd4-d851-4b8d-a153-1accf75d189b" />

### 1. 전체 데이터 통신 흐름
사용자의 조작이 웹 서버를 거쳐 하드웨어까지 도달하는 데이터 경로입니다.
<img width="7375" height="1130" alt="플로우차트" src="https://github.com/user-attachments/assets/c9898aba-a471-4868-bd3d-56b519819b10" />


### 2. STM32 내부 제어 알고리즘
하드웨어의 안전한 기동과 실시간 데이터 처리를 위한 소프트웨어 흐름도입니다.

<img width="5173" height="4623" alt="플로우차트3" src="https://github.com/user-attachments/assets/4aedc76e-759d-4ab2-8b58-786e671b37ef" />



---

## 🔌 하드웨어 연결 및 회로도 (Wiring)

실제 하드웨어 배선은 아래 핀 맵을 기반으로 구성되었습니다.

| 부품 (ESP32) | 제어 보드 (STM32F411RE) | 역할 |
| :--- | :--- | :--- |
| **GPIO17 (TX2)** | **PA10 (UART1 RX)** | 제어 신호 전송 (ESP -> STM) |
| **GPIO16 (RX2)** | **PA9 (UART1 TX)** | 데이터 피드백 (선택 사항) |
| **GND** | **GND** | 공통 그라운드 (필수) |

> **⚠️ 주의사항**
> * **공통 그라운드**: 전위차로 인한 통신 에러 방지를 위해 두 보드의 GND를 반드시 하나로 연결해야 합니다.
> * **외부 전원**: ESC 및 서보 모터는 전력 소모가 크므로, STM32 보드 전원이 아닌 별도의 외부 배터리 연결이 필수입니다.

---

## 📡 통신 프로토콜 & 캘리브레이션

### 1. ESP32 → STM32 (ASCII)
JSON 데이터를 파싱하기 쉬운 문자열 포맷으로 변환하여 송신합니다.
* **데이터 포맷**: `ESC=1540  SERVO=1915\n`
* **ESC**: 주행 속도 (1400: 전진 ~ 1540: 정지 ~ 1800: 후진)
* **SERVO**: 조향 각도 (1400: 우측 끝 ~ 1915: 영점 ~ 2400: 좌측 끝)

### 2. 캘리브레이션 정보
* **조향 영점**: `1915` (기구 조립 상태에 따른 최적 교정값 반영)
* **Deadzone**: 조이스틱 미세 떨림 방지를 위한 `+/- 100` 유격 설정
* **ESC 중립**: `1540` (기기 상태에 따라 1500으로 조정 가능)

---

## 📂 디렉토리 구조 (Directory Structure)

```text
Project-Root
 ┣ 📂 ESP32_Client       # WebSocket 수신 및 UART 송신 코드 (Arduino/C++)
 ┣ 📂 STM32_Firmware     # 모터 제어 및 파싱 로직 (Bare-metal C)
 ┣ 📂 Images             # 회로도 및 플로우차트 이미지 자산
 ┗ 📜 README.md          # 프로젝트 설명서
```

---

## 💻 빌드 및 설치 (Build & Installation)

### 🛠️ STM32 (Bare-metal C)
* **Toolchain**: GNU Toolchain & Makefile
* **Commands**:
```bash
make clean
make
make flash
```

### 📶 ESP32 (Arduino / PlatformIO)
* **필수 라이브러리**: `WebSockets`, `ArduinoJson`
* **설정**:
  1. 소스 코드 내 `WIFI_SSID`, `WIFI_PASS`, `SERVER_HOST` 수정
  2. 본인 환경에 맞춰 수정 후 보드에 업로드

---

## 💡 트러블 슈팅 (Troubleshooting)

### ⚠️ UART 데이터 수신 시 간헐적 패킷 유실 발생
* **원인**: 수신 데이터 노이즈 및 처리 속도 불일치로 인한 오작동.
* **해결**:
  * **GND 공통화**: 두 보드의 GND를 하나로 연결하여 통신 전위차 해결.
  * **로직 개선**: `sscanf` 방식의 전체 파싱 대신 **스트리밍 파싱(Streaming Parsing)** 방식을 도입. 데이터가 일부 유실되더라도 키워드(E, S)를 기준으로 즉시 복구되도록 구현.

---

## 🚀 향후 과제 (Future Roadmap)

* [ ] **FPV 카메라 연동**: ESP32-CAM을 활용한 실시간 영상 스트리밍 조종 구현.
* [ ] **자율 주행**: TOF 센서(VL53L0X) 기반의 장애물 자동 회피 및 신호 강도 필터링 로직 추가.
* [ ] **전용 앱 개발**: 웹 대시보드 외 모바일 전용 컨트롤러 앱 구축.
