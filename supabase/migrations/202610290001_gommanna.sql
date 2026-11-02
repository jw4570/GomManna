-- 곰만나 테이블 정의: 새 Supabase 프로젝트의 SQL Editor에서 한 번 실행.
-- 비즈니스 로직·인증·위치 갱신·만료 처리는 C++ 서버에서 구현합니다.
-- 기존 동명 테이블이 있으면 별도 변경 migration이 필요합니다.
begin;

create table public.users (
  id uuid primary key default gen_random_uuid(),
  avatar text not null default
    (array['bear','rabbit','fox','cat','dog','panda','koala','tiger','penguin'])[1 + floor(random() * 9)::int],
  created_at timestamptz not null default now(),
  constraint valid_avatar check (avatar in
    ('bear','rabbit','fox','cat','dog','panda','koala','tiger','penguin'))
);

create table public.rooms (
  id uuid primary key default gen_random_uuid(),
  title text not null check (char_length(btrim(title)) between 1 and 60),
  host_id uuid not null references public.users(id) on delete cascade,
  invite_code text not null unique check (invite_code ~ '^[A-Z0-9]{8}$'),
  created_at timestamptz not null default now(),
  expires_at timestamptz not null default (now() + interval '24 hours'),
  check (expires_at > created_at)
);

create table public.room_members (
  id uuid primary key default gen_random_uuid(),
  room_id uuid not null references public.rooms(id) on delete cascade,
  user_id uuid not null unique references public.users(id) on delete cascade,
  nickname text not null check (char_length(btrim(nickname)) between 1 and 20),
  transport_mode text not null default 'walking'
    check (transport_mode in ('walking','driving','transit')),
  is_sharing boolean not null default false,
  joined_at timestamptz not null default now(),
  unique (room_id, id)
);
-- user_id UNIQUE: 방장이든 참여자든 동시에 한 모임만 참여.
create index room_members_room_idx on public.room_members(room_id);
create index rooms_host_idx on public.rooms(host_id);
create index rooms_expiry_idx on public.rooms(expires_at);

create table public.member_locations (
  member_id uuid primary key,
  room_id uuid not null,
  latitude double precision not null check (latitude between -90 and 90),
  longitude double precision not null check (longitude between -180 and 180),
  accuracy_m double precision check (accuracy_m >= 0 and accuracy_m < 'Infinity'::float8),
  recorded_at timestamptz not null default now(),
  received_at timestamptz not null default now(),
  foreign key (room_id, member_id)
    references public.room_members(room_id, id) on delete cascade
);
create index member_locations_room_idx on public.member_locations(room_id);

create table public.messages (
  id uuid primary key default gen_random_uuid(),
  room_id uuid not null references public.rooms(id) on delete cascade,
  sender_id uuid references public.users(id) on delete set null,
  body text not null check (char_length(btrim(body)) between 1 and 2000),
  created_at timestamptz not null default now()
);
create index messages_room_time_idx on public.messages(room_id, created_at, id);
create index messages_sender_idx on public.messages(sender_id);

create table public.map_marks (
  id uuid primary key default gen_random_uuid(),
  room_id uuid not null references public.rooms(id) on delete cascade,
  created_by uuid references public.users(id) on delete set null,
  name text not null check (char_length(btrim(name)) between 1 and 40),
  latitude double precision not null check (latitude between -90 and 90),
  longitude double precision not null check (longitude between -180 and 180),
  created_at timestamptz not null default now()
);
create index map_marks_room_idx on public.map_marks(room_id);
create index map_marks_creator_idx on public.map_marks(created_by);

-- C++ 서버 전용: Supabase의 클라이언트 Data API에서 직접 접근하지 못하게 합니다.
revoke all on public.users, public.rooms, public.room_members,
  public.member_locations, public.messages, public.map_marks from public, anon, authenticated;

commit;
