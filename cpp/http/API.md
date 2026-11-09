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

이후 발급받은 토큰을 사용한다. 유효기간은 **UTC 달력상 6개월**이며 인증에 성공할
때마다 그 시각부터 6개월 뒤로 만료 시각을 갱신한다. 해당 월에 같은 날짜가 없으면
월말을 사용한다(8월 31일 → 다음 해 2월 말일). 토큰 문자열은 바뀌지 않는다.
인증된 응답의 `X-Session-Expires-At-Ms` 헤더를 앱에 저장한 세션 만료 시각에 반영한다.
처음 발급 당시의 만료 시각만으로 사용 중인 사용자를 로그아웃시키지 않는다.
서버는 원문 토큰 대신 SHA-256 해시만 저장한다. 이미 만료·취소된 토큰은 갱신하지
않으며, 만료 뒤 계정 복구 API는 없다.
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

## 6. 내 프로필과 삭제 요청

모든 요청에 Bearer 토큰이 필요하다. 서버는 요청 본문의 사용자 ID 대신 **인증된 본인**을
대상으로 처리한다. 삭제 API에 다른 사용자 ID를 넘기는 기능은 없다.

| 요청 | 본문 | 응답 `data` |
|---|---|---|
| GET /v1/me | 없음 | `user: {user_id, created_at_ms, animal_icon}` |
| PUT /v1/me/profile | `{"animal_icon":"fox"}` | 갱신된 `user` |
| DELETE /v1/me | 없음 | 202 · `{"deleted":true,"storage_cleanup":"queued"}` |
| DELETE /v1/rooms/{room_id}/me/data | 없음 | 202 · `{"deleted":true,"storage_cleanup":"queued"}` |
| DELETE /v1/rooms/{room_id}/me/route | 없음 | 200 · `{"deleted":true}` |

동물 ID는 `bear,rabbit,fox,cat,dog,panda,koala,tiger,penguin`이며 기본은 `bear`다.
서버에 이미지를 올리지 않고 앱의 지정 캐릭터를 표시한다. `GET .../members`의
각 멤버에 `animal_icon`이 추가되며, 사용자가 참가한 모든 방에 같은 프로필이 보인다.

계정 삭제는 `users`, 모든 `guest_sessions`, 본인 멤버십·위치·메모·사진 연결·경로·마킹을
트랜잭션 안에서 제거한다. 실제 사진 파일은 Storage 삭제 대기열로 제거한다.
방장이 삭제하면 남은 참가자 중 가장 먼저 참가한 사람에게 방장을 넘기고, 아무도 없으면
방을 종료한다. 다른 사람의 데이터는 지우지 않는다. 성공 후 앱은 토큰·현재 방·본인 캐시를
비워야 한다. 삭제된 토큰의 후속 요청은 401이다.

`me/data`는 **그 방의 본인 데이터**만 삭제하고 계정·참가 상태는 유지한다.
공유를 OFF로 바꾸고 epoch를 증가시켜 진행 중인 이전 위치·사진 요청을 차단한다.
앱도 위치 전송을 중단하고, 다음 공유 ON 응답에서 새 epoch를 받아야 한다.
`me/route`는 과거 경로만 지운다. 공유가 ON이면 이후 위치부터 다시 기록한다.
두 삭제 요청은 나간 방·종료된 방에서도 보존 중인 본인 데이터에 사용할 수 있다.
방 자체가 48시간 정리로 삭제됐다면 404다. 앱은 나간 방의 삭제 메뉴에 필요한 room_id를
보유해야 한다. 현재 서버에는 과거 참가 방 목록 API가 없다.

## 7. 배터리 상태

`PUT /v1/rooms/{room_id}/device-status`

```json
{"battery_percent":12,"is_charging":false,"sharing_epoch":1}
```

세 필드를 모두 보낸다. 배터리는 0~100 정수, 충전 여부는 boolean이며 기기가 알 수 없으면
각각 null이다. 본인이 공유 ON일 때 현재 epoch로만 전송할 수 있다. 앱이 측정값을
보내야 하며 서버가 기기의 배터리를 직접 알아내지는 않는다.
멤버 조회의 `device_status`는 다음 형태이고 OFF·탈퇴·이전 epoch 값은 숨긴다.

```json
{"battery_percent":12,"is_charging":false,"updated_at_ms":1794052800000,"sharing_epoch":1,"status":"recent"}
```

서버 수신 후 120초부터 `status:"stale"`이다. 배터리가 적었고 위치·기기 상태가 오래
갱신되지 않았다면 전원 꺼짐 **가능성**을 안내할 수 있지만, 통신 단절·앱 종료와 구별할
수 없으므로 서버는 `powered_off` 같은 확정 상태를 보내지 않는다.

## 8. 공유 지도 마킹

| 요청 | 본문 | 응답 `data` |
|---|---|---|
| GET /v1/rooms/{room_id}/marks | 없음 | `marks` 배열 |
| POST /v1/rooms/{room_id}/marks | `{"name":"만날 장소","latitude":37.5,"longitude":127.0}` | 201 · `mark` |
| PUT /v1/rooms/{room_id}/marks/{mark_id} | `{"name":"새 이름"}` | `mark` |
| DELETE /v1/rooms/{room_id}/marks/{mark_id} | 없음 | `{}` |

`mark`에는 `mark_id,room_id,creator_user_id,name,latitude,longitude,created_at_ms,updated_at_ms`가
들어간다. 이름은 앞뒤 공백 제거 후 1~60글자, 방당 최대 200개다. 진행 중인 방의 현재
참가자만 조회·생성하며 **작성자 또는 방장**만 이름 변경·삭제한다.
마킹은 GPS 공유 ON 여부와 별개인 공용 데이터다. 작성자가 나가면 숨기고 보존 규칙에
따라 정리한다. 재참가하면 보존된 본인 마킹이 다시 보인다.
서버 push/WebSocket은 추가하지 않았으므로 앱은 변경 후 또는 주기적으로 목록을 조회한다.

## 9. 경로 기록·조회

기존 `PUT .../location`이 최신 위치를 갱신하면서 경로도 저장한다. 별도의 경로 업로드는 없다.

- 첫 위치와 새 공유 epoch의 첫 위치는 저장한다.
- **직전 DB에 저장한 경로점**에서 20m 이상 이동했거나 마지막 저장 후 서버 수신 시각으로
  60초 이상 지났다면 한 점을 추가한다. 둘 중 하나만 만족하면 된다.
- 5초마다 받은 최신 위치는 계속 갱신하지만 모든 요청을 경로에 추가하지 않는다.
- 요청이 없으면 정지 좌표를 만들어 넣지 않는다. 정지 중에도 1분마다 기록하려면 앱이
  유효한 위치를 최소 1분에 한 번 보내야 한다. GPS 콜백이 없을 때 전송하는 부분은 앱 작업이다.
- epoch가 다른 점끼리 선을 잇지 않는다. 중복·역순 sequence와 이전 epoch 요청은 거절한다.

`GET /v1/rooms/{room_id}/me/route`는 본인 경로,
`GET /v1/rooms/{room_id}/members/{member_id}/route`는 해당 참가자의 경로를 조회한다.
선택 query는 `after_id`(기본 0), `limit`(기본 200, 1~500)다.

```json
{"ok":true,"data":{"points":[{"point_id":123,"member_id":"<UUID>","latitude":37.5,"longitude":127.0,"accuracy_m":5,"captured_at_ms":1794052800000,"received_at_ms":1794052800100,"sharing_epoch":1,"sequence":1}],"next_after_id":null}}
```

다음 페이지가 있으면 `next_after_id`를 `after_id`로 보낸다. 끝이면 null이다.
새 점을 폴링할 때는 마지막으로 받은 point_id를 기억해 요청할 수 있다.
본인은 모든 epoch를 조회하며, 나간 방·종료된 방에서도 삭제 전까지 조회할 수 있다.
타인 경로는 같은 방의 현재 참가자에게, 대상이 공유 ON인 **현재 epoch**만 보인다.
대상이 OFF·탈퇴 상태면 빈 배열이다. 방이 종료됐거나 조회자가 탈퇴하면 타인 경로 접근을 거절한다.

## 10. 자동 보존·삭제 기준

| 상황 | 정리 대상이 되는 시점과 범위 |
|---|---|
| 모든 현재 참가자 공유 OFF | 마지막 OFF +48시간: 현재 참가자의 위치·메모·사진·경로·마킹·배터리 |
| 모두 비활동 | 마지막 위치·기기 상태 갱신 +10분을 비활동 시작으로 보고 그 시점 +48시간: 위와 동일 |
| 방 종료 | 종료 +48시간: 방과 모든 참가자의 방 데이터, 중간 탈퇴자 포함 |
| 중간 탈퇴자 | 타인 화면에서 즉시 숨김, 자동 삭제는 방 종료 +48시간까지 대기 |
| 본인 삭제 요청 | DB 데이터 즉시 제거, 사진 객체는 재시도 대기열로 제거 |

활동 시각은 공유 ON, 위치·기기 상태 수신으로 갱신한다. GET 폴링·인증·마킹·메모·사진 편집은
위치 활동 기한을 연장하지 않는다. 누군가 공유를 재개하면 모두 OFF 기한을 취소하고,
새 위치·기기 상태를 받으면 비활동 기한을 다시 계산한다. 방 종료가 이미 시작된 더 이른
삭제 기한을 뒤로 미루지는 않는다. **중간 탈퇴자의 경로도 사용자 데이터 보존 예외에 포함**한다.

서버 시작 시와 약 60초마다 최대 100개 방을 정리한다. 서버가 꺼져 있던 중 기한이 지나면
다음 시작 시 처리한다. 사진은 DB에서 연결을 끊은 뒤 `photo_cleanup`에서 최대 20개씩
삭제하며 저장소 장애 시 재시도한다. 따라서 물리 삭제 완료는 작업 주기·대기열·장애 복구에
따라 늦어질 수 있다. Supabase 별도 백업의 보관 설정은 이 코드가 변경하지 않는다.

DB 적용과 배포 순서는 [서버 업데이트 안내](../../docs/server/PRIVACY_MAP_DEPLOY.md)를 참고한다.
