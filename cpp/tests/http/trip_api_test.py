"""Trip endpoints against an opt-in, isolated PostgreSQL database. No production writes."""
import argparse
import os
import subprocess
import time
import urllib.request
from real_db_api_test import call, check, file_config, free_port


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    args = parser.parse_args()
    config = os.environ.get('GOMMANNA_ENV_FILE')
    if os.environ.get('GOMMANNA_RUN_HTTP_DB_INTEGRATION') != '1' or not config:
        return 77
    check(bool(file_config(config).get('GOMMANNA_DATABASE_URL')), 'Missing isolated database')
    env = os.environ.copy()
    env.pop('GOMMANNA_DATABASE_URL', None)
    env.update(GOMMANNA_HOST='127.0.0.1', GOMMANNA_PORT=str(free_port()))
    base = 'http://127.0.0.1:' + env['GOMMANNA_PORT']
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    server = subprocess.Popen([args.server], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    tokens = []
    def api(method, path, status=200, token=None, body=None):
        return call(opener, base, method, path, status, token, body)
    def guest():
        result = api('POST', '/v1/guests', 201, body={})
        tokens.append(result['token'])
        return result['token']
    try:
        deadline = time.monotonic() + 20
        while True:
            check(server.poll() is None, 'Server exited')
            try:
                api('GET', '/health')
                break
            except OSError:
                check(time.monotonic() < deadline, 'Server timeout')
                time.sleep(.1)
        host, other, outsider = guest(), guest(), guest()
        api('GET', '/v1/me/trips', 401)
        room = api('POST', '/v1/rooms', 201, host, {'title': 'Trip HTTP', 'nickname': 'host'})
        rid = room['room']['room_id']
        r = '/v1/rooms/' + rid
        t = '/v1/trips/' + rid
        joined = api('POST', '/v1/rooms/join', token=other,
                     body={'invite_code': room['room']['invite_code'], 'nickname': 'other'})
        mid = joined['member']['member_id']
        check(not api('GET', r + '/trip-sharing', token=other)['history_shared'], 'Default OFF')
        api('PUT', r + '/sharing', token=other, body={'enabled': True})
        now = int(time.time() * 1000)
        api('PUT', r + '/location', token=other, body={
            'latitude': 37.5, 'longitude': 127.0, 'accuracy_m': 5,
            'captured_at_ms': now, 'sharing_epoch': 1, 'sequence': 1})
        api('PUT', r + '/note', token=other, body={'text': 'first', 'sharing_epoch': 1})
        time.sleep(.02)
        api('PUT', r + '/note', token=other, body={'text': 'second', 'sharing_epoch': 1})
        api('GET', t, 409, host)
        api('POST', r + '/end', token=host, body={})
        trips = api('GET', '/v1/me/trips', token=host)['trips']
        check(any(x['room_id'] == rid for x in trips), 'Discover ended trip')
        check('invite_code' not in trips[0], 'No historical invite code leak')
        api('GET', t, 403, outsider)
        api('GET', t + '/members/' + mid + '/route', 403, host)
        check(not api('GET', t + '/notes', token=host)['notes'], 'Opt-out content hidden')
        api('PUT', r + '/trip-sharing', 400, other, {'enabled': 'true'})
        api('PUT', r + '/trip-sharing', token=other, body={'enabled': True})
        check(api('GET', r + '/trip-sharing', token=other)['history_shared'], 'Consent persists')
        points = api('GET', t + '/members/' + mid + '/route', token=host)['points']
        check(len(points) == 1 and points[0]['sharing_epoch'] == 1, 'All-epoch archive route')
        first = api('GET', t + '/notes?limit=1', token=host)
        check(first['next_after_id'] is not None, 'History pagination')
        second = api('GET', t + '/notes?limit=1&after_id=' + str(first['next_after_id']), token=host)
        check(second['notes'][0]['text'] == 'second', 'Both revisions retained')
        api('GET', t + '/notes?limit=201', 400, host)
        api('GET', '/v1/me/trips?limit=101', 400, host)
        api('DELETE', t + '/notes/' + str(first['notes'][0]['record_id']), 404, host)
        api('DELETE', t + '/notes/' + str(first['notes'][0]['record_id']), 202, other)
        api('PUT', r + '/trip-sharing', token=other, body={'enabled': False})
        check(not api('GET', t + '/notes', token=host)['notes'], 'Revocation hides history')
        api('GET', t + '/members/' + mid + '/route', 403, host)
        print('PASS: trip HTTP access, discovery, opt-in, revisions, paging, deletion and revocation')
    finally:
        for token in tokens:
            try:
                api('DELETE', '/v1/me', 202, token)
            except Exception:
                pass
        server.terminate()
        server.wait(timeout=10)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
