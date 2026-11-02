-- 참고용 이전 버전입니다. 현재 migration과 함께 실행하지 마세요.
-- 곰만나: 새 Supabase 프로젝트에서 SQL Editor로 한 번 실행합니다.
-- 전제: Supabase Auth 사용(익명 로그인 가능). 앱에서는 service_role 키를 사용하지 않습니다.
-- 모임은 기본 24시간 유효하며, 사용자당 현재 모임 하나만 허용합니다.
-- 기존 동명 테이블이 있으면 이 스크립트 대신 별도 변경 migration이 필요합니다.
begin;

create schema if not exists app_private;
revoke all on schema app_private from public, anon;
grant usage on schema app_private to authenticated;

create table public.users (
  id uuid primary key references auth.users(id) on delete cascade,
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

-- Auth 사용자 생성 시 랜덤 동물 프로필을 생성합니다.
create function app_private.handle_new_user()
returns trigger language plpgsql security definer set search_path = '' as $$
begin
  insert into public.users(id) values (new.id);
  return new;
end;
$$;
revoke all on function app_private.handle_new_user() from public, anon, authenticated;
create trigger gommanna_auth_user_created after insert on auth.users
  for each row execute function app_private.handle_new_user();
-- SQL 실행 전 생성된 Auth 사용자의 프로필도 추가합니다.
insert into public.users(id) select id from auth.users on conflict (id) do nothing;

-- 본인의 유효한 모임 ID만 반환합니다. RLS 정책 재귀를 방지하는 내부 함수입니다.
create function app_private.current_room_id()
returns uuid language sql stable security definer set search_path = '' as $$
  select m.room_id from public.room_members m
  join public.rooms r on r.id = m.room_id
  where m.user_id = (select auth.uid()) and r.expires_at > now()
$$;
revoke all on function app_private.current_room_id() from public, anon;
grant execute on function app_private.current_room_id() to authenticated;

alter table public.users enable row level security;
alter table public.rooms enable row level security;
alter table public.room_members enable row level security;
alter table public.member_locations enable row level security;
alter table public.messages enable row level security;
alter table public.map_marks enable row level security;

-- 필요한 작업 권한만 명시적으로 부여합니다.
revoke all on public.users, public.rooms, public.room_members,
  public.member_locations, public.messages, public.map_marks from anon, authenticated;
grant select on public.users, public.rooms, public.room_members,
  public.member_locations, public.messages, public.map_marks to authenticated;
grant update (avatar) on public.users to authenticated;
grant update (nickname, transport_mode, is_sharing) on public.room_members to authenticated;
grant insert (room_id, sender_id, body) on public.messages to authenticated;
grant insert (room_id, created_by, name, latitude, longitude) on public.map_marks to authenticated;
grant update (name) on public.map_marks to authenticated;
grant delete on public.map_marks to authenticated;

create policy users_read on public.users for select to authenticated using (
  id = (select auth.uid()) or exists (
    select 1 from public.room_members m
    where m.user_id = users.id and m.room_id = (select app_private.current_room_id())
  )
);
create policy users_edit_self on public.users for update to authenticated
  using (id = (select auth.uid())) with check (id = (select auth.uid()));
create policy rooms_read on public.rooms for select to authenticated
  using (id = (select app_private.current_room_id()));
create policy members_read on public.room_members for select to authenticated
  using (room_id = (select app_private.current_room_id()));
create policy members_edit_self on public.room_members for update to authenticated
  using (user_id = (select auth.uid()) and room_id = (select app_private.current_room_id()))
  with check (user_id = (select auth.uid()) and room_id = (select app_private.current_room_id()));
create policy locations_read on public.member_locations for select to authenticated using (
  room_id = (select app_private.current_room_id()) and exists (
    select 1 from public.room_members m
    where m.id = member_locations.member_id and m.is_sharing
  )
);
create policy messages_read on public.messages for select to authenticated
  using (room_id = (select app_private.current_room_id()));
create policy messages_send on public.messages for insert to authenticated
  with check (room_id = (select app_private.current_room_id()) and sender_id = (select auth.uid()));
create policy marks_read on public.map_marks for select to authenticated
  using (room_id = (select app_private.current_room_id()));
create policy marks_create on public.map_marks for insert to authenticated
  with check (room_id = (select app_private.current_room_id()) and created_by = (select auth.uid()));
create policy marks_rename on public.map_marks for update to authenticated
  using (room_id = (select app_private.current_room_id()) and created_by = (select auth.uid()))
  with check (room_id = (select app_private.current_room_id()) and created_by = (select auth.uid()));
create policy marks_delete on public.map_marks for delete to authenticated
  using (room_id = (select app_private.current_room_id()) and created_by = (select auth.uid()));

-- 모임 생성과 방장 참여 등록을 하나의 트랜잭션으로 처리합니다.
create function public.create_room(p_title text, p_nickname text)
returns uuid language plpgsql security definer set search_path = '' as $$
declare
  v_user uuid := auth.uid();
  v_room uuid;
begin
  if v_user is null then raise exception '인증이 필요합니다.'; end if;
  perform 1 from public.users where id = v_user for update;
  if not found then raise exception '프로필이 없습니다.'; end if;
  delete from public.rooms r where r.expires_at <= now()
    and r.id in (select room_id from public.room_members where user_id = v_user);
  if exists (select 1 from public.room_members where user_id = v_user) then
    raise exception '이미 참여 중인 모임이 있습니다.';
  end if;
  if p_title is null or char_length(btrim(p_title)) not between 1 and 60
    or p_nickname is null or char_length(btrim(p_nickname)) not between 1 and 20 then
    raise exception '모임 제목과 이름을 확인해주세요.';
  end if;
  loop
    begin
      insert into public.rooms(title, host_id, invite_code)
      values (btrim(p_title), v_user, upper(substr(replace(gen_random_uuid()::text, '-', ''), 1, 8)))
      returning id into v_room;
      exit;
    exception when unique_violation then
      -- 초대 코드가 중복되면 다시 생성합니다.
    end;
  end loop;
  insert into public.room_members(room_id, user_id, nickname)
    values (v_room, v_user, btrim(p_nickname));
  return v_room;
end;
$$;

-- 코드 입력과 QR 스캔 모두 이 RPC를 호출합니다.
create function public.join_room(p_invite_code text, p_nickname text)
returns uuid language plpgsql security definer set search_path = '' as $$
declare
  v_user uuid := auth.uid();
  v_room uuid;
  v_existing uuid;
begin
  if v_user is null then raise exception '인증이 필요합니다.'; end if;
  perform 1 from public.users where id = v_user for update;
  if not found then raise exception '프로필이 없습니다.'; end if;
  if p_nickname is null or char_length(btrim(p_nickname)) not between 1 and 20 then
    raise exception '이름을 확인해주세요.';
  end if;
  delete from public.rooms r where r.expires_at <= now()
    and r.id in (select room_id from public.room_members where user_id = v_user);
  select id into v_room from public.rooms
    where invite_code = upper(btrim(p_invite_code)) and expires_at > now() for update;
  if not found then raise exception '코드가 올바르지 않거나 종료된 모임입니다.'; end if;
  select room_id into v_existing from public.room_members where user_id = v_user;
  if v_existing = v_room then return v_room; end if;
  if v_existing is not null then raise exception '현재 모임에서 먼저 나가주세요.'; end if;
  insert into public.room_members(room_id, user_id, nickname)
    values (v_room, v_user, btrim(p_nickname));
  return v_room;
end;
$$;

create function public.leave_room()
returns void language plpgsql security definer set search_path = '' as $$
declare
  v_user uuid := auth.uid();
  v_room uuid;
  v_host uuid;
begin
  if v_user is null then raise exception '인증이 필요합니다.'; end if;
  perform 1 from public.users where id = v_user for update;
  select room_id into v_room from public.room_members where user_id = v_user;
  if v_room is null then return; end if;
  select host_id into v_host from public.rooms where id = v_room for update;
  if v_host = v_user then raise exception '방장은 모임 종료를 선택해주세요.'; end if;
  delete from public.room_members where user_id = v_user;
end;
$$;

create function public.end_room(p_room_id uuid)
returns void language plpgsql security definer set search_path = '' as $$
declare
  v_user uuid := auth.uid();
  v_host uuid;
begin
  if v_user is null then raise exception '인증이 필요합니다.'; end if;
  perform 1 from public.users where id = v_user for update;
  select host_id into v_host from public.rooms where id = p_room_id for update;
  if not found then return; end if;
  if v_host <> v_user then raise exception '방장만 모임을 종료할 수 있습니다.'; end if;
  delete from public.rooms where id = p_room_id;
end;
$$;

-- 본인의 최신 위치만 저장하며 이동 이력은 쌓지 않습니다.
create function public.update_location(
  p_latitude double precision,
  p_longitude double precision,
  p_accuracy_m double precision default null,
  p_recorded_at timestamptz default now()
)
returns void language plpgsql security definer set search_path = '' as $$
declare
  v_user uuid := auth.uid();
  v_member uuid;
  v_room uuid;
begin
  if v_user is null then raise exception '인증이 필요합니다.'; end if;
  select m.id, m.room_id into v_member, v_room from public.room_members m
    join public.rooms r on r.id = m.room_id
    where m.user_id = v_user and m.is_sharing and r.expires_at > now()
    for update of m;
  if not found then raise exception '참여 중인 모임에서 위치 공유를 켜주세요.'; end if;
  if p_recorded_at is null or p_recorded_at > now() + interval '1 minute' then
    raise exception '위치 측정 시각을 확인해주세요.';
  end if;
  insert into public.member_locations(member_id, room_id, latitude, longitude, accuracy_m, recorded_at)
  values (v_member, v_room, p_latitude, p_longitude, p_accuracy_m, p_recorded_at)
  on conflict (member_id) do update set
    latitude = excluded.latitude, longitude = excluded.longitude,
    accuracy_m = excluded.accuracy_m, recorded_at = excluded.recorded_at, received_at = now()
  where excluded.recorded_at >= public.member_locations.recorded_at;
end;
$$;

revoke all on function public.create_room(text,text), public.join_room(text,text),
  public.leave_room(), public.end_room(uuid),
  public.update_location(double precision,double precision,double precision,timestamptz)
  from public, anon, authenticated;
grant execute on function public.create_room(text,text), public.join_room(text,text),
  public.leave_room(), public.end_room(uuid),
  public.update_location(double precision,double precision,double precision,timestamptz)
  to authenticated;

-- Realtime INSERT/UPDATE 구독을 활성화합니다. 삭제·나가기·종료는 상태 재조회도 병행합니다.
do $$
declare v_table text;
begin
  if not exists (select 1 from pg_publication where pubname = 'supabase_realtime') then
    create publication supabase_realtime;
  end if;
  foreach v_table in array array['users','rooms','room_members','member_locations','messages','map_marks'] loop
    if not exists (select 1 from pg_publication_tables
      where pubname = 'supabase_realtime' and schemaname = 'public' and tablename = v_table) then
      execute format('alter publication supabase_realtime add table public.%I', v_table);
    end if;
  end loop;
end;
$$;

commit;

-- 자동 삭제를 위해 Supabase Cron에서 아래 SQL을 주기적으로 실행하세요(예: 5분마다).
-- delete from public.rooms where expires_at <= now();
-- 스케줄 미설정 상태에서도 만료된 모임은 RLS로 비공개 처리되고 참여할 수 없습니다.
