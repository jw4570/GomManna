#!/usr/bin/env python3
"""실제 HTTP + 격리 PostgreSQL 통합 테스트. 외부/원격 DB에 연결하지 않습니다."""
import argparse
import concurrent.futures
import datetime
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', default=str(ROOT / 'build/gommanna_server'))
    parser.add_argument('--pg-bin', default=os.environ.get('PG_BIN', '/opt/homebrew/opt/postgresql@17/bin'))
    args = parser.parse_args()
    pg_bin = Path(args.pg_bin)
    if not (pg_bin / 'initdb').exists():
        found = shutil.which('pg_config')
        if not found:
            raise SystemExit('PostgreSQL 개발/테스트 도구가 필요합니다. --pg-bin 경로를 지정하세요.')
        pg_bin = Path(subprocess.check_output([found, '--bindir'], text=True).strip())
    pg_port, http_port = free_port(), free_port()
    while pg_port == http_port:
        http_port = free_port()
    with tempfile.TemporaryDirectory(prefix='gommanna-integration-') as temp:
        temp = Path(temp)
        data = temp / 'pg'
        server = None
        pg_started = False
        server_log = open(temp / 'server.log', 'w+')

        def run(tool, *params, **kwargs):
            return subprocess.run([str(pg_bin / tool), *map(str, params)], check=True, capture_output=True, text=True, **kwargs)

        def sql(statement):
            return run('psql', '-h', '127.0.0.1', '-p', pg_port, '-U', 'gommanna_test', '-d', 'gommanna_test',
                       '-v', 'ON_ERROR_STOP=1', '-At', '-c', statement).stdout.strip()

        base = f'http://127.0.0.1:{http_port}'

        def request(method, path, payload=None, token=None, status=200):
            headers = {'Content-Type': 'application/json'}
            if token:
                headers['Authorization'] = 'Bearer ' + token
            data_bytes = json.dumps(payload, ensure_ascii=False).encode() if payload is not None else None
            req = urllib.request.Request(base + path, data=data_bytes, headers=headers, method=method)
            try:
                with urllib.request.urlopen(req, timeout=15) as response:
                    code, result = response.status, json.load(response)
            except urllib.error.HTTPError as error:
                code, result = error.code, json.load(error)
            assert code == status, (method, path, code, status, result)
            return result

        def guest():
            return request('POST', '/v1/auth/guest', status=201)

        try:
            run('initdb', '-D', data, '-U', 'gommanna_test', '-A', 'trust', '--no-locale', '-E', 'UTF8')
            run('pg_ctl', '-D', data, '-l', temp / 'postgres.log', '-o',
                f'-h 127.0.0.1 -p {pg_port} -k {temp}', '-w', 'start')
            pg_started = True
            run('createdb', '-h', '127.0.0.1', '-p', pg_port, '-U', 'gommanna_test', 'gommanna_test')
            sql('CREATE ROLE anon; CREATE ROLE authenticated;')
            run('psql', '-h', '127.0.0.1', '-p', pg_port, '-U', 'gommanna_test', '-d', 'gommanna_test',
                '-v', 'ON_ERROR_STOP=1', '-f', ROOT.parent / 'supabase/migrations/202610290001_gommanna.sql')
            env = {**os.environ, 'DATABASE_URL': f'postgresql://gommanna_test@127.0.0.1:{pg_port}/gommanna_test',
                   'AUTH_SECRET': secrets.token_hex(32), 'HOST': '127.0.0.1', 'PORT': str(http_port),
                   'DB_POOL_SIZE': '4', 'ROOM_TTL_HOURS': '24', 'TOKEN_TTL_HOURS': '24'}
            server = subprocess.Popen([str(Path(args.binary).resolve())], cwd=temp, env=env, stdout=server_log, stderr=server_log)
            for _ in range(100):
                try:
                    request('GET', '/ready')
                    break
                except (OSError, AssertionError):
                    if server.poll() is not None:
                        raise RuntimeError('Server exited during startup')
                    time.sleep(0.1)
            else:
                raise RuntimeError('Server not ready')
            request('GET', '/health')
            request('GET', '/v1/me', status=401)
            host, member, outsider = guest(), guest(), guest()
            ht, mt, ot = [u['access_token'] for u in (host, member, outsider)]
            request('GET', '/v1/me', token=ht[:-12]+'invalidtoken', status=401)
            renewed = request('POST', '/v1/auth/renew', token=ht)['access_token']
            assert request('GET', '/v1/me', token=renewed)['user']['id'] == host['user']['id']
            assert host['user']['created_at'].endswith('Z') and 'T' in host['user']['created_at']
            assert request('PATCH', '/v1/me', {'avatar': 'bear'}, ht)['user']['avatar'] == 'bear'
            request('PATCH', '/v1/me', {'avatar': 'bear', 'id': member['user']['id']}, ht, 400)
            request('PATCH', '/v1/me', {'avatar': 'unknown'}, ht, 400)
            assert request('GET', '/v1/rooms/current', token=ht)['room'] is None
            created = request('POST', '/v1/rooms', {'title': "한강 '산책'; --", 'nickname': '곰'}, ht, 201)['room']
            code, room_id = created['invite_code'], created['id']
            assert len(code) == 8 and created['members'][0]['is_host']
            request('POST', '/v1/rooms', {'title': '중복 방', 'nickname': '곰'}, ht, 409)
            assert sql('SELECT count(*) FROM public.rooms') == '1'
            joined = request('POST', '/v1/rooms/join', {'invite_code': code.lower(), 'nickname': '곰'}, mt)['room']
            assert len(joined['members']) == 2 and joined['my_member_id'] != created['my_member_id']
            request('POST', '/v1/rooms/join', {'invite_code': code, 'nickname': '곰'}, mt)
            assert sql('SELECT count(*) FROM public.room_members') == '2'
            request('POST', '/v1/rooms/current/messages', {'body': '외부인'}, ot, 404)
            request('POST', '/v1/rooms/current/messages', {'body': '위장', 'sender_id': host['user']['id']}, mt, 400)
            request('POST', '/v1/rooms/current/messages', {'body': '   '}, ht, 400)
            sent = request('POST', '/v1/rooms/current/messages', {'body': '여기서 만나요 🐻'}, ht, 201)['message']
            assert request('GET', '/v1/rooms/current/messages', token=mt)['messages'][0]['id'] == sent['id']
            for i in range(52):
                request('POST', '/v1/rooms/current/messages', {'body': f'메시지 {i}'}, ht, 201)
            page = request('GET', '/v1/rooms/current/messages', token=ht)
            assert len(page['messages']) == 50
            older = request('GET', '/v1/rooms/current/messages?before='+page['next_before'], token=ht)
            assert len(older['messages']) == 3
            assert not ({m['id'] for m in page['messages']} & {m['id'] for m in older['messages']})
            location = {'latitude': 37.5665, 'longitude': 126.978, 'accuracy_m': 8}
            request('PUT', '/v1/rooms/current/location', location, ht, 409)
            request('PATCH', '/v1/rooms/current/membership', {'is_sharing': True, 'transport_mode': 'transit'}, ht)
            assert request('PUT', '/v1/rooms/current/location', location, ht)['updated']
            old = (datetime.datetime.now(datetime.timezone.utc)-datetime.timedelta(minutes=1)).isoformat()
            assert not request('PUT', '/v1/rooms/current/location', {**location, 'recorded_at': old}, ht)['updated']
            members = request('GET', '/v1/rooms/current', token=mt)['room']['members']
            assert next(m for m in members if m['user_id'] == host['user']['id'])['latitude'] == 37.5665
            request('PATCH', '/v1/rooms/current/membership', {'is_sharing': False}, ht)
            assert sql('SELECT count(*) FROM public.member_locations') == '0'
            mark = request('POST', '/v1/rooms/current/marks', {'name': '만날 곳', 'latitude': 37.5, 'longitude': 127}, ht, 201)['mark']
            mark_path = '/v1/rooms/current/marks/' + mark['id']
            request('PATCH', mark_path, {'name': '다른 사람 변경'}, mt, 404)
            request('DELETE', mark_path, token=mt, status=404)
            assert request('PATCH', mark_path, {'name': '카페 앞'}, ht)['mark']['name'] == '카페 앞'
            request('POST', '/v1/rooms/current/marks', {'name': '잘못된 위치', 'latitude': 91, 'longitude': 127}, ht, 400)
            request('DELETE', '/v1/rooms/current/membership', token=ht, status=409)
            request('DELETE', '/v1/rooms/current', token=mt, status=403)
            request('DELETE', '/v1/rooms/current/membership', token=mt)
            assert request('GET', '/v1/rooms/current', token=mt)['room'] is None
            request('POST', '/v1/rooms/join', {'invite_code': code, 'nickname': '다시 참여'}, mt)
            request('PATCH', '/v1/rooms/current/membership', {'is_sharing': True}, mt)
            request('PUT', '/v1/rooms/current/location', location, mt)
            request('DELETE', '/v1/rooms/current', token=ht)
            assert request('GET', '/v1/rooms/current', token=mt)['room'] is None
            for table in ('rooms','room_members','member_locations','messages','map_marks'):
                assert sql(f'SELECT count(*) FROM public.{table}') == '0', table
            assert sql('SELECT count(*) FROM public.users') == '3'
            print('PASS: auth, ownership, single room, messages/pagination, location consent/order, marks, end/cascade')
            # 같은 사용자에게 동시에 방 만들기 요청이 들어와도 하나만 생성됩니다.
            def create_race():
                try:
                    return request('POST', '/v1/rooms', {'title': '동시 생성', 'nickname': '곰'}, ht, 201)
                except AssertionError as e:
                    assert e.args[0][2] == 409
                    return None
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                raced = list(pool.map(lambda _: create_race(), range(2)))
            assert sum(x is not None for x in raced) == 1
            assert sql('SELECT count(*) FROM public.rooms') == '1'
            # 과거에 만료된 모임은 조회 불가. 새 방 생성 시 잔여 참여 행을 정리합니다.
            sql("UPDATE public.rooms SET created_at=now()-interval '2 days',expires_at=now()-interval '1 day'")
            assert request('GET', '/v1/rooms/current', token=ht)['room'] is None
            new = request('POST', '/v1/rooms', {'title': '새 방', 'nickname': '곰'}, ht, 201)['room']
            assert sql('SELECT count(*) FROM public.rooms') == '1'
            # 방 종료와 참여 경합 이후에도 고아 참여 행이 남지 않아야 합니다.
            def join_race():
                try:
                    return request('POST', '/v1/rooms/join', {'invite_code': new['invite_code'], 'nickname': '친구'}, mt)
                except AssertionError as e:
                    assert e.args[0][2] == 404
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                a = pool.submit(join_race)
                b = pool.submit(request, 'DELETE', '/v1/rooms/current', None, ht)
                a.result(); b.result()
            assert sql('SELECT count(*) FROM public.room_members') == '0'
            print('PASS: concurrent create, expiry cleanup/re-entry, join/end race')
            assert sql("SELECT has_table_privilege('anon','public.users','SELECT')") == 'f'
            assert sql("SELECT has_table_privilege('authenticated','public.member_locations','SELECT')") == 'f'
            print('PASS: direct client Data API table privileges are blocked')
        except Exception:
            server_log.flush(); server_log.seek(0)
            print(server_log.read())
            raise
        finally:
            if server:
                server.terminate()
                try: server.wait(timeout=10)
                except subprocess.TimeoutExpired: server.kill(); server.wait()
            server_log.close()
            if pg_started:
                run('pg_ctl', '-D', data, '-m', 'immediate', '-w', 'stop')

if __name__ == '__main__':
    main()
