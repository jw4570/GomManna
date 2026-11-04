-- New and active rooms use six-character codes. Preserve existing ended
-- eight-character test rooms without making those old codes joinable.
begin;
alter table gommanna.rooms drop constraint rooms_invite_code_check;
alter table gommanna.rooms add constraint rooms_invite_code_check check (
  invite_code ~ '^[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{6}$'
  or (status = 'ended' and invite_code ~ '^[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{8}$')
);
commit;
