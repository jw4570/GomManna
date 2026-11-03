-- GomManna v3: private server-only schema.
-- This does not migrate or delete the earlier public prototype tables.
-- Run only against the dedicated GomManna test project after inspecting it.

create role gommanna_server nologin nosuperuser nocreatedb nocreaterole
  noinherit nobypassrls;
grant gommanna_server to postgres;

create schema gommanna;
revoke all on schema gommanna from public, anon, authenticated;
grant usage on schema gommanna to gommanna_server;

create table gommanna.users (
  id uuid primary key default gen_random_uuid(),
  created_at timestamptz not null default now()
);

create table gommanna.guest_sessions (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references gommanna.users(id) on delete cascade,
  token_sha256 text not null unique check (token_sha256 ~ '^[0-9a-f]{64}$'),
  created_at timestamptz not null,
  expires_at timestamptz not null,
  revoked_at timestamptz,
  constraint valid_session_expiry check (expires_at > created_at)
);
create index guest_sessions_user_id_idx on gommanna.guest_sessions(user_id);

create table gommanna.rooms (
  id uuid primary key default gen_random_uuid(),
  title text not null check (char_length(btrim(title)) between 1 and 60),
  host_id uuid not null references gommanna.users(id),
  invite_code text not null unique check (invite_code ~ '^[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{8}$'),
  status text not null default 'active' check (status in ('active', 'ended')),
  created_at timestamptz not null,
  ended_at timestamptz,
  constraint valid_room_status check (
    (status = 'active' and ended_at is null) or
    (status = 'ended' and ended_at is not null and ended_at >= created_at)
  )
);
create index rooms_host_id_idx on gommanna.rooms(host_id);

create table gommanna.room_members (
  id uuid primary key default gen_random_uuid(),
  room_id uuid not null references gommanna.rooms(id) on delete cascade,
  user_id uuid not null references gommanna.users(id),
  nickname text not null check (char_length(btrim(nickname)) between 1 and 20),
  is_sharing boolean not null default false,
  sharing_epoch bigint not null default 0 check (sharing_epoch >= 0),
  joined_at timestamptz not null,
  unique (room_id, user_id),
  unique (room_id, id)
);
create index room_members_user_id_idx on gommanna.room_members(user_id);

create table gommanna.member_locations (
  member_id uuid primary key,
  room_id uuid not null,
  latitude double precision not null check (latitude between -90 and 90),
  longitude double precision not null check (longitude between -180 and 180),
  accuracy_m double precision check (accuracy_m >= 0 and accuracy_m < 'Infinity'::float8),
  captured_at timestamptz not null,
  received_at timestamptz not null,
  sharing_epoch bigint not null check (sharing_epoch >= 1),
  sequence bigint not null check (sequence >= 1),
  foreign key (room_id, member_id)
    references gommanna.room_members(room_id, id) on delete cascade
);
create index member_locations_room_id_idx on gommanna.member_locations(room_id);

create table gommanna.member_notes (
  member_id uuid primary key,
  room_id uuid not null,
  id uuid not null unique default gen_random_uuid(),
  text text not null check (char_length(btrim(text)) between 1 and 200),
  written_at timestamptz not null,
  origin_latitude double precision,
  origin_longitude double precision,
  origin_captured_at timestamptz,
  sharing_epoch bigint not null check (sharing_epoch >= 1),
  photo_key text,
  photo_uploaded_at timestamptz,
  constraint valid_note_origin check (
    (origin_latitude is null and origin_longitude is null and origin_captured_at is null)
    or (origin_latitude is not null and origin_latitude between -90 and 90
        and origin_longitude is not null and origin_longitude between -180 and 180
        and origin_captured_at is not null)
  ),
  constraint valid_note_photo check (
    (photo_key is null and photo_uploaded_at is null)
    or (photo_key is not null and photo_uploaded_at is not null)
  ),
  foreign key (room_id, member_id)
    references gommanna.room_members(room_id, id) on delete cascade
);
create index member_notes_room_id_idx on gommanna.member_notes(room_id);

revoke all on all tables in schema gommanna from public, anon, authenticated;
grant select, insert, update, delete on all tables in schema gommanna to gommanna_server;

-- Only this server role can directly access the tables. Row-level checks for
-- actors, sharing, epochs and room state are applied by the C++ repository.
alter table gommanna.users enable row level security;
alter table gommanna.guest_sessions enable row level security;
alter table gommanna.rooms enable row level security;
alter table gommanna.room_members enable row level security;
alter table gommanna.member_locations enable row level security;
alter table gommanna.member_notes enable row level security;
create policy server_access on gommanna.users to gommanna_server using (true) with check (true);
create policy server_access on gommanna.guest_sessions to gommanna_server using (true) with check (true);
create policy server_access on gommanna.rooms to gommanna_server using (true) with check (true);
create policy server_access on gommanna.room_members to gommanna_server using (true) with check (true);
create policy server_access on gommanna.member_locations to gommanna_server using (true) with check (true);
create policy server_access on gommanna.member_notes to gommanna_server using (true) with check (true);
