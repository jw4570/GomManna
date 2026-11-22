-- Stop older backend processes before applying. No production data is exported.
begin;
alter table gommanna.room_members add column trip_history_shared boolean not null default false;

-- Append-only note revisions, independent from the single live note per member.
-- A photo update belongs to the same note revision; replacement queues the old object.
create table gommanna.trip_notes (
  id bigint generated always as identity primary key,
  room_id uuid not null,
  member_id uuid not null,
  note_id uuid not null,
  text text not null,
  written_at timestamptz not null,
  origin_latitude double precision,
  origin_longitude double precision,
  origin_captured_at timestamptz,
  sharing_epoch bigint not null,
  photo_key text,
  photo_uploaded_at timestamptz,
  foreign key(room_id,member_id) references gommanna.room_members(room_id,id) on delete cascade,
  unique(member_id,note_id,written_at)
);
create index trip_notes_room_page_idx on gommanna.trip_notes(room_id,id);
create index trip_notes_photo_idx on gommanna.trip_notes(photo_key) where photo_key is not null;
alter table gommanna.trip_notes enable row level security;
revoke all on gommanna.trip_notes from public,anon,authenticated;
grant select,insert,update,delete on gommanna.trip_notes to gommanna_server;
grant usage,select on sequence gommanna.trip_notes_id_seq to gommanna_server;
create policy server_access on gommanna.trip_notes to gommanna_server using(true) with check(true);

create function gommanna.record_trip_note() returns trigger language plpgsql
set search_path=pg_catalog,gommanna as $$
begin
  insert into gommanna.trip_notes(room_id,member_id,note_id,text,written_at,origin_latitude,
    origin_longitude,origin_captured_at,sharing_epoch,photo_key,photo_uploaded_at)
  values(new.room_id,new.member_id,new.id,new.text,new.written_at,new.origin_latitude,
    new.origin_longitude,new.origin_captured_at,new.sharing_epoch,new.photo_key,new.photo_uploaded_at)
  on conflict(member_id,note_id,written_at) do update set
    photo_key=excluded.photo_key,photo_uploaded_at=excluded.photo_uploaded_at;
  return new;
end;
$$;
revoke all on function gommanna.record_trip_note() from public;
grant execute on function gommanna.record_trip_note() to gommanna_server;
create trigger record_trip_note after insert or update on gommanna.member_notes
for each row execute function gommanna.record_trip_note();
create trigger enqueue_replaced_trip_photo after update of photo_key or delete on gommanna.trip_notes
for each row execute function gommanna.enqueue_replaced_note_photo();
-- Only the current revision can be seeded. Overwritten/deleted history cannot be recovered.
insert into gommanna.trip_notes(room_id,member_id,note_id,text,written_at,origin_latitude,
  origin_longitude,origin_captured_at,sharing_epoch,photo_key,photo_uploaded_at)
select room_id,member_id,id,text,written_at,origin_latitude,origin_longitude,
  origin_captured_at,sharing_epoch,photo_key,photo_uploaded_at from gommanna.member_notes;
insert into gommanna.schema_versions(version) values(3);
commit;
