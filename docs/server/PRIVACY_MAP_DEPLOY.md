# 개인정보 보존·공유 지도 서버 업데이트

대상: 기존 서울 Lightsail `43.200.162.52`, `/home/ubuntu/GomManna`, `gommanna.service`.
이 문서는 적용 절차이며, 문서 작성이나 로컬 테스트만으로 운영 DB/AWS가 변경되지는 않는다.

## 바뀌는 것

- 본인 계정(`users`, `guest_sessions` 포함), 방의 본인 데이터, 본인 경로 삭제 API.
- 모두 공유 OFF 또는 위치·기기 상태 갱신이 10분간 없는 시점부터 48시간 후 정리.
- 방 종료부터 48시간 후 방 데이터 정리. 중간 탈퇴자는 방 종료 기한까지 보존하되 본인 요청 시 삭제.
- UTC 달력상 6개월 토큰. 성공한 인증 시 그 시점부터 6개월 뒤로 갱신.
- DB 공용 마킹 추가·이름 변경·삭제. 변경/삭제는 작성자와 방장만.
- 지정 동물 프로필, 배터리와 충전 상태 공유.
- 방·참가자별 경로: 직전 저장점에서 20m 이상 이동 **또는** 60초 경과 시 저장.

API 본문·응답·권한·보존의 정확한 규칙은 [앱 API 문서](../../cpp/http/API.md) 6~10절에 있다.
기존 C 초대 코드·QR 모듈은 그대로 사용한다. DB 재연결이나 프록시 IP 처리 변경은 포함하지 않았다.

## 1. 로컬 변경을 먼저 Git에 올리기

사용자 브랜치 이름은 실제로 `origin/seoyoung`이며 원격 추적 이름은 `origin/origin/seoyoung`이다.
로컬에서 변경 파일을 검토해 **사용자가 커밋·푸시**한 뒤 배포한다. PR 자동 생성은 하지 않는다.
배포할 커밋의 전체 SHA를 기록한다. 수정 사항을 푸시하기 전에는 서버에서 내려받을 수 없다.

최신 main의 서버를 기준으로 변경했고, dev에만 있는 앱 화면 변경을 함께 가져오지는 않았다.
앱 담당자는 자신의 최신 앱 브랜치에서 아래 API 연결 작업을 별도로 진행한다.

## 2. DB 담당자에게 전달할 파일

이안에게 [202611070001_gommanna_privacy_and_shared_map.sql](../../supabase/migrations/202611070001_gommanna_privacy_and_shared_map.sql)을 전달한다.
기존 private 스키마 migration 1~4가 적용되어 있어야 한다. `supabase db push`로
옛 public 시제품까지 한꺼번에 적용하지 않는다.

이 파일은 **DB 관리자**가 적용한다. 서버 전용 `gommanna_login`으로 실행하거나 그 계정에
관리자 권한을 추가하는 방식이 아니다. SQL에 비밀번호·Secret key는 없다.
같은 migration을 중복 실행하지 말고 적용 이력을 기록한다. 전체 SQL은 한 트랜잭션이다.

기존 데이터는 삭제하지 않고 컬럼과 새 테이블을 추가한다. 공유가 모두 OFF인 기존 방의
타이머는 migration 실행 시각부터 시작한다. 이미 종료된 방은 원래 종료 시각을 기준으로
새 서버 첫 정리 때 48시간이 지난 데이터를 삭제한다. 만료 전 세션은 새 유효기간으로
연장하고, 이미 만료·취소된 세션을 되살리지는 않는다.

## 3. 운영 적용 순서

같은 Supabase를 쓰는 **구버전 서버를 모두 중지**한다. 예전 시드니 인스턴스
`32.236.33.77`가 아직 실행 중이라면 그 서버도 포함한다. 구버전은 탈퇴·종료 때 즉시
삭제하므로 새 보존 기준과 함께 실행하면 안 된다. 앱에는 잠시 점검 중임을 알린다.

서울 서버 SSH에서:

```bash
sudo systemctl stop gommanna.service
cd /home/ubuntu/GomManna
git status --short
```

추적 중인 파일의 로컬 변경이 있다면 덮어쓰지 말고 먼저 확인한다. `.env`는 유지한다.
작업 폴더가 깨끗한 상태에서 **1번에서 푸시한 변경을** 받는다.

```bash
git fetch origin
git merge --ff-only origin/origin/seoyoung
git log -1 --format='%H %s'
```

마지막 SHA가 배포하기로 한 커밋인지 확인한다. fast-forward가 불가능하면 중단하고
브랜치 차이를 확인한다. `reset --hard`로 기존 변경을 지우지 않는다.

이안이 2번 SQL을 운영 Supabase에 적용한다. 적용 뒤 관리자가 확인할 값:

```sql
select version from gommanna.schema_versions;
-- 2
```

서버에서 기존 Linux 빌드 도구로 다시 빌드한다. 이 명령은 기존 DB 비밀번호, Storage Secret,
CA, Nginx, IP 인증서와 자동 갱신 설정을 바꾸지 않는다.

```bash
cmake -S cpp -B build/server \
  -DCMAKE_BUILD_TYPE=Release \
  -DGOMMANNA_USE_FAKE_REPOSITORY=OFF \
  -DGOMMANNA_BUILD_HTTP_DB_TESTS=OFF \
  -DBUILD_TESTING=ON
cmake --build build/server --parallel 2
ctest --test-dir build/server --output-on-failure
```

각 명령이 성공한 경우에만 다음으로 진행한다. 여기서는 운영 DB를 사용하는 통합 테스트를
활성화하지 않는다. 새 기능의 실제 DB 테스트는 개발자의 별도 테스트 DB에서 실행한다.

```bash
sudo systemctl start gommanna.service
sudo systemctl is-active gommanna.service
curl --fail --show-error http://127.0.0.1:8080/health
curl --fail --show-error https://43.200.162.52/health
sudo journalctl -u gommanna.service -n 40 --no-pager
```

`active`와 두 health 응답을 확인한다. `/health`만으로 실제 DB 쓰기와 앱 연동 완료를
판단하지 않는다. 앱에서 별도 시험용 계정으로 프로필·마킹·경로·삭제 흐름도 확인한다.
DB 스키마 버전 2가 없으면 새 서버는 시작하지 않는다.
로그에 `Retention cleanup will be retried`가 반복되면 DB 연결/권한과 서비스 상태를 확인한다.
이미 삭제된 데이터는 구버전 실행 파일로 되돌려도 복구되지 않는다. 스키마 변경 뒤에는
이전 서버를 무심코 다시 켜지 말고 실패 원인을 해결한 새 서버로 재시작한다.

## 4. 앱 담당자에게 남은 연결

1. 성공한 인증 응답의 `X-Session-Expires-At-Ms`를 저장해 로컬 만료 시각을 갱신한다.
2. 동물 선택을 `PUT /v1/me/profile`에 저장하고 `members[].animal_icon`을 표시한다.
3. 공유 중 실제 배터리/충전 상태를 `/device-status`로 보내고, 오래된 값은 확정 전원 OFF로 표시하지 않는다.
4. 지도 마킹을 `/marks`로 저장·조회하고 작성자/방장에게 이름 변경·삭제 동작을 연결한다.
5. 경로 조회 결과를 페이지별로 받고 epoch마다 선을 끊는다. 정지 시에도 1분 기록을 원하면
   앱에서 유효한 위치를 최소 1분에 한 번 보내야 한다. 서버가 GPS 점을 만들어 주지는 않는다.
6. 계정 삭제, 해당 방의 내 데이터 삭제, 내 경로 삭제를 구분해서 API에 연결한다.
   나간 방도 삭제하려면 해당 room_id를 기억한다. 삭제 후 로컬 캐시와 위치 전송 상태도 정리한다.
7. 앱에 표시하는 보유 기간 설명은 실제 서버 기준에 맞게 반영한다. 서버 코드만 바뀌어도
   앱 화면·앱 내 정책 문서·Supabase 백업 설정이 자동으로 바뀌지는 않는다.

## 검증 범위

Windows C++20 빌드, 메모리 저장소/정책 단위 테스트, 새로 만든 로컬 PostgreSQL 17의
migration·실제 SQL·HTTP API, 로컬 HTTPS 사진 저장소 연동을 검증했다.
시간을 주입하는 저장소 테스트에서 48시간 직전/정각, 더 이른 삭제 기한, 비활동과 재개,
중간 탈퇴자 보존, 본인만 삭제, 20m/60초 저장 조건, 6개월 월말·윤년을 확인한다.
사진 저장소 실패 후 삭제 재시도도 검증한다. 실제 iPhone, 운영 Supabase/AWS에서의
최종 앱 연동이나 동시접속 부하 시험을 대신하지 않는다.

재실행하려면 새 private migration까지 적용된 폐기 가능한 테스트 DB를 준비하고:

```text
cmake -S cpp -B build/test-server -DBUILD_TESTING=ON -DGOMMANNA_BUILD_HTTP_DB_TESTS=ON
cmake --build build/test-server
```

`GOMMANNA_ENV_FILE`(테스트 DB/Storage 설정), `GOMMANNA_TEST_DATABASE_URL`(같은 테스트 DB),
`GOMMANNA_RUN_HTTP_DB_INTEGRATION=1`, `GOMMANNA_RUN_DB_INTEGRATION=1`,
`GOMMANNA_RUN_SHARED_DB_TESTS=1`, 사진까지라면 `GOMMANNA_RUN_PHOTO_INTEGRATION=1`을
해당 터미널에 설정하고 `ctest --test-dir build/test-server --output-on-failure`를 실행한다.
실제 DB 테스트에는 테스트 사용자·종료된 방이 남을 수 있으므로 운영 환경에서는 실행하지 않는다.
