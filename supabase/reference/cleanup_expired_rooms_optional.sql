-- Supabase Dashboard에서 pg_cron 확장을 활성화한 뒤 한 번 실행합니다.
-- 5분마다 만료된 모임과 연결된 참여자·위치·대화·마킹을 삭제합니다.
select cron.schedule(
  'gommanna-cleanup-expired-rooms',
  '*/5 * * * *',
  $$delete from public.rooms where expires_at <= now();$$
);
