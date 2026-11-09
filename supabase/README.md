# 곰만나 DB와 사진 저장소

현재 C++ 서버는 Supabase PostgreSQL의 비공개 `gommanna` 스키마를 사용한다.
앱은 DB에 직접 접근하지 않는다. 서버 전용 로그인은 `gommanna_server` 역할로
전환하며 실제 비밀번호는 migration이나 Git에 넣지 않는다.

## 적용된 구조

- `users`, `guest_sessions`: 익명 사용자와 토큰 해시.
- `rooms`, `room_members`: 방, 6자리 초대 코드, 참가자와 공유 상태.
- `member_locations`, `member_notes`: 참가자별 최신 위치와 메모·작성 당시 위치.
- `photo_cleanup`: 비공개 Storage에서 삭제할 사진의 재시도 대기열.
- `map_marks`, `route_points`: 공용 마킹과 20m/60초 조건을 충족한 경로점.
- `schema_versions`: 서버가 시작할 때 확인하는 스키마 버전.

사진 파일은 비공개 Storage 버킷에 저장하며 DB에는 내부 키만 남긴다. 메모 수정·삭제,
본인 삭제 요청·48시간 보존 기한 만료로 연결이 끊긴 사진 키는 대기열에 기록되고
서버가 삭제를 재시도한다. 탈퇴·종료 자체로 즉시 지우지는 않는다.
업로드 중 실패한 객체도 미리 예약한 대기열에서 정리한다.

## Migration 주의

이 저장소에는 이전 시제품의 `202610290001_gommanna.sql`이 함께 남아 있다.
이는 현재 private 스키마와 다른 public 스키마용 참고 파일이므로 기존 프로젝트에
무작정 실행하지 않는다.

현재 private 스키마 관련 변경은 아래 순서다.

1. `20261103021215_gommanna_private_core.sql`: private 스키마와 서버 역할.
2. `20261103043752_gommanna_server_login.sql`: 서버 전용 로그인. 비밀번호는 별도 설정.
3. `20261104075107_gommanna_six_character_invites.sql`: 새 방의 초대 코드를 6자리로 변경. 종료된 8자리 기존 기록은 보존.
4. `20261104081015_gommanna_photo_cleanup.sql`: 사진 삭제 재시도 대기열과 메모 변경 trigger.
5. `202611070001_gommanna_privacy_and_shared_map.sql`: 6개월 세션, 계정 삭제 cascade,
   탈퇴·보존 기한, 프로필·배터리·마킹·경로, 스키마 버전 2.

5번 적용 전에는 같은 DB를 쓰는 이전 버전 서버를 모두 중지한다. 이전 서버가 계속
동작하면 탈퇴·종료 때 즉시 지우는 기존 동작이 새 보존 규칙을 깨뜨릴 수 있다.
DB 관리자가 적용하며 서버 전용 로그인에 DDL 권한을 추가하지 않는다.
새 서버는 스키마 버전 2가 없으면 시작하지 않는다.
[업데이트 절차](../docs/server/PRIVACY_MAP_DEPLOY.md)를 참고한다.

기존 Supabase 프로젝트의 스키마와 migration 이력을 먼저 확인하고, 적용된 파일을
다시 실행하거나 모든 migration을 무조건 push하지 않는다. 새 환경의 서버 로그인,
비공개 Storage 버킷과 환경 변수도 별도로 준비해야 한다.

`reference/`에는 이전 설계 자료가 있으며 자동 실행 대상이 아니다.
[서버 환경 변수와 실행 방법](../cpp/http/README.md)
