# Git 브랜치 & 커밋 전략

STM32 임베디드 실습 프로젝트 기준
회로도 
---

## 브랜치 전략

```
main
├── dev
│   ├── feature/uart-isr
│   ├── feature/timer-interrupt
│   └── feature/pwm-motor
```

### 브랜치 종류

| 브랜치 | 용도 | 예시 |
|--------|------|------|
| `main` | 완성되고 동작 확인된 코드만 | 발표/제출용 |
| `dev` | 개발 통합 브랜치 | 기능 합치는 곳 |
| `feature/xxx` | 기능 하나씩 개발 | 실험적 작업 |

### 흐름

```
feature/xxx → dev → main
```

```bash
# 새 기능 시작
git checkout dev
git checkout -b feature/uart-isr

# 개발 완료 후 dev에 합치기
git checkout dev
git merge feature/uart-isr

# 완전히 완성되면 main에 합치기
git checkout main
```

---

## 커밋 메시지 전략

### 형식

```
<타입>: <내용> [파일명]
```

### 타입 종류

| 타입 | 의미 | 예시 |
|------|------|------|
| `feat` | 새 기능 추가 | `feat: UART1 수신 인터럽트 추가` |
| `fix` | 버그 수정 | `fix: EXTI Pending 클리어 순서 수정` |
| `refactor` | 코드 정리 (동작 변화 없음) | `refactor: Key_ISR_Enable 주석 정리` |
| `docs` | 문서/주석 작업 | `docs: timer.c 전체 주석 추가` |
| `test` | 테스트 코드 | `test: UART 루프백 테스트 코드` |
| `chore` | 빌드/설정 변경 | `chore: Makefile 최적화 옵션 추가` |

### 예시

```bash
git commit -m "feat: 버튼 EXTI 인터럽트 구현 [key.c]"
git commit -m "fix: UART1 TXE 무한대기 버그 수정 [uart.c]"
git commit -m "feat: TIM4 타이머 인터럽트 추가 [timer.c]"
git commit -m "refactor: Main 루프 버튼/UART 이벤트 분리 [main.c]"
git commit -m "docs: timer.c 동작 원리 주석 추가"
```

---

## 기본 명령어

```bash
# 현재 상태 확인
git status
git log --oneline

# 브랜치 확인/이동
git branch
git checkout dev

# 스테이징 & 커밋
git add uart.c
git add .               # 전체
git commit -m "feat: ..."

# 원격 올리기
git push origin feature/uart-isr
```

---

## 실습 프로젝트 브랜치 예시

```
main
└── dev
    ├── feature/led-gpio
    ├── feature/uart2-printf
    ├── feature/button-exti
    ├── feature/uart1-isr
    ├── feature/timer-interrupt
    └── feature/pwm-motor         ← 다음 목표
```
