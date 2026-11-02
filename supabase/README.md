# 곰만나 DB — C++ 서버용

새 프로젝트의 Supabase SQL Editor에서 `migrations/202610290001_gommanna.sql`을 한 번 실행합니다.

현재 migration에는 6개 테이블, PK/FK/UNIQUE/CHECK, 기본값, 인덱스만 정의합니다.
커스텀 함수, 트리거, RLS 정책, Auth 연동, Realtime 구독, Cron 작업은 생성하지 않습니다.
Supabase Data API의 anon/authenticated 접근 권한은 차단합니다. C++ 서버에서 별도 DB 연결 계정으로 접근하세요.

- `users.id`: 독립 UUID. Supabase Auth에 종속되지 않습니다.
- `users.avatar`: 동물 9종 중 랜덤 기본값.
- `room_members.user_id` UNIQUE: 사용자당 현재 모임 하나. 나가면 참여 행을 삭제합니다.
- `member_locations.member_id` PK: 멤버당 최신 위치 하나. C++ 서버에서 UPSERT합니다.
- 모임 삭제 시 참여자·위치·대화·마킹은 FK CASCADE로 함께 삭제됩니다.
- 초대 코드는 8자리입니다. 코드 생성과 중복 재시도는 C++ 서버에서 처리합니다.
- 기본 모임 유효기간은 24시간입니다. `expires_at`은 자동 삭제 기능이 아니며, 서버가 만료 확인·정리 작업을 수행해야 합니다.
- 방장 확인, 프로필 본인 확인, 모임 참여 권한, 위치 공유 여부 확인은 C++ 서버에서 처리합니다.
- 모임 생성과 방장 참여 등록은 하나의 트랜잭션으로 처리하세요.

`reference/`는 이전 Supabase RPC 설계의 참고 자료로, 현재 migration과 함께 실행하지 않습니다.
이전 SQL을 이미 원격 DB에 실행했다면 현재 파일을 다시 실행하지 말고, 기존 스키마를 확인한 뒤 변경 migration을 적용해야 합니다.
