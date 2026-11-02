# GomManna C++ 서버

C++20 HTTP JSON API 서버입니다. iOS 앱 → C++ 서버 → Supabase PostgreSQL 순서로 사용합니다.
Supabase RPC나 Supabase Auth는 사용하지 않고, 서버 자체 익명 사용자 JWT를 사용합니다.

## 시작

macOS 개발 도구:

```bash
brew install --formula cmake postgresql@17
cd server
./scripts/build.sh
cp .env.example .env
openssl rand -hex 32
```

`.env`에 다음 두 항목을 입력합니다.

- `DATABASE_URL`: Supabase 프로젝트의 **Connect**에서 PostgreSQL 연결 문자열을 복사합니다. IPv4 환경이면 **Session pooler**를 사용할 수 있습니다. 이 서버에서는 Direct 또는 Session 모드를 사용하세요. DB 비밀번호의 특수문자는 URL 인코딩합니다.
- `AUTH_SECRET`: 위 `openssl` 명령의 결과를 입력합니다. 토큰 검증에 사용하므로 서버 재시작·복제 인스턴스에서도 같은 값을 유지합니다.

먼저 새 Supabase 프로젝트의 SQL Editor에서 [테이블 migration](../supabase/migrations/202610290001_gommanna.sql)을 실행한 뒤 서버를 시작합니다. 기존 테이블이 있다면 migration을 다시 실행하지 않습니다.

```bash
./scripts/run.sh
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/ready
```

`.env`는 Git 및 Docker 빌드에서 제외됩니다. 값이 터미널 환경 변수로 지정되어 있으면 `.env`보다 우선합니다.
`sslmode=verify-full&sslrootcert=system`은 DB 서버의 인증서를 시스템 CA로 검증합니다. 프로젝트에서 별도 CA 인증서가 필요하면 Supabase에서 받은 인증서를 `sslrootcert` 파일 경로로 지정합니다.
실제 iPhone에서 접속하려면 `HOST=0.0.0.0`과 Mac의 LAN 주소를 사용합니다. 현재 앱의 로컬 임시 데이터는 이 서버에 아직 연결하지 않았습니다.

## 설정

| 환경 변수 | 기본값 | 설명 |
|---|---|---|
| `DATABASE_URL` | 필수 | PostgreSQL URL 또는 libpq 연결 문자열 |
| `AUTH_SECRET` | 필수 | 무작위 32자 이상 |
| `HOST` | `127.0.0.1` | 수신 인터페이스 |
| `PORT` | `8080` | HTTP 포트 |
| `DB_POOL_SIZE` | `4` | DB 연결 수, 1~32 |
| `TOKEN_TTL_HOURS` | `720` | 토큰 유효기간, 기본 30일 |
| `ROOM_TTL_HOURS` | `24` | 모임 유효기간, 1~168시간 |

HTTP는 로컬 개발용입니다. 외부 배포 시 HTTPS를 제공하는 리버스 프록시 뒤에서 사용합니다.
서버는 전달된 `user_id`, `host_id`, `sender_id`를 신뢰하지 않고 토큰에서 사용자 ID를 결정합니다.
원본 DB 오류·접속 문자열·토큰·메시지·좌표를 로그에 남기지 않습니다.

## 인증

```bash
curl -X POST http://127.0.0.1:8080/v1/auth/guest
```

응답에는 `access_token`, `token_type`, `expires_in`, `user`가 있습니다.
나머지 `/v1` 요청에는 `Authorization: Bearer <access_token>`을 보냅니다.
이 토큰은 Supabase 토큰이 아니라 **C++ 서버 전용 토큰**입니다.
iOS에서는 Keychain에 저장하고 유효기간이 지나기 전에 `/v1/auth/renew`로 갱신합니다.
현재는 외부 계정 연동·만료된 토큰의 복구·개별 토큰 철회 기능이 없습니다. 토큰을 잃거나 만료되면 새로운 익명 사용자로 시작합니다.

## API

요청 본문은 `Content-Type: application/json`을 사용합니다. 시간은 UTC ISO 8601 문자열입니다.
성공 응답은 JSON, 실패 응답은 `{"error":{"code":"...","message":"..."}}`입니다.

| 메서드 | 경로 | 본문 / 동작 |
|---|---|---|
| GET | `/health` | 프로세스 확인, DB 연결 불필요 |
| GET | `/ready` | DB 연결 및 테이블 6개 확인 |
| POST | `/v1/auth/guest` | 사용자·랜덤 동물 생성, 토큰 발급 |
| POST | `/v1/auth/renew` | 유효한 기존 토큰으로 토큰 갱신 |
| GET | `/v1/me` | 내 프로필 |
| PATCH | `/v1/me` | `{"avatar":"bear"}` |
| GET | `/v1/rooms/current` | 내 모임·멤버·공유 위치·마킹. 없으면 `{"room":null}` |
| POST | `/v1/rooms` | `{"title":"서울 산책","nickname":"주원"}` |
| POST | `/v1/rooms/join` | `{"invite_code":"ABCDEFGH","nickname":"서영"}` |
| DELETE | `/v1/rooms/current/membership` | 참여자 나가기, 방장은 불가 |
| DELETE | `/v1/rooms/current` | 방장만 모임 종료, 연결 데이터 연쇄 삭제 |
| PATCH | `/v1/rooms/current/membership` | `nickname`, `transport_mode`, `is_sharing` 중 변경할 필드 |
| PUT | `/v1/rooms/current/location` | `latitude`, `longitude`, 선택 `accuracy_m`, `recorded_at` |
| GET | `/v1/rooms/current/messages` | 최신 50개, 오래된 순서로 반환 |
| GET | `/v1/rooms/current/messages?before=<message_id>` | 이전 메시지 50개. 응답의 `next_before`를 사용 |
| POST | `/v1/rooms/current/messages` | `{"body":"어디쯤 왔나요?"}` |
| POST | `/v1/rooms/current/marks` | `{"name":"카페 앞","latitude":37.5665,"longitude":126.978}` |
| PATCH | `/v1/rooms/current/marks/<id>` | `{"name":"공원 입구"}`, 만든 사람만 가능 |
| DELETE | `/v1/rooms/current/marks/<id>` | 만든 사람만 가능 |

`avatar`: `bear`, `rabbit`, `fox`, `cat`, `dog`, `panda`, `koala`, `tiger`, `penguin`.
`transport_mode`: `walking`, `driving`, `transit`.
초대 코드는 혼동되는 문자를 제외한 8자리로 서버에서 생성합니다. QR에는 같은 코드를 넣습니다.
모임 생성/참여 응답과 현재 모임 조회 응답은 같은 `room` 구조를 사용합니다.
멤버의 `user_id`는 사용자 식별자, `id`는 해당 모임의 참여 식별자입니다. `my_member_id`로 내 멤버를 구분합니다.
다른 사람의 프로필은 변경할 수 없습니다. 프로필 변경은 모든 모임 조회에 반영됩니다.

### 위치 공유

기본값은 꺼짐입니다. 먼저 `PATCH /v1/rooms/current/membership`으로 `{"is_sharing":true}`를 전송합니다.
공유를 끄면 해당 멤버의 저장된 위치도 삭제합니다. 위치는 최신 하나만 유지하고, 이전 측정 시각의 요청은 최신 좌표를 덮어쓰지 않습니다.
`recorded_at`은 최근 5분~미래 1분 범위만 허용합니다. 생략하면 DB 서버 시각을 사용합니다.
거리·도착 시간·대중교통 API는 아직 연결하지 않았습니다.

### 종료·만료·알림

- 사용자당 하나의 모임만 허용합니다. 생성·참여는 트랜잭션과 사용자/방 잠금으로 동시 요청을 처리합니다.
- 방장이 종료하면 멤버·위치·대화·마킹이 모두 삭제됩니다. 사용자 프로필은 남습니다.
- 60초마다 만료된 방을 삭제합니다. 정리 전에도 만료된 방의 조회·참여·쓰기 요청은 차단합니다.
- 현재 모임 조회로 방 삭제를 확인한 앱은 위치 전송을 멈추고 홈으로 돌아가야 합니다.
- 현재는 HTTP 조회 방식입니다. WebSocket/SSE/푸시 알림은 포함하지 않았습니다. 초기에는 방/메시지를 주기적으로 재조회하고, 앱이 백그라운드로 가면 조회와 위치 전송을 중지하는 방식으로 연결하세요.

요청 본문은 최대 16 KiB, 일반 요청은 IP/사용자별 분당 600회로 제한합니다.
익명 사용자 생성은 IP당 시간당 20회, 초대 코드 입력은 IP당 10분당 30회로 제한합니다.
제한은 단일 서버 메모리에 유지됩니다. 여러 서버 배포 시 공유 저장소나 API Gateway 제한으로 확장해야 합니다.
프록시 헤더는 직접 신뢰하지 않습니다. 프록시 뒤에서는 IP별 제한이 프록시 단위로 적용될 수 있습니다.

## 검증

```bash
./scripts/test.sh
# PostgreSQL 경로가 다르면:
./scripts/test.sh --pg-bin /path/to/postgresql/bin
```

- 단위 테스트: JWT 서명·만료·필수 클레임, 입력 검증, 초대 코드, 요청 제한.
- 통합 테스트: 별도 임시 PostgreSQL을 생성하고 HTTP 서버에 실제 요청합니다.
  원격 Supabase나 기존 개발 DB는 사용하지 않습니다. 종료 후 임시 DB·프로세스를 정리합니다.
- 같은 이름의 멤버 구분, 비참여자 접근 차단, 대화 페이지 구분, 위치 공유 해제·시각 순서,
  마킹 작성자 확인, 종료 시 연쇄 삭제, 동시 방 생성·참여/종료 경합, 만료 후 재참여를 검증합니다.

Linux에서는 CMake, C++20 컴파일러, OpenSSL 개발 패키지, PostgreSQL 개발 패키지가 필요합니다.
예: Debian/Ubuntu의 `g++ cmake libssl-dev libpq-dev postgresql python3`.
빌드 시 고정 버전·SHA-256의 cpp-httplib, nlohmann/json, jwt-cpp를 다운로드합니다.

## Docker

```bash
docker build -t gommanna-server ./server
docker run --rm --env-file server/.env -e HOST=0.0.0.0 -p 8080:8080 gommanna-server
```

Dockerfile은 비루트 사용자로 실행하며 `.env`는 이미지에 넣지 않습니다.
로컬 CMake 빌드와 임시 PostgreSQL 테스트를 검증했습니다. Docker 이미지 빌드와 실제 Supabase 접속은 별도 환경에서 확인해야 합니다.
