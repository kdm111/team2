"""
GestDrive WebSocket Server  v5
"""
import os
os.environ["KMP_DUPLICATE_LIB_OK"] = "TRUE"
import statistics # 상단에 import 추가

import asyncio
import json
import logging
import threading
import time
from collections import deque
from contextlib import asynccontextmanager
from typing import Optional
from concurrent.futures import ThreadPoolExecutor

import numpy as np
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse

logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")
log = logging.getLogger("gestdrive")

# ─── Grounding DINO ─────────────────────────────────────
import base64, io

gdino_processor = None
gdino_model     = None
GDINO_AVAILABLE = False

GDINO_PROMPT     = "person . car . bicycle . obstacle . wall . door ."
GDINO_THRESHOLD  = 0.30
GDINO_FRAME_SKIP = 3      # 15Hz → ~5Hz
GDINO_MAX_SIZE   = 640

gdino_executor    = ThreadPoolExecutor(max_workers=1)
gdino_frame_count = 0

# 객체 추적 모드: 타겟 라벨이 설정되면 화면 중앙으로 서보 조정
track_target: str | None = None   # None이면 추적 꺼짐
TRACK_DEAD_ZONE  = 0.08   # 중앙 ±8% 이내면 보정 없음
TRACK_SERVO_STEP = 60     # 1회 보정 서보 이동량 (μs)

def load_gdino():
    global gdino_processor, gdino_model, GDINO_AVAILABLE
    print("\n" + "━"*50)
    print("  Grounding DINO 로딩 중... (tiny, CPU)")
    print("━"*50)
    try:
        from transformers import AutoProcessor, AutoModelForZeroShotObjectDetection
        model_id = "IDEA-Research/grounding-dino-tiny"
        gdino_processor = AutoProcessor.from_pretrained(model_id)
        gdino_model     = AutoModelForZeroShotObjectDetection.from_pretrained(model_id)
        gdino_model.eval()
        GDINO_AVAILABLE = True
        print("  ✅ Grounding DINO 로드 완료")
        print("━"*50 + "\n")
    except ImportError:
        print("  ❌ transformers 미설치  (pip install transformers torch pillow)")
        print("━"*50 + "\n")
    except Exception as e:
        print(f"  ❌ DINO 로드 실패: {e}")
        print("━"*50 + "\n")


def _run_gdino(jpeg_bytes: bytes, orig_w: int, orig_h: int) -> list:
    if not GDINO_AVAILABLE:
        return []
    try:
        import torch
        from PIL import Image
        img = Image.open(io.BytesIO(jpeg_bytes)).convert("RGB")
        w, h = img.size
        scale = min(GDINO_MAX_SIZE / max(w, h), 1.0)
        if scale < 1.0:
            img = img.resize((int(w*scale), int(h*scale)), Image.LANCZOS)
        inputs = gdino_processor(images=img, text=GDINO_PROMPT, return_tensors="pt")
        import torch
        with torch.no_grad():
            outputs = gdino_model(**inputs)
        try:
            results = gdino_processor.post_process_grounded_object_detection(
                outputs, inputs["input_ids"],
                threshold=GDINO_THRESHOLD,
                target_sizes=[img.size[::-1]])[0]
        except TypeError:
            results = gdino_processor.post_process_grounded_object_detection(
                outputs, inputs["input_ids"],
                box_threshold=GDINO_THRESHOLD,
                text_threshold=GDINO_THRESHOLD,
                target_sizes=[img.size[::-1]])[0]
        detections = []
        for box, score, label in zip(
                results["boxes"].tolist(),
                results["scores"].tolist(),
                results["labels"]):
            if scale < 1.0:
                box = [c / scale for c in box]
            box = [max(0,min(box[0],orig_w)), max(0,min(box[1],orig_h)),
                   max(0,min(box[2],orig_w)), max(0,min(box[3],orig_h))]
            detections.append({"label": label, "score": round(float(score),3),
                                "box": [round(c) for c in box]})
        if detections:
            print(f"[DINO] {len(detections)}개 검출: {[d['label'] for d in detections]}", flush=True)
        return detections
    except Exception as e:
        print(f"[DINO] ✗ 추론 오류: {e}", flush=True)
        return []


async def _track_object(dets: list, img_w: int):
    """
    타겟 객체가 검출되면 화면 중앙 기준으로 서보를 조정.
    긴급정지 중이거나 주행 중 아니면 무시.
    """
    global track_target
    if not track_target or tof_emergency_stop:
        return

    # 타겟 라벨 중 confidence 가장 높은 것
    candidates = [d for d in dets if track_target in d["label"]]
    if not candidates:
        return
    best = max(candidates, key=lambda d: d["score"])

    box = best["box"]
    cx  = (box[0] + box[2]) / 2        # 객체 중심 x (원본 픽셀)
    norm = (cx / img_w) - 0.5           # -0.5(왼쪽) ~ +0.5(오른쪽)

    if abs(norm) <= TRACK_DEAD_ZONE:    # 중앙에 있으면 패스
        return

    # 현재 서보 값에서 ±step 보정
    cur_servo = current_cmd.get("servo", SERVO_MID)
    step      = int(norm * 2 * TRACK_SERVO_STEP)   # norm에 비례
    new_servo = max(SERVO_MIN, min(SERVO_MAX, cur_servo + step))

    print(f"[TRACK] {track_target}  cx_norm={norm:+.2f}  servo {cur_servo}→{new_servo}μs")
    await manager.send_to_car({
        "type":      "control",
        "direction": current_cmd.get("direction", "stop"),
        "esc":       current_cmd.get("esc", ESC_MID),
        "servo":     new_servo,
    })



# ─── Whisper ──────────────────────────────────────────
whisper_model     = None
WHISPER_AVAILABLE = False

def load_whisper():
    global whisper_model, WHISPER_AVAILABLE
    print("\n" + "━"*50)
    print("  Whisper 로딩 중... (faster-whisper tiny, CPU int8)")
    print("━"*50)
    try:
        from faster_whisper import WhisperModel
        whisper_model = WhisperModel("tiny", device="cpu", compute_type="int8")
        WHISPER_AVAILABLE = True
        print("  ✅ Whisper 로드 완료")
        print("━"*50 + "\n")
    except ImportError:
        print("  ❌ faster-whisper 미설치  (pip install faster-whisper)")
        print("━"*50 + "\n")
    except Exception as e:
        print(f"  ❌ Whisper 로드 실패: {e}")
        print("━"*50 + "\n")

def _run_whisper(samples: np.ndarray) -> str:
    if not WHISPER_AVAILABLE or whisper_model is None:
        print("[Whisper] ✗ WHISPER_AVAILABLE=False", flush=True)
        return ""
    try:
        audio = samples.astype(np.float32) / 32768.0
        rms = float(np.sqrt(np.mean(audio ** 2)))
        db  = 20 * np.log10(rms + 1e-10)
        print(f"[Whisper] 레벨: {rms:.5f} ({db:+.1f} dB)  samples={len(audio)}", flush=True)
        if rms < 0.0008:
            print(f"[Whisper] ⚠️  신호 약함", flush=True)
            return ""
        print("[Whisper] ⚙️  추론 시작 ...", flush=True)
        segs, _ = whisper_model.transcribe(
            audio, language="ko", beam_size=5, vad_filter=False, word_timestamps=False)
        seg_list = list(segs)
        print(f"[Whisper] 세그먼트 수: {len(seg_list)}", flush=True)
        if not seg_list:
            print("[Whisper] ⚠️  세그먼트 없음", flush=True)
            return ""
        text = "".join(s.text for s in seg_list).strip()
        print(f"[Whisper] 원문: {repr(text)}", flush=True)
        return text
    except Exception as e:
        print(f"[Whisper] ✗ 예외: {type(e).__name__}: {e}", flush=True)
        return ""

# ─── 오디오 설정 ──────────────────────────────────────
SAMPLE_RATE      = 16000
MAX_BUFFER_SAMP  = SAMPLE_RATE * 15
CHUNK_SAMP       = 1600

executor         = ThreadPoolExecutor(max_workers=1)
audio_buffer: list[np.ndarray] = []
last_whisper_ts: float = 0.0

# ─── ToF 안전 시스템 ──────────────────────────────────
TOF_RING_SIZE    = 10     # 링버퍼 크기
TOF_DANGER_CM    = 25     # 위험 거리 (cm)
TOF_DANGER_COUNT = 5      # 연속 N회 → 긴급 정지
TOF_SEND_INTERVAL = 0.1   # 제어 재전송 주기 (초)

tof_ring: deque         = deque(maxlen=TOF_RING_SIZE)
tof_emergency_stop: bool = False

# ─── 주행 제어 상태 ───────────────────────────────────
ESC_MID   = 1540;  ESC_MIN  = 1400;  ESC_MAX  = 1800
SERVO_MID = 1440;  SERVO_MIN = 950;  SERVO_MAX = 1700

NEUTRAL_CMD: dict = {
    "type": "control", "direction": "stop",
    "throttle": 0.0, "steering": 0.0,
    "esc": ESC_MID, "servo": SERVO_MID,
}
current_cmd:  dict = NEUTRAL_CMD.copy()
drive_active: bool = False

# ─── 스피커 ───────────────────────────────────────────
_spk_buf: deque = deque()
SPEAKER_ENABLED = False

def speaker_worker():
    global SPEAKER_ENABLED
    try:
        import sounddevice as sd
        play_buf = np.zeros(0, dtype=np.int16)
        buf_lock = threading.Lock()

        def _callback(outdata, frames, time_info, status):
            nonlocal play_buf
            with buf_lock:
                avail = len(play_buf)
                if avail >= frames:
                    outdata[:, 0] = play_buf[:frames]
                    play_buf = play_buf[frames:]
                else:
                    outdata[:avail, 0] = play_buf
                    outdata[avail:, 0] = 0
                    play_buf = np.zeros(0, dtype=np.int16)

        print(f"[SPEAKER] 콜백 스피커 시작  {SAMPLE_RATE}Hz Mono")
        SPEAKER_ENABLED = True
        with sd.OutputStream(samplerate=SAMPLE_RATE, channels=1, dtype="int16",
                             blocksize=CHUNK_SAMP, callback=_callback, latency="low"):
            while True:
                try:
                    pcm = _spk_buf.popleft()
                    if pcm is None: break
                    s = np.frombuffer(pcm, dtype=np.int16).copy()
                    with buf_lock:
                        play_buf = np.concatenate([play_buf, s])
                        if len(play_buf) > SAMPLE_RATE:
                            play_buf = play_buf[-SAMPLE_RATE:]
                except IndexError:
                    time.sleep(0.005)
    except ImportError:
        print("[SPEAKER] sounddevice 미설치  (pip install sounddevice)")
    except Exception as e:
        print(f"[SPEAKER] 오류: {e}")
    finally:
        SPEAKER_ENABLED = False

# ─── 주행 제어 함수 ───────────────────────────────────
async def set_current_cmd(cmd: dict):
    """음성/대시보드 명령 → current_cmd 갱신 + 즉시 전송"""
    global current_cmd, drive_active, tof_emergency_stop
    is_stop = (cmd.get("direction") == "stop" or
               (cmd.get("throttle", 0) == 0 and
                cmd.get("esc", ESC_MID) == ESC_MID))
    if tof_emergency_stop and not is_stop:
        print(f"[TOF] 긴급정지 중 — 명령 무시: {cmd.get('direction')}")
        return
    # esc/servo 항상 포함 (ESP32 Direct PWM 모드 보장)
    cmd.setdefault("esc",   ESC_MID)
    cmd.setdefault("servo", SERVO_MID)
    current_cmd  = cmd
    drive_active = not is_stop
    await manager.send_to_car({
        "type":      "control",
        "direction": cmd.get("direction", "stop"),
        "esc":       cmd["esc"],
        "servo":     cmd["servo"],
    })
    print(f"[DRIVE] {'▶ 주행' if drive_active else '■ 정지'}  "
          f"dir={cmd.get('direction')}  "
          f"ESC={cmd.get('esc')}μs  SERVO={cmd.get('servo')}μs")

import statistics # 상단에 import 추가

async def drive_loop():
    global tof_emergency_stop, current_cmd, drive_active

    while True:
        await asyncio.sleep(TOF_SEND_INTERVAL)

        if len(tof_ring) >= 5: # 최소 데이터가 쌓였을 때만 판단
            # [개선 2] 중간값 필터 사용 (튀는 값 한두 개는 무시함)
            recent_list = list(tof_ring)
            median_dist = statistics.median(recent_list)
            
            # 위험 거리 판단 (15cm)
            danger_threshold = TOF_DANGER_CM * 10
            # 해제 거리 판단 (25cm) - 정지와 해제 사이에 간격을 두어 덜덜거림 방지
            release_threshold = (TOF_DANGER_CM + 10) * 10 

            if median_dist < danger_threshold and not tof_emergency_stop:
                tof_emergency_stop = True
                drive_active       = False
                current_cmd        = NEUTRAL_CMD.copy()
                await manager.send_to_car(NEUTRAL_CMD)
                await manager.broadcast({
                    "source": "tof", "type": "emergency_stop",
                    "reason": f"ToF 위험 감지: {median_dist/10:.1f}cm",
                    "ts": time.time(),
                })
                print(f"\n[TOF] ⚠️ 긴급 정지! 중간값: {median_dist/10:.1f}cm")

            elif median_dist > release_threshold and tof_emergency_stop:
                tof_emergency_stop = False
                # 즉시 출발하지 않고 중립 상태 유지 (사용자가 다시 명령 내릴 때까지 대기)
                print(f"[TOF] ✅ 긴급 정지 해제 (거리 확보: {median_dist/10:.1f}cm)")

        # 주행 유지 로직
        if drive_active and not tof_emergency_stop and manager.car is not None:
            # 주행 중일 때만 현재 명령 재전송
            await manager.send_to_car(current_cmd)

# ─── lifespan ─────────────────────────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    loop = asyncio.get_running_loop()
    await loop.run_in_executor(executor, load_whisper)
    await loop.run_in_executor(gdino_executor, load_gdino)
    threading.Thread(target=speaker_worker, daemon=True).start()
    drive_task = asyncio.create_task(drive_loop())

    yield

    drive_task.cancel()
    _spk_buf.append(None)
    log.info("서버 종료")

app = FastAPI(title="GestDrive Server v5", lifespan=lifespan)

# ─── 연결 관리자 ──────────────────────────────────────
class ConnectionManager:
    def __init__(self):
        self.raspberry: Optional[WebSocket] = None
        self.mic:       Optional[WebSocket] = None
        self.car:       Optional[WebSocket] = None
        self.dashboards: list[WebSocket]    = []

    async def broadcast(self, payload: dict):
        msg  = json.dumps(payload, ensure_ascii=False)
        dead = []
        for ws in self.dashboards:
            try:    await ws.send_text(msg)
            except: dead.append(ws)
        for ws in dead: self.dashboards.remove(ws)

    async def broadcast_binary(self, data: bytes):
        dead = []
        for ws in self.dashboards:
            try:    await ws.send_bytes(data)
            except: dead.append(ws)
        for ws in dead: self.dashboards.remove(ws)

    async def send_to_car(self, payload: dict):
        if self.car is None:
            return
        try:
            await self.car.send_text(json.dumps(payload))
        except Exception as e:
            log.error(f"Car 전송 실패: {e}")
            self.car = None

    async def push_status(self):
        await self.broadcast({
            "type": "status",
            "connections": {
                "raspberry":  self.raspberry is not None,
                "mic":        self.mic       is not None,
                "car":        self.car       is not None,
                "dashboards": len(self.dashboards),
            },
            "whisper_available": WHISPER_AVAILABLE,
            "speaker_enabled":   SPEAKER_ENABLED,
            "tof_emergency":     tof_emergency_stop,
            "ts": time.time(),
        })

manager = ConnectionManager()

# ─── /ws/raspberry ────────────────────────────────────
@app.websocket("/ws/raspberry")
async def ws_raspberry(ws: WebSocket):
    await ws.accept()
    manager.raspberry = ws
    log.info("Raspberry Pi 연결됨")
    await manager.push_status()
    loop = asyncio.get_running_loop()
    try:
        while True:
            raw = await ws.receive_text()
            msg = json.loads(raw)
            if msg.get("type") not in ("frame", "tof"):
                continue
            msg["source"] = "raspberry"
            msg["ts"] = time.time()
            await manager.broadcast(msg)

            # ToF → 링버퍼
            if msg.get("type") == "tof":
                dist_mm = msg.get("distance", msg.get("dist", 9999))
                
                if dist_mm > 100: 
                    tof_ring.append(dist_mm)
                else:
                    # 아무것도 탐지 안 될 때 10mm가 들어온다면, 이를 '먼 거리'로 치환해서 넣거나 무시합니다.
                    tof_ring.append(2000) # 2미터 정도로 치환

            # ── Grounding DINO 추론 ──────────────────────
            elif msg.get("type") == "frame" and GDINO_AVAILABLE:
                global gdino_frame_count
                gdino_frame_count += 1
                if gdino_frame_count % GDINO_FRAME_SKIP == 0:
                    b64 = msg.get("data", "")
                    if b64:
                        jpeg_bytes = base64.b64decode(b64)
                        orig_w = msg.get("width",  1920)
                        orig_h = msg.get("height", 1080)

                        async def _detect(jb=jpeg_bytes, ow=orig_w, oh=orig_h):
                            dets = await loop.run_in_executor(
                                gdino_executor, _run_gdino, jb, ow, oh)
                            if dets:
                                await manager.broadcast({
                                    "source": "dino", "type": "detections",
                                    "detections": dets, "ts": time.time(),
                                })
                                await _track_object(dets, ow)

                        asyncio.create_task(_detect())



    except WebSocketDisconnect:
        log.info("Raspberry Pi 연결 해제")
    finally:
        manager.raspberry = None
        await manager.push_status()

# ─── 음성 명령 파서 ───────────────────────────────────
# ── 음성 명령: 방향만 정의 (속도는 ToF가 결정) ──────────
VOICE_COMMANDS = [
    (["전진", "앞으로", "앞으로가", "직진", "전방", "앞", "go", "forward"], "forward"),
    (["후진", "뒤로", "뒤로가", "후방", "백", "back", "backward"],          "backward"),
    (["좌회전", "왼쪽으로", "왼쪽", "왼으로", "좌측", "왼", "left"],        "left"),
    (["우회전", "오른쪽으로", "오른쪽", "오른으로", "우측", "오른", "right"],"right"),
    (["정지", "멈춰", "멈춤", "서", "스톱", "중지", "그만", "stop", "halt"], "stop"),
]

# ── ToF 거리 → ESC 변환 ──────────────────────────────────
# ESC_MIN(1400) = 전진 최대 / ESC_MID(1540) = 중립 / ESC_MAX(1800) = 후진 최대
DIST_STOP  = 150    # mm — 15cm 이하: 정지
DIST_SLOW  = 400    # mm — 40cm 이하: 저속 (최대 30% 출력)
DIST_FULL  = 1200   # mm — 120cm 이상: 최대 출력 (80%)

# ── 방향별 고정 PWM 값 ────────────────────────────────────
# ESC: 작을수록 전진 / 클수록 후진
# SERVO: 950(좌 최대) ~ 1325(중앙) ~ 1700(우 최대)
CMD_TABLE = {
    "forward":  {"esc": 1440, "servo": SERVO_MID},
    "backward": {"esc": 1700, "servo": SERVO_MID},
    "left":     {"esc": 1440, "servo": 1100},
    "right":    {"esc": 1440, "servo": 1900},
    "stop":     {"esc": ESC_MID, "servo": SERVO_MID},
}

def parse_voice_command(text: str) -> dict | None:
    """Whisper 전사 → 방향 추출 → 고정 PWM 값 반환"""
    if not text:
        return None
    normalized = text.replace(" ", "").lower()
    for keywords, direction in VOICE_COMMANDS:
        for kw in keywords:
            if kw in normalized or kw in text.lower():
                pwm = CMD_TABLE.get(direction, CMD_TABLE["stop"])
                payload = {
                    "type":      "control",
                    "direction": direction,
                    "esc":       pwm["esc"],
                    "servo":     pwm["servo"],
                }
                print(f"[CMD] '{text}' → '{kw}'  {direction}"
                      f"  ESC={pwm['esc']}μs  SERVO={pwm['servo']}μs")
                return payload
    print(f"[CMD] '{text}' → 매칭 없음")
    return None


# ─── Whisper 실행 헬퍼 ───────────────────────────────
async def _trigger_whisper(loop):
    global audio_buffer, last_whisper_ts
    if not WHISPER_AVAILABLE or not audio_buffer:
        return
    last_whisper_ts = time.time()
    combined = np.concatenate(audio_buffer).copy()
    audio_buffer.clear()
    dur    = len(combined) / SAMPLE_RATE
    rms_p  = float(np.sqrt(np.mean(combined.astype(np.float32)**2))) / 32768.0
    db_p   = 20 * np.log10(rms_p + 1e-10)
    print(f"{'='*55}\n[Whisper] ▶ 실행  {dur:.1f}s / {len(combined)} samples  {db_p:+.1f}dB\n{'='*55}")

    async def _transcribe(s=combined):
        t0    = time.time()
        text  = await loop.run_in_executor(executor, _run_whisper, s)
        elapsed = time.time() - t0
        if text:
            print(f"[Whisper] ✅ {elapsed:.2f}s  →  {text}\n")
            await manager.broadcast({
                "source": "mic", "type": "transcript",
                "text": text, "ts": time.time(),
            })
            cmd = parse_voice_command(text)
            if cmd:
                await set_current_cmd(cmd)          # ← drive_loop에 반영
                await manager.broadcast({
                    "source": "mic", "type": "voice_cmd",
                    "cmd": cmd, "text": text, "ts": time.time(),
                })
        else:
            print(f"[Whisper] ⚠️  결과 없음 ({elapsed:.2f}s)\n")

    asyncio.create_task(_transcribe())

# ─── /ws/mic ─────────────────────────────────────────
@app.websocket("/ws/mic")
async def ws_mic(ws: WebSocket):
    global audio_buffer, last_whisper_ts
    if manager.mic is not None:
        await ws.accept()
        await ws.send_text(json.dumps({"type": "error", "msg": "마이크 이미 연결됨"}))
        await ws.close(code=1008)
        return
    await ws.accept()
    manager.mic = ws
    log.info("마이크 연결됨")
    await manager.push_status()
    loop = asyncio.get_running_loop()
    total_chunks = 0
    try:
        while True:
            try:
                frame = await asyncio.wait_for(ws.receive(), timeout=5.0)
            except asyncio.TimeoutError:
                continue
            now = time.time()
            if "bytes" in frame and frame["bytes"]:
                raw_bytes = frame["bytes"]
                audio_buffer.append(np.frombuffer(raw_bytes, dtype=np.int16).copy())
                total_chunks += 1
                while sum(len(s) for s in audio_buffer) > MAX_BUFFER_SAMP and audio_buffer:
                    audio_buffer.pop(0)
                buf_sec = sum(len(s) for s in audio_buffer) / SAMPLE_RATE
                _spk_buf.append(raw_bytes)
                await manager.broadcast({"source": "mic", "type": "audio_meta",
                                          "size": len(raw_bytes), "ts": now})
                await manager.broadcast_binary(raw_bytes)
                if total_chunks % 10 == 0:
                    print(f"[MIC] #{total_chunks:05d}  buf={buf_sec:.1f}s  "
                          f"{'준비됨' if WHISPER_AVAILABLE else '❌미설치'}  "
                          f"{'🔊' if SPEAKER_ENABLED else ''}")
                if WHISPER_AVAILABLE and sum(len(s) for s in audio_buffer) >= MAX_BUFFER_SAMP:
                    await _trigger_whisper(loop)
            elif "text" in frame and frame["text"]:
                msg = json.loads(frame["text"])
                if msg.get("type") == "speech_end":
                    print(f"\n[VAD] speech_end → Whisper 실행")
                    await _trigger_whisper(loop)
                else:
                    msg["source"] = "mic"
                    msg["ts"]     = now
                    await manager.broadcast(msg)
    except WebSocketDisconnect:
        log.info("마이크 연결 해제")
    except RuntimeError as e:
        if "disconnect" in str(e).lower():
            log.info("마이크 연결 해제 (RuntimeError)")
        else:
            log.error(f"ws_mic RuntimeError: {e}")
    finally:
        manager.mic = None
        await manager.push_status()

# ─── /ws/car ─────────────────────────────────────────
@app.websocket("/ws/car")
async def ws_car(ws: WebSocket):
    await ws.accept()
    manager.car = ws
    log.info("자동차 ESP32 연결됨")
    await manager.push_status()
    try:
        while True:
            raw = await ws.receive_text()
            msg = json.loads(raw)
            msg["source"] = "car"
            msg["ts"]     = time.time()
            await manager.broadcast(msg)
    except WebSocketDisconnect:
        log.info("자동차 연결 해제")
    finally:
        manager.car = None
        await manager.push_status()

# ─── /ws/dashboard ───────────────────────────────────
@app.websocket("/ws/dashboard")
async def ws_dashboard(ws: WebSocket):
    await ws.accept()
    manager.dashboards.append(ws)
    log.info(f"대시보드 연결 ({len(manager.dashboards)}개)")
    await manager.push_status()
    try:
        while True:
            raw = await ws.receive_text()
            msg = json.loads(raw)
            if msg.get("type") == "control":
                cmd = {
                    "type":      "control",
                    "direction": msg.get("direction", "manual"),
                    "throttle":  msg.get("throttle", 0.0),
                    "steering":  msg.get("steering",  0.0),
                    "esc":       msg.get("esc"),
                    "servo":     msg.get("servo"),
                }
                await set_current_cmd(cmd)          # ← drive_loop에 반영
            elif msg.get("type") == "track":
                # {"type":"track","target":"person"} or {"type":"track","target":null}
                global track_target
                track_target = msg.get("target") or None
                label = track_target or "OFF"
                print(f"[TRACK] 타겟 설정: {label}")
                await ws.send_text(json.dumps({"type":"track_ack","target":label}))
            elif msg.get("type") == "ping":
                await ws.send_text(json.dumps({"type": "pong", "ts": time.time()}))
    except WebSocketDisconnect:
        log.info("대시보드 연결 해제")
    finally:
        if ws in manager.dashboards:
            manager.dashboards.remove(ws)
        await manager.push_status()

# ─── 정적 서빙 ────────────────────────────────────────
@app.get("/")
async def serve_dashboard():
    with open("dashboard.html", "r", encoding="utf-8") as f:
        return HTMLResponse(content=f.read())

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("server:app", host="0.0.0.0", port=8765, reload=False)