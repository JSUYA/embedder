# PR #155 MessageLoop 전환 검증 체크리스트

## 0) 사전 조건
- Debug 빌드 + 로그 활성화
- 가능하면 Sanitizer 빌드:
  - ASAN
  - TSAN
- 대상 변경 파일:
  - `flutter/shell/platform/tizen/tizen_vsync_waiter.h`
  - `flutter/shell/platform/tizen/tizen_vsync_waiter.cc`

## 1) 기능 정상성

### TC-01 기본 렌더링/애니메이션
- 앱 실행 후 애니메이션 2~3분 유지
- 기대: 프레임 멈춤/드랍 없음, vsync 에러 로그 없음

### TC-02 백그라운드/포어그라운드 전환
- 홈 이동/복귀 30회 반복
- 기대: 복귀 후 렌더 정상, 오류 로그 폭주 없음

## 2) 종료 레이스 / Task drain

### TC-03 빠른 시작-종료 반복 (핵심)
- 앱 실행 후 0.5~1초 내 종료를 100회 반복
- 기대: crash/deadlock/hang 없음

### TC-04 종료 직전 vsync 요청 몰림
- 고빈도 invalidate 상태에서 즉시 종료
- 기대: 종료 정상, 남은 task 처리 정책이 일관됨

## 3) 동시성 안정성

### TC-05 TSAN
- TC-01~04를 TSAN 환경에서 반복
- 기대: data race 0건
- 중점: `quit_`, `engine_`, `baton_` 접근 경로

### TC-06 ASAN
- start/stop 반복 + 애니메이션
- 기대: UAF/heap 오류 0건

## 4) 성능/지연 영향

### TC-07 Vsync 지연 관찰
- 5분간 frame interval 관찰
- 기대: 기존 대비 회귀 없음, 큐 backlog 증가 추세 없음

### TC-08 CPU 사용량 비교
- 기존 브랜치 vs PR 브랜치 비교
- 기대: 유의미한 CPU 악화 없음

## 5) 실패 경로 검증

### TC-09 tdm invalid 경로
- engine stop 직후 vsync 요청 유도
- 기대: 안전한 early return, crash 없음

### TC-10 PostTask after quit
- MessageLoop 종료 후 PostTask 호출
- 기대: 명시적 실패 처리(반환값/로그)

## Merge 전 권장 Acceptance
- [ ] TC-03/05/06 통과 (필수)
- [ ] 종료 hang/crash 0건
- [ ] race/UAF 0건
- [ ] 성능 회귀 없음
- [ ] 종료 정책(task drain/cancel) 코드/로그로 명확
