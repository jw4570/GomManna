# 서버와 실제 DB 연결

서버는 C++ `IRepository`를 통해 Supabase PostgreSQL을 조회하고 결과를 JSON으로 반환한다. 별도 계정 `gommanna_login`은 필요한 서버 역할로 전환해 사용한다. 앱에 DB 접속 정보를 넣지 않는다.

## 실행 설정

프로젝트 루트의 `.env.example`을 참고해 각자의 컴퓨터에 `.env`를 만든다. `GOMMANNA_DATABASE_URL`은 Supabase Connect의 **Session pooler** 주소를 사용한다. 비밀번호와 인증서 경로에 특수문자가 있다면 URL에서 퍼센트 인코딩한다. DB 연결은 `verify-full`로 인증서를 검사한다. 비밀번호, 실제 `.env`, 토큰을 Git이나 PR에 올리지 않는다.

서버는 실행한 작업 폴더의 `.env`를 읽는다. 다른 위치에 파일이 있으면 실행 전에 `GOMMANNA_ENV_FILE` 환경 변수에 전체 경로를 지정한다. 별도로 지정한 환경 변수는 `.env` 값보다 우선한다. 파일은 한 줄에 `KEY=value` 형식이며 빈 줄과 `#`으로 시작하는 줄을 무시한다. 파일 내용을 서버 로그에 출력하지 않는다.

Windows에서 빌드할 때 PostgreSQL libpq 17 이상, OpenSSL 3, CMake와 C++20 컴파일러가 필요하다. 실행 시 해당 라이브러리의 `bin` 디렉터리를 PATH에서 찾을 수 있어야 한다. HTTP/JSON 의존성은 지정된 버전을 Git으로 가져온다.

```text
cmake -S cpp -B build/cpp -DBUILD_TESTING=ON -DPostgreSQL_ROOT=<PostgreSQL경로> -DOPENSSL_ROOT_DIR=<OpenSSL경로>
cmake --build build/cpp --config Release
```

기본 주소 `127.0.0.1:8080`은 같은 컴퓨터에서만 사용할 수 있다. 실제 아이폰에서 호출할 수 있는 네트워크 연결과 HTTPS 구성은 별도로 검증해야 한다.

## 방 모듈을 기다리는 동안

서영의 `cpp/room` 모듈이 없으면 기본 빌드는 중단된다. **DB와 HTTP 통합시험에 한해** 첫 명령에 `-DGOMMANNA_TEST_WITHOUT_ROOM_SERVICE=ON`을 추가할 수 있다. 이 시험 설정에서는 실제 Supabase에 연결하지만, 방 생성·참가·초대·나가기·종료 API는 모두 `501 NOT_IMPLEMENTED`다. 기존 방의 조회·위치·메모 API는 사용할 수 있다. 방 모듈이 병합되면 실제 `gm_room`을 사용하며 이 설정 없이 다시 빌드한다.

## 앱에서 사용할 주요 API

성공은 `{"ok":true,"data":...}`, 실패는 `{"ok":false,"error":{"code":"...","message":"..."}}`다. `POST /v1/guests`를 제외한 아래 API는 `Authorization: Bearer <token>`이 필요하다. 시각은 UTC Unix 밀리초, 좌표는 위도·경도다.

| 메서드·경로 | 용도 |
|---|---|
| POST `/v1/guests` | 익명 사용자와 토큰 발급 |
| GET `/v1/rooms/{room_id}` | 참가 중인 방 조회 |
| GET `/v1/rooms/{room_id}/members` | 참가자의 공유 상태·위치·메모 조회 |
| PUT `/v1/rooms/{room_id}/sharing` | 내 위치 공유 ON/OFF |
| PUT `/v1/rooms/{room_id}/location` | 내 좌표·시각·공유 epoch·순서 저장 |
| PUT / DELETE `/v1/rooms/{room_id}/note` | 내 메모 저장·삭제 |

멤버 조회의 `location`은 현재 위치, `note.origin`은 메모 작성 당시 위치다. 공유 OFF 또는 이전 epoch의 위치·메모는 `null`이다. 사진 API는 아직 `501`이다. 세부 필드와 방 API는 팀 공통 연동 규격을 따른다.

다음은 `GET /v1/rooms/{room_id}/members`의 가상 응답이다. `location_status`는 `off`, `no_data`, `fresh`, `delayed`, `stale` 중 하나다.

```json
{
  "ok": true,
  "data": {
    "members": [{
      "member_id": "00000000-0000-4000-8000-000000000001",
      "room_id": "00000000-0000-4000-8000-000000000002",
      "user_id": "00000000-0000-4000-8000-000000000003",
      "nickname": "이안",
      "is_sharing": true,
      "sharing_epoch": 1,
      "joined_at_ms": 1793779200000,
      "location_status": "fresh",
      "location": {
        "latitude": 34.66, "longitude": 135.50, "accuracy_m": null,
        "captured_at_ms": 1793779205000, "received_at_ms": 1793779206000,
        "sharing_epoch": 1, "sequence": 1
      },
      "note": {
        "note_id": "00000000-0000-4000-8000-000000000004",
        "text": "3층 에스컬레이터 옆", "written_at_ms": 1793779207000,
        "origin": {"latitude": 34.66, "longitude": 135.50, "captured_at_ms": 1793779205000},
        "sharing_epoch": 1, "photo_available": false,
        "photo_uploaded_at_ms": null, "photo_path": null
      }
    }]
  }
}
```

공유 설정은 `{"enabled":true}` 또는 `{"enabled":false}`다. 위치 저장은 `latitude`, `longitude`, `captured_at_ms`, `sharing_epoch`, `sequence`를 보내며 `accuracy_m`은 생략하거나 `null`로 보낼 수 있다. 메모 저장은 `text`, `sharing_epoch`를 보낸다. 공유 ON 응답에서 받은 epoch를 사용한다. `/health`의 성공만으로 DB 조회 성공을 판단하지 않는다.

## 실제 DB 통합시험

별도의 **테스트 프로젝트**에서만 실행한다. 빌드 설정에 `-DGOMMANNA_BUILD_HTTP_DB_TESTS=ON`을 추가하고 Python 3를 준비한다. `GOMMANNA_ENV_FILE`에 테스트용 `.env` 경로와 `GOMMANNA_RUN_HTTP_DB_INTEGRATION=1`을 설정한 뒤 `ctest --test-dir build/cpp -C Release --output-on-failure`를 실행한다. 변수가 없으면 실제 DB 시험은 건너뛴다.

시험은 가상의 계정·방·위치를 생성하고 종료한다. 테스트용 사용자와 종료된 방 기록이 남을 수 있다. 방 모듈이 없는 시험에서는 방 생성·참가를 API가 아니라 DB 시험용 도구로 준비하므로, 방 API의 실제 연동 성공을 의미하지 않는다.
