# 방 코드 · QR 생성 (순수 C 모듈)

구현 위치는 **`cpp/room/c/`**입니다. 서버용 코드이므로 iOS의
`GomManna/Services/CRules/`에 넣지 않습니다. 이 브랜치의 실제 서버 폴더는
`cpp/`이며, 루트 README에 적힌 `server/` 경로와 다릅니다.

## 구현 범위

1. 운영체제의 안전한 난수로 **8자리 방 초대 코드 후보**를 생성합니다.
2. **DB에 저장된 코드**로 초대 링크와 스캔 가능한 **QR SVG 문자열**을 생성합니다.

실행 코드와 예제는 모두 `.c` / `.h`입니다. C++ 구현, HTTP 엔드포인트,
DB 저장, 방 참여/탈퇴/종료는 추가하지 않았습니다. QR 인코더도 MIT 라이선스의
C 소스를 고정 버전으로 포함하므로 외부 QR API 호출 없이 동작합니다.

```text
cpp/room/
├── README.md                          # 서버 연결 안내 (이 문서)
└── c/
    ├── CMakeLists.txt                 # 독립 C 빌드, gm_room_c 라이브러리
    ├── include/gommanna/room_invite.h  # C/C++ 공용 공개 함수
    ├── src/
    │   ├── room_code.c                # 코드 생성·검증, 초대 링크 생성
    │   ├── room_qr.c                  # QR 인코딩 및 SVG 출력
    │   ├── secure_random.c            # Windows/Linux/macOS 난수
    │   └── secure_random.h            # 내부 헤더
    ├── third_party/qrcodegen/         # MIT C QR 인코더와 출처·해시
    ├── examples/room_invite_example.c # CLI 실행 예제
    └── tests/                        # C 테스트, 선택적 독립 QR 디코딩 검증
```

## 기존 서버 규격

`cpp/include/gommanna/contracts.hpp`의 `Invite` 필드에 대응합니다.

| 서버 필드 | C 모듈 결과 |
|---|---|
| `invite_code` | `gm_room_code_generate()`의 8자리 코드 |
| `qr_payload` | `gm_room_invite_payload()`의 `gommanna://join?code=ABCD2345` |
| `qr_svg` | `gm_room_qr_svg()`의 완성된 SVG 문자열 |

문자 집합은 `ABCDEFGHJKLMNPQRSTUVWXYZ23456789`입니다. DB migration의 8자리
제약조건에 맞추며, 혼동하기 쉬운 `0`, `1`, `I`, `O`는 제외합니다. 현재 iOS의
6자리 안내/로컬 QR 샘플과는 다르므로, 앱 연결 시 서버의 8자리 코드 및 딥링크
규격에 맞춰야 합니다. 이 모듈이 앱의 카메라 인식이나 URL 처리를 구현하지는 않습니다.

## C에서 호출

```c
#include "gommanna/room_invite.h"

char code[GM_ROOM_CODE_CAPACITY];
char payload[GM_ROOM_PAYLOAD_CAPACITY];
char svg[GM_ROOM_QR_SVG_CAPACITY];
size_t svg_bytes;

if (gm_room_code_generate(code, sizeof(code)) != GM_ROOM_OK) {
    return; /* 이 예제는 void 함수 안에서 호출하는 경우. 요청 처리를 중단. */
}
/* 여기서 서버가 code를 DB에 저장. 충돌이면 새 후보를 만들어 재시도. */
if (gm_room_invite_payload(code, payload, sizeof(payload)) == GM_ROOM_OK &&
    gm_room_qr_svg(code, svg, sizeof(svg), &svg_bytes) == GM_ROOM_OK) {
    /* payload와 svg를 응답에 사용. SVG 길이는 svg_bytes - 1. */
}
```

각 함수의 반환값을 확인해야 합니다. 버퍼는 호출자가 소유하며 `free()`할
반환 포인터는 없습니다. 작은 버퍼와 잘못된 코드를 오류로 반환합니다.
정확한 크기만 할당하려면 `gm_room_qr_svg(code, NULL, 0, &svg_bytes)`로 조회합니다.
모든 문자열은 NUL로 끝나야 하고 입력·출력 버퍼는 겹치면 안 됩니다.

## 단독 빌드 및 실행

저장소 루트에서 CMake와 C 컴파일러가 설치된 터미널로 실행합니다.
Windows는 Visual Studio의 **Developer PowerShell / Native Tools Command Prompt**를
사용할 수 있습니다. Linux/macOS에서도 동일한 CMake 명령을 사용합니다.

```sh
cmake -S cpp/room/c -B cpp/room/c/build -DBUILD_TESTING=ON
cmake --build cpp/room/c/build --config Debug
ctest --test-dir cpp/room/c/build -C Debug --output-on-failure
```

단일 구성 빌드(Ninja/Makefiles)의 예제:

```sh
cpp/room/c/build/room_invite_example ABCD2345 > invite.svg
```

Visual Studio 다중 구성 빌드에서는:

```powershell
.\cpp\room\c\build\Debug\room_invite_example.exe ABCD2345 > invite.svg
```

코드를 생략하면 새로운 후보를 생성합니다. SVG는 표준 출력으로, 코드/링크는
표준 오류로 출력하므로 `invite.svg`에 안내 문구가 섞이지 않습니다.
예제는 DB에 방을 생성하지 않습니다.

QR 독립 디코딩 테스트는 Python에 `numpy`, `zxing-cpp`가 이미 설치되어 있을 때
선택적으로 실행합니다. 서버/라이브러리 실행에는 Python이 필요하지 않습니다.

```sh
python cpp/room/c/tests/verify_qr_decode.py cpp/room/c/build/room_invite_example
```

## 나중에 C++ 서버에 연결할 위치

현재 `cpp/http/server.cpp`는 `IRoomService`를 호출하고, 최상위 CMake는
`gm_room`이라는 **C++ 서비스 구현**을 요구합니다. 이번 라이브러리 이름은
**`gm_room_c`**이며 두 대상은 다릅니다. 따라서 이번 변경만으로 실제 DB 모드
서버가 완성되거나 `/v1/rooms`에 자동 연결되지는 않습니다.

서버 통합 담당자가 연결할 때:

1. **`cpp/room/room_service.cpp`**에 기존 `IRoomService`를 구현하고
   `make_room_service(IRepository&)`를 정의합니다. 생성/참여/탈퇴/종료의 DB 호출은
   기존 `IRepository`를 사용합니다. 공개 헤더의 `extern "C"` 처리 덕분에
   `#include "gommanna/room_invite.h"` 후 C 함수를 그대로 호출할 수 있습니다.
2. **`cpp/CMakeLists.txt`**의 프로젝트 선언을 `project(GomManna LANGUAGES C CXX)`로
   바꿔 C 컴파일러도 활성화합니다. **`cpp/room/CMakeLists.txt`**는 다음 구조로 추가합니다. `room_service.cpp`가
   아직 없으므로 이 상위 CMake 파일은 이번 작업에서 만들지 않습니다.

   ```cmake
   add_subdirectory(c)  # gm_room_c 생성
   add_library(gm_room STATIC room_service.cpp)
   target_link_libraries(gm_room PUBLIC gm_contract PRIVATE gm_room_c)
   ```

3. 기존 `cpp/CMakeLists.txt`는 위 파일이 생기면 `add_subdirectory(room)` 후
   `gommanna_server`에 `gm_room`을 연결하도록 이미 작성되어 있습니다.
4. **방 생성 흐름:** 코드 후보 생성 → `IRepository::create_room(..., code, ...)`
   → 성공한 코드만 반환합니다. `invite_code_conflict`일 때만 새 코드를 생성해
   제한 횟수(예: 5회) 재시도하고, DB 오류 등 다른 오류는 즉시 전달합니다.
   사전 조회만으로 중복을 판단하지 말고 DB의 UNIQUE 제약을 최종 기준으로 삼습니다.
5. **초대 조회 흐름:** `IRepository::get_room(actor_id, room_id)`로 멤버 권한 및
   활성 방 상태를 확인한 뒤, 반환된 **저장 코드**로 payload/SVG를 만듭니다.
   조회할 때마다 코드를 다시 생성하면 기존 초대 코드와 QR이 달라지므로 안 됩니다.
6. `Invite{invite_code, qr_payload, qr_svg}`를 반환하면 기존
   **`GET /v1/rooms/{room_id}/invite`**가 세 필드를 JSON으로 응답합니다.
   SVG만 제공하는 별도 응답이 필요하면 MIME 타입은 `image/svg+xml`입니다.

## 배포 때 넣을 것

- 서버 저장소의 **`cpp/room/` 폴더 전체**를 유지한 채 빌드합니다.
- 헤더뿐 아니라 `src/`와 `third_party/qrcodegen/`의 소스·라이선스도 필요합니다.
- 서버 연결 후 생성된 실행 파일(또는 서버 컨테이너)을 배포합니다. 정적 라이브러리로
  연결하므로 `.c` 파일을 실행 서버에 별도로 복사해서 실행하는 방식이 아닙니다.
- QR은 요청 시 SVG 문자열로 만들므로 QR 파일 저장 폴더나 외부 QR 서버가 필요 없습니다.
- 테스트/예제/빌드 산출물은 운영 서버 배포에 필요하지 않습니다.

## 검증 범위

C 테스트는 코드 형식·잘못된 입력·작은 버퍼·정확한 버퍼 경계·난수 실패를 검사합니다.
선택적 Python 테스트는 C 프로그램이 출력한 SVG를 독립 스캐너(ZXing)로 읽어
초대 링크를 비교합니다. DB 충돌 재시도, HTTP 권한 검사, 앱의 QR 인식은 이후
C++ 서버/앱 통합 단계에서 별도로 검증해야 합니다.

작성 시 Windows/MSVC 19.51에서 C 빌드와 CTest 2개, ZXing 3.1.1을 통한 QR 20개
디코딩, C++20 호출 프로그램의 링크·실행을 확인했습니다. Linux/macOS 난수 분기는
구현되어 있지만 이 Windows 환경에서는 실행 검증하지 않았습니다.
