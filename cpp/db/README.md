# 곰만나 DB 어댑터

이안 담당 코드다. C++ 서버는 `make_postgres_repository`로 객체를 만들고 공통 `IRepository`만 사용한다. HTTP 경로와 JSON 변환은 포함하지 않는다.

## 현재 구현

- Supabase PostgreSQL 연결, 익명 세션 조회, 방 생성·참가·탈퇴·종료
- 공유 ON/OFF와 공유 세션 번호, 최신 위치, 최신 메모의 저장·조회·삭제
- 위치 공유 중지 시 해당 위치·메모를 DB 조회에서 제외하고, 재개 후 이전 세션의 정보를 다시 공개하지 않음
- 사진 연결과 Storage 삭제 대기열, 48시간 보존 정리
- 인증 성공마다 UTC 달력상 6개월로 만료 갱신, 본인 계정·방 데이터·경로 삭제
- 동물 프로필·배터리 공유, 공용 마킹, 20m/60초 경로 저장과 페이지 조회
- 다른 사용자의 요청 여부와 방 상태를 트랜잭션 안에서 검사. SQL 파라미터를 별도로 전달하고 오류에 DB 비밀번호를 넣지 않음

방과 참가자 행을 잠그므로 한 방의 요청은 단순하게 직렬화된다. 중간 프로젝트용 구현이며 다수 사용자에서의 처리량은 시험하지 않았다. 입력한 사용자 ID는 서버가 인증한 결과여야 한다. DB에 `gommanna_server` 권한을 가진 서버만 접속하며 연결할 때 `SET ROLE gommanna_server`를 실행한다. 이 서버 권한은 모든 곰만나 행에 접근할 수 있으므로 앱에 DB 연결 정보를 넣으면 안 된다.

## 독립 빌드

C++20, CMake 3.24 이상과 PostgreSQL libpq 17 이상이 필요하다. macOS/Windows 각각의 개발 환경에 맞는 클라이언트 라이브러리와 헤더를 설치한다. `PostgreSQL_ROOT`는 자신의 설치 경로를 지정한다.

```text
cmake -S cpp/db -B build/db -DBUILD_TESTING=ON -DPostgreSQL_ROOT=<PostgreSQL-설치-경로>
cmake --build build/db --config Debug
ctest --test-dir build/db -C Debug --output-on-failure
```

Windows에서 PostgreSQL 설치 폴더의 `bin/libpq.dll`을 찾으면 CTest에 필요한 경로를 자동으로 설정한다. 자동으로 찾지 못하면 PostgreSQL `bin` 폴더를 PATH에 추가하고 시험한다. CMake 빌드 성공과 시험 프로그램 실행 성공은 별도로 확인한다.

실제 DB 시험은 별도 표시한 테스트 프로젝트에서만 실행한다. `GOMMANNA_TEST_DATABASE_URL`, `GOMMANNA_RUN_DB_INTEGRATION=1`과 올바른 PostgreSQL CA 인증서(`PGSSLROOTCERT`)가 없으면 시험을 건너뛴다. 어댑터는 연결 시 `sslmode=verify-full`을 강제한다. 시험에는 가상 위치와 이름만 사용한다. 사용자·종료된 방 등 테스트 레코드가 DB에 남을 수 있다.

실제 DB 연결 정보는 비밀번호다. Git, 앱 코드, 스크린샷, PR, 채팅에 넣지 않는다. URL의 호스트·포트는 Supabase Connect의 **Session pooler** 값을 사용한다. macOS나 다른 개발자의 컴퓨터에서는 각자 설치한 libpq와 환경 변수로 빌드해야 하며, Windows에서 빌드한 실행 파일을 그대로 공유하지 않는다.

## 기존 시제품 migration 주의

`supabase/migrations/202610290001_gommanna.sql`은 기존 시제품으로 public 스키마와 24시간 만료를 사용한다. 새 `gommanna` 비공개 스키마와는 별개이며 현재 기획의 기준이 아니다. 두 파일을 합의 없이 한꺼번에 적용하지 않는다. 특히 migration 이력을 정리하기 전에는 `supabase db push`로 전체 파일을 실행하지 않는다. 새 테스트 프로젝트에는 새 비공개 스키마 migration만 적용했다.

## 연동할 때

- 원규: `RepositoryConfig{connection_string}`으로 생성하고 `Result` 오류를 JSON·HTTP 상태로 변환한다. 토큰 생성과 해시, 요청자 인증은 HTTP 서버 담당이다.
- 서영: 방 상태와 저장은 `IRepository`를 호출한다. 방 인원 제한과 중복 참가도 DB에서 다시 확인한다.
- 주원: `MemberView`의 위치 상태 및 거리 계산 경계값을 `classify_location`과 통합 시 비교한다. DB 코드에는 같은 기본 기준을 적용한 임시 판별이 있다.

사진 API, 실제 iOS/GPS, 방 API를 포함한 팀 전체 통합시험은 이 디렉터리만으로 완성되지 않는다. 서버의 실행과 DB/API 시험은 [HTTP 안내](../http/README.md)를 참고한다.

## 이안의 실행 결과와 설명 포인트

2026-11-03에 Windows에서 C++ 코드와 테스트를 빌드하고 전용 Supabase 테스트 프로젝트에 직접 연결해 `gm_db_config`, `gm_db_integration`을 실행했다. 게스트 인증, 방 생성·참가·종료, 위치·메모, OFF/ON 후 정보 숨김, 오래된 위치 요청 거절을 가상 데이터로 확인했다. 11월 4일에는 HTTP 서버를 거쳐 위치·메모 저장과 조회를 실제 테스트 DB에서 확인했다. 이때 방은 DB 시험용 도구로 만들었고, 서영의 방 API와 실제 휴대폰 위치는 아직 시험하지 않았다. 동시에 요청이 들어오는 상황과 macOS 빌드도 별도 확인이 필요하다.

발표나 코드 확인에서는 다음 흐름을 설명할 수 있으면 된다.

1. 서버가 익명 토큰을 확인해 사용자 ID를 넘긴다. DB 코드는 ID만으로 외부 요청을 인증했다고 가정하지 않는다.
2. `IRepository`는 다른 팀원이 부르는 공통 창구이고, `PostgresRepository`는 실제 DB에 값을 저장하거나 읽는 구현이다.
3. 저장 전에 방이 열려 있는지, 요청자가 참가자인지, 본인의 공유가 켜져 있는지와 공유 세션 번호를 확인한다.
4. 여러 조건 확인과 저장을 하나의 트랜잭션으로 묶고, 방·참가자 행을 잠가 공유 중지와 저장이 엇갈리지 않게 한다.
5. 공유 OFF 또는 이전 세션의 위치·메모는 조회 결과에서 제외한다. 메모를 쓸 당시의 위치는 현재 위치와 따로 저장한다.

테스트에서 사용한 관리자 DB 비밀번호는 권한이 넓다. 서버 연동에는 별도의 `gommanna_login` 계정을 사용한다. 이 계정은 서버 전용 `gommanna_server` 역할로 전환해 필요한 테이블에 접근한다. 비밀번호는 migration이나 Git에 기록하지 않고 별도로 설정하며, 접속 정보는 안전한 경로로 전달한다. 현재 테스트 DB를 운영 서비스처럼 사용하지 않는다.
