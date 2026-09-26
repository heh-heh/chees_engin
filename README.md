# Chess Engine

이 프로젝트는 C++로 만든 학습용 체스 엔진입니다. 현재는 ImGui + GLFW 기반 GUI 플레이 화면, 간단한 콘솔 실행 진입점, LAN 멀티플레이어, 그리고 네트워크 프로토콜 테스트까지 포함합니다.

## 현재 상태

- GUI 체스 보드 실행 가능
- 로컬 2인 LAN 대전 지원
- 대기실 준비 상태 동기화 및 경기 시작 지원
- 경기 중 이동/상태/채팅 메시지 동기화 지원
- Windows 배포 ZIP 포함
- 네트워크 메시지 파싱 테스트 포함

아직 완전한 정식 체스 엔진은 아니며, 체크/체크메이트, 캐슬링, 앙파상, 프로모션 같은 규칙 보완이 남아 있습니다.

## 주요 구성

### GUI

- `gui/main.cpp`
- ImGui + GLFW + OpenGL 기반
- 싱글 플레이와 멀티플레이 화면 포함
- 보드 선택, 이동, 리셋, 채팅 UI 제공

### 콘솔 진입점

- `src/main.cpp`
- `chess_console` 타깃으로 빌드됨
- Linux/macOS 기준으로 안내 메시지를 출력한 뒤 빌드된 GUI 실행 파일을 호출하는 간단한 런처 역할

### 네트워크

- `src/network/chess_protocol.hpp`
- 보드 상태, 채팅, 준비 상태, 경기 시작, 경기 상태 메시지 처리
- 줄 단위 메시지 프레이밍 사용

### 테스트

- `tests/network_protocol_test.cpp`
- 프로토콜 생성/파싱 동작 검증

## 요구 사항

- CMake 3.16 이상
- C++17 컴파일러
- Linux/macOS에서는 OpenGL 개발 환경이 필요
- Windows에서 직접 빌드할 때는 OpenGL을 사용할 수 있는 C++ 빌드 환경이 필요
- 저장소 내 `third_party/imgui`, `third_party/glfw` 소스

## 빌드

작업은 저장소 루트에서 진행합니다.

```bash
cd <repository-root>
```

### GUI 빌드

```bash
cd <repository-root>
./scripts/build_gui.sh
```

직접 빌드:

```bash
cd <repository-root>
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### GUI 실행

아래 명령은 Linux/macOS 기준입니다.

```bash
cd <repository-root>
./scripts/run_gui.sh
```

또는:

```bash
cd <repository-root>
./build/chess_gui
```

### 콘솔 진입점 실행

아래 명령은 Linux/macOS 기준입니다.

`chess_console`는 내부적으로 GUI 실행 파일을 호출하므로, 먼저 GUI 빌드가 완료되어 있어야 합니다.

```bash
cd <repository-root>
./build/chess_console
```

## 테스트 실행

```bash
cd <repository-root>
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## LAN 멀티플레이 방법

현재 멀티플레이는 같은 로컬 네트워크(LAN) 환경 기준으로 정리되어 있습니다. 다른 인터넷망에서 접속하려면 별도 포트 포워딩과 방화벽 설정이 필요합니다.

1. 한쪽에서 `Multiplayer` → `Host Match`를 선택합니다.
2. 화면에 표시되는 `LAN address`와 `Port`를 확인합니다.
3. 다른 쪽에서 같은 포트와 호스트 주소를 입력한 뒤 `Join Match`를 선택합니다.
4. 양쪽이 연결되면 각각 `Ready`를 체크합니다.
5. 호스트가 `Start Match`를 누르면 경기가 시작됩니다.
6. 대기실과 경기 화면의 `MATCH CHAT`으로 메시지를 주고받을 수 있습니다.

## 주의 사항

- 현재 네트워크 연결에는 인증이나 암호화가 없습니다.
- 신뢰할 수 있는 로컬 네트워크에서만 사용하는 것을 권장합니다.
- 인터넷에 직접 노출하는 방식의 사용은 지원 대상으로 보지 않습니다.
- 호스트 PC는 해당 TCP 포트 인바운드 허용이 필요할 수 있습니다.
- Windows에서는 배포 ZIP이 제공되는 경우 바로 사용할 수 있고, 필요하면 CMake로 직접 빌드할 수도 있습니다.

## 프로젝트 구조

아래 구조는 주요 소스 디렉터리 중심의 개요입니다.

```text
repository-root/
├── CMakeLists.txt
├── README.md
├── gui/
│   └── main.cpp
├── scripts/
│   ├── build_gui.sh
│   └── run_gui.sh
├── src/
│   ├── main.cpp
│   ├── game/
│   ├── manager/
│   ├── network/
│   │   └── chess_protocol.hpp
│   └── object/
├── tests/
│   └── network_protocol_test.cpp
└── third_party/
```

### 배포 산출물 예시

- `chess_windows_release/`
- `chess_windows_release.zip`
- `chess_windows_release_with_dlls.zip`

## 앞으로 보완할 부분

- 정식 체스 규칙 완성
- 엔진 로직과 GUI 상태 동기화 강화
- 멀티플레이 안정성 개선
- 플랫폼별 빌드/배포 문서 보강
