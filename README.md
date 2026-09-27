# Chees Engine

C++ 학습과 체스 엔진 설계 실험을 목표로 시작한 프로젝트입니다. 기본적으로는 터미널 기반의 체스 보드와 기물 이동 로직을 구현해보고, 이후 GUI 기반 시각화와 네트워크 멀티플레이어 구조까지 확장하는 방향으로 진행해 왔습니다.

## 프로젝트 개요

이 프로젝트는 다음 단계로 발전해 왔습니다.

1. 터미널 기반 체스 엔진 기본 구조 구현
2. 기물 이동, 턴 처리, 보드 표시 흐름 구성
3. ImGui + GLFW 기반 GUI 보드 프로토타입 추가
4. 표준 체스 보드 방향 정리
5. Windows용 실행 파일 패키징
6. 호스트-클라이언트 방식 LAN 네트워크 체스 구현
7. GUI를 Qt Widgets 기반으로 전면 재구성 (imgui/glfw 제거)

현재 코드 구조는 체스 엔진의 핵심 규칙을 별도 계층으로 분리해 두고, 보드 상태와 기물 상태를 함께 관리할 수 있도록 설계되어 있습니다. 아직 완전한 체스 규칙 엔진으로는 완성되지 않았지만, 기물 이동과 턴 흐름, 보드 표현, 기본 GUI 흐름까지는 프로젝트의 핵심 구성을 확인할 수 있는 수준까지 정리되었습니다.

## 현재 구현 상태

### 1. 콘솔 기반 체스 엔진

- 8x8 보드 상태를 관리하는 Board 구조 사용
- 기물 객체를 색상과 종류로 관리
- 기물의 이동 가능 여부와 경로 차단 여부를 판정
- 선택 기물과 이동 후보, 불가 후보를 구분해 출력
- 잡힌 기물 처리와 턴 전환 로직을 포함
- 표준 체스 시각 기준에 맞춰 흰색이 아래쪽, 검은색이 위쪽에 위치하도록 정리

### 2. GUI (Qt Widgets)

- Qt6 Widgets 기반으로 시작 화면, 로비, 모드 선택, 대기실, 대국 화면을 구성
- 체스판은 `QWidget` 커스텀 페인팅으로 구현하고 유니코드 체스 기물 글리프로 표시
- 보드 선택 및 이동 후보 하이라이트 구현
- 기본 턴 관리와 Reset/New Game 동작 구현
- 멀티플레이어 대기실, 채팅, 체크메이트 다이얼로그 등 기존 기능을 동일하게 유지
- 네트워크 통신은 `QTcpServer`/`QTcpSocket` 기반으로 처리 (수동 스레드/뮤텍스 제거)

### 3. Windows 빌드 패키징

- Linux 네이티브 빌드(`build/`)와 mingw-w64 크로스 컴파일 Windows 빌드(`build-win/`)를 모두 생성 가능
- `cmake/toolchain-mingw-w64.cmake` 툴체인 파일로 `x86_64-w64-mingw32-g++`(posix 스레드) 크로스 컴파일러를 사용하고, Qt는 aqtinstall로 받은 `win64_mingw` 빌드를 링크 대상으로, 버전이 일치하는 `linux_gcc_64` 빌드를 moc/uic/rcc 호스트 툴로 사용
- `scripts/package_windows.sh`가 크로스 빌드 후 `chess_gui.exe`, `chess_console.exe`와 필요한 `Qt6*.dll`, `platforms/qwindows.dll`, `libwinpthread-1.dll`을 `chess_windows_release/`에 모아 zip으로 묶는다

### 4. LAN 멀티플레이어

- 호스트는 TCP 포트를 모든 네트워크 인터페이스에 열고, 흰색 기물을 조작한다.
- 클라이언트는 호스트의 LAN IPv4 주소 또는 호스트 이름과 포트를 입력해 접속하고, 검은색 기물을 조작한다.
- TCP 이동, 채팅, 준비, 시작, 상태 메시지는 줄 단위로 프레이밍되어 여러 메시지가 한 번에 도착하거나 분할 도착해도 처리할 수 있다.
- 양쪽 플레이어가 준비를 완료하면 호스트가 게임을 시작하며, 호스트 기준 턴 시간과 점수가 클라이언트에도 표시된다.

### 5. 릴레이 서버 (`relay_server`)

- 체스 로직이 전혀 없는 순수 TCP 중계 전용 프로그램. `chess_gui`의 호스트/클라이언트가 각각 릴레이 서버로 outbound 연결만 하면 되므로, 호스트 쪽 포트 포워딩/방화벽 설정 없이도 인터넷을 통한 매칭이 가능하다.
- 프로토콜: 접속 직후 한 줄짜리 핸드셰이크 `HELLO|HOST|<room_code>` 또는 `HELLO|CLIENT|<room_code>`를 보낸다. 같은 room code로 HOST/CLIENT가 모두 접속하면 서버가 양쪽에 `PAIRED`를 보내고, 이후부터는 두 소켓 사이의 바이트를 그대로 전달만 한다(기존 MOVE/CHAT/READY/START/STATE 메시지 형식이 그대로 통과).
- room code가 이미 사용 중이면 `ERROR|room code already in use`, 존재하지 않는 room에 join하면 `ERROR|room not found`를 보내고 연결을 끊는다.
- 실행: `./relay_server --port 9100` (기본 포트 `9100`). 공인 IP를 가진 서버(VPS 등)에서 실행하면 릴레이 TCP는 `9100`, 브라우저 상태 페이지는 자동으로 `9101`에서 열린다.
- 상태 페이지: 브라우저에서 `http://<서버 주소>:9101`로 접속하면 현재 TCP 접속자 수, 대기 중인 방, 대국 중인 방을 5초마다 확인할 수 있다.

## 다른 PC에서 접속하기

같은 로컬 네트워크에 연결된 두 PC에서 다음 순서로 실행합니다.

1. 호스트 PC에서 GUI의 `Multiplayer` 화면으로 이동한 뒤 `Host Match`를 누릅니다.
2. 대기 화면에 표시되는 `LAN address`와 `Port`(기본값 `9001`)를 확인합니다.
3. 클라이언트 PC에서 같은 포트를 입력하고, `Host`에 호스트의 `LAN address`를 넣은 뒤 `Join Match`를 누릅니다.
4. 양쪽 화면에 연결 완료가 표시되면 각각 `Ready`를 선택합니다. 호스트가 `Start Match`를 누르면 양쪽 게임이 시작됩니다. 호스트는 백, 클라이언트는 흑입니다.
5. 대기실과 게임 화면의 `MATCH CHAT`에서 메시지를 보내 상대방과 대화할 수 있습니다.

호스트 PC의 방화벽은 해당 포트의 TCP 인바운드 연결을 허용해야 합니다. 서로 다른 인터넷망에서 접속하려면 호스트 공유기의 TCP 포트 포워딩도 필요합니다. 현재 연결에는 인증이나 암호화가 없으므로 신뢰할 수 있는 네트워크에서만 사용해야 합니다.

## 릴레이 서버로 인터넷을 통해 접속하기

포트 포워딩 없이 서로 다른 네트워크에 있는 두 사람이 접속하려면 공인 IP를 가진 서버에서 `relay_server`를 띄워 중계하면 됩니다.

1. 공인 IP가 있는 서버(클라우드 VPS 등)에서 릴레이 서버를 실행합니다.
   ```bash
   ./relay_server --port 9100
   ```
   해당 서버의 방화벽에서 릴레이용 `9100` 포트와 상태 페이지용 `9101` 포트(TCP) 인바운드를 열어두면 됩니다. 상태 페이지를 외부에 공개하지 않으려면 `9101`은 방화벽에서 열지 않아도 됩니다.
2. 호스트 플레이어는 GUI의 `Mode Select` 화면에서 `Relay Server` 영역의 `Use relay server`를 체크하고, `Relay Address`에 릴레이 서버의 공인 IP/도메인, `Relay Port`에 `9100`, `Room Code`에 원하는 방 코드를 입력한 뒤 `Host Match`를 누릅니다.
3. 클라이언트 플레이어도 같은 방식으로 `Use relay server`를 체크하고 동일한 `Relay Address`/`Relay Port`/`Room Code`를 입력한 뒤 `Join Match`를 누릅니다.
4. 릴레이 서버가 같은 room code의 호스트/클라이언트를 짝지으면 이후 흐름(준비, 시작, 대국, 채팅)은 LAN 멀티플레이어와 동일합니다.

릴레이 서버는 두 소켓 사이의 바이트를 그대로 전달만 할 뿐 체스 로직이나 메시지 내용을 해석하지 않으며, 별도의 인증이나 암호화도 없습니다. 신뢰할 수 있는 서버에서만 운영하는 것을 권장합니다.

## 프로젝트 구조

```text
chees_engin/
├── CMakeLists.txt
├── README.md
├── cmake/
│   └── toolchain-mingw-w64.cmake
├── build/
├── build-win/
├── gui/
│   ├── main.cpp
│   ├── MainWindow.h / .cpp
│   ├── GameSession.h / .cpp
│   ├── NetworkSession.h / .cpp
│   ├── BoardWidget.h / .cpp
│   └── StartPage / LobbyPage / ModeSelectPage / WaitingRoomPage / GamePage
├── scripts/
├── src/
│   ├── main.cpp
│   ├── include_zip.cpp
│   ├── datas/
│   │   ├── linked_list.cpp
│   │   └── vector.cpp
│   ├── game/
│   │   ├── bord.cpp
│   │   ├── game.cpp
│   │   ├── render.cpp
│   │   └── update.cpp
│   ├── manager/
│   │   └── Director.cpp
│   └── object/
│       └── pieces.cpp
└── chess_windows_release/
```

## Windows용 빌드 (mingw-w64 크로스 컴파일)

Linux에서 Windows 실행 파일을 생성하려면 mingw-w64 툴체인과 Windows/Linux용 Qt6가 모두 필요합니다.

1. mingw-w64 크로스 컴파일러 설치, posix 스레드 모델로 전환 (Qt가 요구하는 std::thread 지원을 위해 필요):
   ```bash
   sudo apt-get install -y g++-mingw-w64-x86-64
   sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
   sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
   ```
2. aqtinstall로 Windows(mingw) Qt와, moc/uic/rcc 실행에 쓸 버전이 동일한 Linux Qt를 받는다:
   ```bash
   pip install aqtinstall
   python3 -m aqt install-qt windows desktop 6.8.3 win64_mingw -O /opt/qt
   python3 -m aqt install-qt linux desktop 6.8.3 linux_gcc_64 -O /opt/qt
   ```
3. 빌드 및 배포 패키징:
   ```bash
   ./scripts/package_windows.sh
   ```
   내부적으로 `cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake`로 구성한 뒤 빌드하고, 결과물을 `chess_windows_release/`와 `chess_windows_release.zip`으로 묶는다.

Qt 설치 경로가 다르면 `QT_MINGW_PREFIX`, `QT_HOST_PATH` 환경변수(또는 툴체인 파일의 기본값)를 맞게 바꿔야 한다. host/target Qt 버전이 다르면 CMake가 moc/uic 패키지를 target(Windows) 경로로 잘못 해석해 실행 불가능한 `.exe` 도구를 호출하려 하므로, 두 Qt는 반드시 동일 버전이어야 한다.

## 핵심 개발 방향

### 현재까지 정리된 핵심 요점

- 보드 상태와 기물 상태를 분리하여 관리한다.
- 기물 이동 검증은 개별 객체가 담당한다.
- 턴 관리와 선택/이동 흐름은 Director 또는 game 루프로 제어한다.
- GUI는 보드 표현을 위해 별도의 화면 로직을 가지되, 엔진 규칙을 기반으로 동작하도록 유지한다.
- 표준 체스 방향을 기준으로 보드 초기화 순서를 정리했다.

### 앞으로의 확장 계획

#### 1. 체스 규칙 완성

- 체크, 체크메이트, 스테일메이트 판정
- 캐슬링
- 앙파상
- 프로모션
- 좌표와 상태 검증을 더 엄격하게 보완

#### 2. 엔진-UI 정합성 강화

- 터미널 엔진과 GUI 보드가 같은 상태를 공유하도록 정리
- 이동 수행 시 보드 배열, 기물 객체, UI 상태를 일관되게 동기화
- 각 기물의 움직임을 실제 게임 규칙에 맞게 다시 검증

#### 3. 네트워크 기반 호스트-클라이언트 체스

- 한 명이 호스트로 서버를 연다.
- 다른 플레이어가 해당 서버에 접속한다.
- 게임 보드 상태, 기물 이동, 턴 정보를 서로 공유한다.
- 서버는 보드 상태를 유지하고, 클라이언트는 이동 메시지를 전송받아 동기화한다.
- 이후에는 실시간 멀티플레이어 체스 구조로 확장할 계획이다.

이것이 지금 프로젝트의 다음 큰 단계이며, 가장 중요한 목표 중 하나입니다.

## 현재 프로젝트의 위치

지금까지의 프로젝트는 다음 조건을 만족하는 수준까지 도달했습니다.

- 기본 체스 엔진 구조를 유지하고 있다.
- GUI 기반 보드 프로토타입을 만들 수 있다.
- Linux/Windows 빌드를 생성할 수 있다.
- 보드 방향과 기물 초기배치 기준을 표준 체스 방향에 맞게 정리했다.
- 이후 멀티플레이어 체스 구조를 붙일 수 있는 기반이 마련되었다.

즉, 단순한 학습용 체스 프로젝트를 넘어서, 점진적으로 완성형 체스 도구로 성장할 수 있는 기반이 확보된 상태입니다.

## 향후 목표

- 내부 엔진 로직을 안정화하고
- GUI와 콘솔 흐름을 하나의 규칙 체계로 통합하고
- 최종적으로 호스트-클라이언트 기반 온라인 체스 기능을 구현할 계획입니다.

핵심 목표는 단순히 기물을 움직이는 체스를 넘어서, 두 사람이 실제로 연결해서 플레이할 수 있는 구조를 만드는 것입니다.
