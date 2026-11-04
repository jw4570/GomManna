-- Private cleanup queue. Object contents remain in a private Storage bucket.
-- Reserve a random object key before uploading; remove its queue row when the
-- note attachment commits. Replaced or deleted note photos enter the queue.
begin;
create table gommanna.photo_cleanup (
  storage_key text primary key
    check (storage_key ~ '^v1/[a-f0-9]{64}[.](jpg|png)$'),
  not_before timestamptz not null,
  created_at timestamptz not null default now()
);
alter table gommanna.photo_cleanup enable row level security;
create policy photo_cleanup_server_access on gommanna.photo_cleanup
  for all to gommanna_server using (true) with check (true);
grant select, insert, delete on gommanna.photo_cleanup to gommanna_server;

create function gommanna.enqueue_replaced_note_photo() returns trigger
language plpgsql
set search_path = pg_catalog, gommanna
as $$
begin
  if tg_op = 'DELETE' then
    if old.photo_key is not null then
      insert into gommanna.photo_cleanup(storage_key, not_before)
      values (old.photo_key, now()) on conflict (storage_key) do nothing;
    end if;
    return old;
  end if;
  if old.photo_key is not null and old.photo_key is distinct from new.photo_key then
    insert into gommanna.photo_cleanup(storage_key, not_before)
    values (old.photo_key, now()) on conflict (storage_key) do nothing;
  end if;
  return new;
end;
$$;
revoke all on function gommanna.enqueue_replaced_note_photo() from public;
grant execute on function gommanna.enqueue_replaced_note_photo() to gommanna_server;
create trigger enqueue_replaced_note_photo
  after update of photo_key or delete on gommanna.member_notes
  for each row execute function gommanna.enqueue_replaced_note_photo();
commit;
