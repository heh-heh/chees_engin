# chees_engin

C++ 공부를 목적으로 만드는 체스 엔진 프로젝트입니다.

## 현재 구현

- 8 x 8 체스판 초기 상태 출력
- 체스 기물별 기본 이동 형태 검사
	- 폰
	- 나이트
	- 비숍
	- 룩
	- 퀸
	- 킹
- 이동 가능한 좌표인지 체스판 범위 검사
- 기물의 현재 위치와 이동 횟수 관리
- 룩, 비숍, 퀸 등의 이동 경로를 연결 리스트에 저장
- `std::setw`를 사용한 보드 출력 정렬

## 디렉터리 구조

```text
src/
├── main.cpp       # 프로그램 시작점
├── game.cpp       # 게임 진행 관리자 뼈대
├── bord.cpp       # 체스판과 보드 출력
├── pieces.cpp     # 기물별 이동 규칙
├── vector.cpp     # 좌표 자료형
├── linked_list.cpp # 이동 경로 저장용 연결 리스트
└── Director.cpp   # 향후 게임 진행 제어용 클래스
```

## 자료형

좌표는 사용자가 만든 `vector` 타입으로 표현합니다.

```cpp
vector position{3, 4};
```

`x`와 `y`의 범위는 각각 `0`부터 `7`까지입니다.

기물은 다음 값으로 구분합니다.

```text
color: -1 흰색, 1 검은색
type : 1 폰, 2 나이트, 3 비숍, 4 룩, 5 퀸, 6 킹
```

## 사용 예시

```cpp
pieces rook(-1, 4, vector{0, 0});

if (rook.move(0, 5)) {
		rook.path().print();
}
```

이동이 성공하면 기물의 현재 위치가 갱신되고, 이동 중간의 좌표는 `path()`로 확인할 수 있습니다.

## 빌드

현재 `main.cpp`는 테스트 출력만 포함하고 있습니다. 프로젝트 루트에서 다음 명령으로 빌드할 수 있습니다.

```bash
g++ -std=c++17 -Wall -Wextra -pedantic src/main.cpp -o chess
./chess
```

개별 소스 파일의 문법만 확인하려면 다음처럼 컴파일합니다.

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -c src/game.cpp -o /tmp/game.o
g++ -std=c++17 -Wall -Wextra -pedantic -c src/bord.cpp -o /tmp/bord.o
```

## 아직 구현할 기능

- 보드 위 기물 배치 및 이동 처리
- 다른 기물에 의한 이동 경로 차단
- 기물 잡기
- 폰의 대각선 잡기와 프로모션
- 캐슬링 및 앙파상
- 체크, 체크메이트, 스테일메이트 판정
- 게임 입력과 턴 관리
- 기물 클래스와 보드 클래스의 헤더 파일 분리
