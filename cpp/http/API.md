# 곰만나 앱 연동 API

## 공통

- 앱 → C++ HTTP 서버 → Supabase DB·비공개 사진 저장소. 앱에 DB 주소·DB 비밀번호·Storage 비밀키를 넣지 않는다.
- 인증이 필요한 요청은 `Authorization: Bearer <익명 토큰>`을 보낸다. 토큰은 기기의 안전한 저장소에 보관한다.
- 요청과 응답의 JSON 필드는 snake_case다. 시각은 UTC Unix **밀리초**, 좌표는 십진 도다.
- 성공: `{"ok":true,"data":{...}}`. 오류: `{"ok":false,"error":{"code":"...","message":"..."}}`.
- JSON 요청은 `Content-Type: application/json`을 사용한다. 명시되지 않은 필드는 보내지 않는다.
- 기본 서버 주소 `127.0.0.1:8080`은 서버를 실행한 컴퓨터 내부에서만 접근 가능하다. 아이폰 연동에는 실제 접근 주소와 HTTPS 구성을 별도로 준비한다.

## 1. 익명 세션

`POST /v1/guests` · 인증 불필요 · 요청 `{}` · 성공 201

```json
{"ok":true,"data":{"user_id":"00000000-0000-4000-8000-000000000001","token":"<64자리 토큰>","expires_at_ms":1799000000000}}
```

이후 발급받은 토큰을 사용한다. 기본 유효기간은 30일이며 만료 뒤 복구·갱신은 아직 없다.
`GET /health`도 인증이 필요 없지만, 그 응답만으로 DB 연결이 정상이라는 뜻은 아니다.

## 2. 방과 초대

| 요청 | JSON 본문 | 응답 `data` |
|---|---|---|
| POST /v1/rooms | `{"title":"오사카 여행","nickname":"이안"}` | `room`, 방장 `member` · 201 |
| POST /v1/rooms/join | `{"invite_code":"ABCD23","nickname":"주원"}` | `room`, 참가자 `member` |
| GET /v1/rooms/{room_id} | 없음 | `room` |
| GET /v1/rooms/{room_id}/members | 없음 | `members` 배열 |
| GET /v1/rooms/{room_id}/invite | 없음 | `invite_code`, `qr_payload`, `qr_svg` |
| POST /v1/rooms/{room_id}/leave | `{}` | `{}` · 방장 제외 |
| POST /v1/rooms/{room_id}/end | `{}` | `{}` · 방장만 |

초대 코드는 `ABCDEFGHJKLMNPQRSTUVWXYZ23456789` 중 6자리다. 앞뒤 공백과
소문자는 정규화한다. 최대 8명이고, 같은 사용자가 중복 참가하면 기존 참가 정보를
반환한다. QR에는 예를 들어 `gommanna://join?code=ABCD23`이 들어간다.
서버 QR 생성과 iOS 카메라 인식·딥링크 연결은 별도다.

`room`에는 `room_id,title,host_id,invite_code,status,created_at_ms,ended_at_ms`가,
`member`에는 `member_id,room_id,user_id,nickname,is_sharing,sharing_epoch,joined_at_ms`가
들어간다. 방 생성 직후 공유 상태는 OFF다.

## 3. 공유 상태·위치·메모

| 요청 | JSON 본문 | 응답 `data` |
|---|---|---|
| PUT /v1/rooms/{room_id}/sharing | `{"enabled":true}` 또는 `{"enabled":false}` | `member` |
| PUT /v1/rooms/{room_id}/location | 아래 예시 | `location` |
| PUT /v1/rooms/{room_id}/note | `{"text":"3층 입구","sharing_epoch":1}` | `note` |
| DELETE /v1/rooms/{room_id}/note | 없음 | `{}` |

```json
{"latitude":34.66,"longitude":135.50,"accuracy_m":null,"captured_at_ms":1793779205000,"sharing_epoch":1,"sequence":1}
```

공유 ON 응답의 `member.sharing_epoch`를 위치·메모 저장에 사용한다. 같은 epoch에서
위치 `sequence`를 증가시킨다. OFF→ON이면 epoch가 바뀌고 새 위치를 보내기 전에는
이전 위치가 다시 나타나지 않는다. 앱이 백그라운드에서 위치를 항상 전송한다고 보장하지 않는다.

멤버 조회에서 `location_status`는 `off,no_data,fresh,delayed,stale` 중 하나다.
`location`은 현재 위치, `note.origin`은 메모를 쓸 때의 위치다. OFF 또는 이전 epoch의
위치·메모·사진은 null이다. 오래된 위치의 도보 시간은 현재 시간으로 표시하지 않는다.

## 4. 메모 사진

`PUT /v1/rooms/{room_id}/note/photo`는 인증과 `multipart/form-data`가 필요하다.
텍스트 필드 `note_id`, `written_at_ms`, `sharing_epoch`와 파일 필드 `file`을 보낸다.
`note_id`와 `written_at_ms`는 직전에 저장한 메모 응답에서 가져온다. 업로드 중 메모가
바뀌거나 공유가 꺼지면 첨부가 거절된다.

- JPEG/PNG, 원본과 처리된 파일 각각 최대 5 MiB, 최대 4096×4096 픽셀.
- 서버에서 이미지 내용을 검사하고 다시 인코딩해 위치 등 메타데이터를 제거한다. 방향 정보도 제거되므로 앱은 업로드 전에 사진 방향을 정규화해야 한다.
- 성공 응답 `data.note`의 `photo_available`, `photo_uploaded_at_ms`, `photo_path`를 사용한다. 비공개 저장소 키는 응답에 포함되지 않는다.
- `GET /v1/rooms/{room_id}/members/{member_id}/photo`는 Bearer 인증 후 이미지 바이트를 반환한다. 같은 방이고 상대가 공유 중인 경우에만 내려준다.
- 사진이 없는 경우 `photo_available:false`와 null 경로가 반환된다. 사진 저장소 설정이 없으면 사진 API는 503이다.

## 5. 오류와 호출 제한

| 상태 | 주요 코드 |
|---|---|
| 400 | INVALID_ARGUMENT: 필드·형식·좌표 오류 |
| 401 | UNAUTHENTICATED: 누락·만료된 토큰 |
| 403 | FORBIDDEN: 비참가자 또는 방장 권한 필요 |
| 404 | NOT_FOUND: 대상 방·코드·사진이 없음 또는 사진 비공개 |
| 409 | CONFLICT, INVITE_CODE_CONFLICT, SHARING_OFF, STALE_UPDATE |
| 410 | ROOM_CLOSED |
| 413 | PHOTO_TOO_LARGE |
| 429 | RATE_LIMITED: 잠시 후 재시도 |
| 503 | DB_UNAVAILABLE, STORAGE_UNAVAILABLE |

코드 추측을 줄이기 위해 방 참가와 익명 세션 생성은 각각 접속 주소당 1분에
30회까지 허용한다. 현재 제한은 단일 서버 프로세스 메모리 기준이다. 여러 서버나
프록시 뒤의 공개 배포에는 별도의 공통 제한·HTTPS·보안 점검이 필요하다.
