-- Apply as the DB administrator while ALL older backend processes are stopped.
-- No service keys/passwords. No changes to the unrelated public prototype schema.
begin;
alter table gommanna.users add column animal_icon text not null default 'bear'
  check (animal_icon in ('bear','rabbit','fox','cat','dog','panda','koala','tiger','penguin'));
alter table gommanna.guest_sessions add column last_authenticated_at timestamptz;
create index guest_sessions_expiry_idx on gommanna.guest_sessions(expires_at);
-- Existing unexpired tokens receive the new lifetime; expired/revoked tokens stay invalid.
update gommanna.guest_sessions
set expires_at=greatest(expires_at, now() + interval '6 months')
where revoked_at is null and expires_at>now();

alter table gommanna.rooms
  add column all_sharing_stopped_at timestamptz,
  add column last_activity_at timestamptz not null default now(),
  add column retention_purged boolean not null default false;
-- Start legacy-room timers at migration time instead of deleting historical data immediately.
update gommanna.rooms r set all_sharing_stopped_at=now()
where r.status='active' and not exists (
  select 1 from gommanna.room_members m where m.room_id=r.id and m.is_sharing);
alter table gommanna.rooms alter column host_id drop not null;
alter table gommanna.rooms drop constraint rooms_host_id_fkey;
alter table gommanna.rooms add foreign key(host_id) references gommanna.users(id) on delete set null;

alter table gommanna.room_members
  add column left_at timestamptz,
  add column battery_percent integer check (battery_percent between 0 and 100),
  add column is_charging boolean,
  add column device_updated_at timestamptz,
  add column device_epoch bigint,
  add constraint departed_not_sharing check (left_at is null or not is_sharing);
alter table gommanna.room_members drop constraint room_members_user_id_fkey;
alter table gommanna.room_members add foreign key(user_id) references gommanna.users(id) on delete cascade;
create index room_members_active_idx on gommanna.room_members(room_id) where left_at is null;

create table gommanna.map_marks (
  id uuid primary key default gen_random_uuid(),
  room_id uuid not null references gommanna.rooms(id) on delete cascade,
  creator_id uuid not null references gommanna.users(id) on delete cascade,
  name text not null check(char_length(btrim(name)) between 1 and 60),
  latitude double precision not null check(latitude between -90 and 90),
  longitude double precision not null check(longitude between -180 and 180),
  created_at timestamptz not null,
  updated_at timestamptz not null,
  unique(room_id,id)
);
create index map_marks_room_idx on gommanna.map_marks(room_id,created_at,id);
create index map_marks_creator_idx on gommanna.map_marks(creator_id);

create table gommanna.route_points (
  id bigint generated always as identity primary key,
  member_id uuid not null,
  room_id uuid not null,
  latitude double precision not null check(latitude between -90 and 90),
  longitude double precision not null check(longitude between -180 and 180),
  accuracy_m double precision check(accuracy_m>=0 and accuracy_m<'Infinity'::float8),
  captured_at timestamptz not null,
  received_at timestamptz not null,
  sharing_epoch bigint not null check(sharing_epoch>=1),
  sequence bigint not null check(sequence>=1),
  foreign key(room_id,member_id) references gommanna.room_members(room_id,id) on delete cascade,
  unique(member_id,sharing_epoch,sequence)
);
create index route_points_member_page_idx on gommanna.route_points(member_id,id);
create index route_points_room_idx on gommanna.route_points(room_id);
alter table gommanna.map_marks enable row level security;
alter table gommanna.route_points enable row level security;
revoke all on gommanna.map_marks,gommanna.route_points from public,anon,authenticated;
grant select,insert,update,delete on gommanna.map_marks,gommanna.route_points to gommanna_server;
grant usage,select on sequence gommanna.route_points_id_seq to gommanna_server;
create policy server_access on gommanna.map_marks to gommanna_server using(true) with check(true);
create policy server_access on gommanna.route_points to gommanna_server using(true) with check(true);
-- This marker makes an old-schema deployment fail at startup instead of half working.
create table gommanna.schema_versions(version integer primary key);
insert into gommanna.schema_versions values(2);
alter table gommanna.schema_versions enable row level security;
revoke all on gommanna.schema_versions from public,anon,authenticated;
grant select on gommanna.schema_versions to gommanna_server;
create policy server_read on gommanna.schema_versions for select to gommanna_server using(true);
commit;
