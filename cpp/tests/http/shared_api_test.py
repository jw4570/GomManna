"""Opt-in HTTP regression for shared map, device/profile and personal erasure."""
import argparse
import os
import subprocess
import time
import urllib.request
from real_db_api_test import (call, check, file_config, free_port, member_for,
                              photo_fixture, photo_upload, photo_download)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    args = parser.parse_args()
    config = os.environ.get('GOMMANNA_ENV_FILE')
    if os.environ.get('GOMMANNA_RUN_HTTP_DB_INTEGRATION') != '1' or not config:
        print('SKIP: requires explicit opt-in and an isolated test database')
        return 77
    check(bool(file_config(config).get('GOMMANNA_DATABASE_URL')), 'Missing test DB')
    env = os.environ.copy()
    env.pop('GOMMANNA_DATABASE_URL', None)
    env.update(GOMMANNA_HOST='127.0.0.1', GOMMANNA_PORT=str(free_port()))
    base = 'http://127.0.0.1:' + env['GOMMANNA_PORT']
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    server = subprocess.Popen([args.server], env=env, stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
    tokens = []
    try:
        deadline = time.monotonic() + 20
        while True:
            check(server.poll() is None, 'Server startup failed')
            try:
                call(opener, base, 'GET', '/health', 200)
                break
            except OSError:
                check(time.monotonic() < deadline, 'Readiness timeout')
                time.sleep(0.1)

        def api(method, path, status=200, token=None, body=None):
            return call(opener, base, method, path, status, token, body)

        def guest():
            data = api('POST', '/v1/guests', 201, body={})
            tokens.append(data['token'])
            return data['user_id'], data['token']

        host_id, host = guest()
        guest_id, participant = guest()
        _, outsider = guest()
        api('GET', '/v1/me', 401)
        check(api('GET', '/v1/me', token=host)['user']['animal_icon'] == 'bear', 'Default profile')
        api('PUT', '/v1/me/profile', token=host, body={'animal_icon': 'penguin'})
        api('PUT', '/v1/me/profile', 400, host, {'animal_icon': 'unknown'})
        req = urllib.request.Request(base + '/v1/me', headers={'Authorization': 'Bearer ' + host})
        with opener.open(req, timeout=10) as response:
            expiry = int(response.headers['X-Session-Expires-At-Ms'])
            check(expiry > int(time.time() * 1000) + 170 * 86400000, 'Sliding expiry header')
        created = api('POST', '/v1/rooms', 201, host, {'title': 'Shared API test', 'nickname': 'host'})
        room = '/v1/rooms/' + created['room']['room_id']
        hm = created['member']['member_id']
        joined = api('POST', '/v1/rooms/join', token=participant,
                     body={'invite_code': created['room']['invite_code'], 'nickname': 'guest'})
        gm = joined['member']['member_id']
        for token in (host, participant):
            api('PUT', room + '/sharing', token=token, body={'enabled': True})
        api('PUT', room + '/device-status', token=host,
            body={'battery_percent': 8, 'is_charging': False, 'sharing_epoch': 1})
        api('PUT', room + '/device-status', 400, host,
            {'battery_percent': 101, 'is_charging': False, 'sharing_epoch': 1})
        api('PUT', room + '/device-status', 409, host,
            {'battery_percent': 8, 'is_charging': False, 'sharing_epoch': 2})
        api('PUT', room + '/device-status', token=participant,
            body={'battery_percent': None, 'is_charging': None, 'sharing_epoch': 1})
        members = api('GET', room + '/members', token=participant)
        visible = member_for(members, host_id)
        check(visible['animal_icon'] == 'penguin', 'Shared profile icon')
        check(visible['device_status']['battery_percent'] == 8, 'Shared battery')
        check(visible['device_status']['is_charging'] is False, 'Charging false remains boolean')
        mark = api('POST', room + '/marks', 201, participant,
                   {'name': '약속 장소', 'latitude': 37.5, 'longitude': 127})['mark']
        mark_path = room + '/marks/' + mark['mark_id']
        api('GET', room + '/marks', 403, outsider)
        api('PUT', mark_path, 403, outsider, {'name': 'denied'})
        api('PUT', mark_path, token=host, body={'name': '새 장소'})
        check(api('GET', room + '/marks', token=participant)['marks'][0]['name'] == '새 장소', 'Shared rename')
        api('DELETE', mark_path, token=participant)
        api('POST', room + '/marks', 400, host, {'name': ' ', 'latitude': 37, 'longitude': 127})
        api('POST', room + '/marks', 400, host, {'name': 'bad', 'latitude': 91, 'longitude': 127})

        def locate(token, longitude, sequence, epoch=1):
            return api('PUT', room + '/location', token=token,
                       body={'latitude': 0, 'longitude': longitude, 'accuracy_m': 1,
                             'captured_at_ms': int(time.time() * 1000),
                             'sharing_epoch': epoch, 'sequence': sequence})

        locate(host, 0, 1)
        locate(host, 0.00001, 2)  # ~1m: update current location, no route insert
        locate(host, 0.0003, 3)  # ~33m from last stored point
        locate(participant, 0, 1)
        own = api('GET', room + '/me/route', token=host)
        check(len(own['points']) == 2, 'Route sampling through HTTP')
        api('GET', room + '/me/route?limit=0', 400, host)
        api('GET', room + '/me/route?after_id=-1', 400, host)
        api('GET', room + '/me/route?limit=1&limit=2', 400, host)
        api('GET', room + '/me/route?user_id=' + guest_id, 400, host)
        page = api('GET', room + '/members/' + hm + '/route?limit=1', token=participant)
        check(page['next_after_id'] is not None, 'Pagination cursor')
        check(len(api('GET', room + '/members/' + hm + '/route?after_id=' +
                      str(page['next_after_id']), token=participant)['points']) == 1, 'Next page')
        api('GET', room + '/members/' + hm + '/route', 403, outsider)
        api('DELETE', room + '/me/route', token=host)
        check(not api('GET', room + '/me/route', token=host)['points'], 'Own route erased')
        check(len(api('GET', room + '/me/route', token=participant)['points']) == 1, 'Other route preserved')
        locate(host, 0.0006, 4)
        note = api('PUT', room + '/note', token=host, body={'text': 'photo', 'sharing_epoch': 1})['note']
        photo_path = None
        if os.environ.get('GOMMANNA_RUN_PHOTO_INTEGRATION') == '1':
            status, result = photo_upload(opener, base, room, host, note, 1, photo_fixture())
            check(status == 200, 'Photo upload')
            photo_path = result['data']['note']['photo_path']
        api('PUT', room + '/sharing', token=host, body={'enabled': False})
        hidden = member_for(api('GET', room + '/members', token=participant), host_id)
        check(hidden['device_status'] is None and hidden['location'] is None and hidden['note'] is None,
              'Sharing OFF privacy')
        check(not api('GET', room + '/members/' + hm + '/route', token=participant)['points'], 'OFF hides route')
        api('DELETE', room + '/me/data', 202, host)
        check(api('GET', '/v1/me', token=host)['user']['user_id'] == host_id, 'Room erasure preserves account')
        check(not api('GET', room + '/me/route', token=host)['points'], 'All personal room data erased')
        if photo_path:
            check(photo_download(opener, base, photo_path, participant)[0] == 404, 'Erased photo inaccessible')
        api('POST', room + '/leave', token=participant, body={})
        check(len(api('GET', room + '/me/route', token=participant)['points']) == 1, 'Departed own data retained')
        api('GET', room + '/marks', 403, participant)
        api('DELETE', room + '/me/route', token=participant)
        api('POST', '/v1/rooms/join', token=participant,
            body={'invite_code': created['room']['invite_code'], 'nickname': 'returned'})
        api('DELETE', '/v1/me', 202, host)
        api('GET', '/v1/me', 401, host)
        check(api('GET', room, token=participant)['room']['host_id'] == guest_id, 'Account erasure transfers host')
        api('POST', room + '/end', token=participant, body={})
        api('GET', room + '/members', 410, participant)
        api('DELETE', room + '/me/data', 202, participant)  # still allowed after room end
        print('PASS: HTTP profiles/devices/marks/routes, auth, own erasure, leave/rejoin and host transfer')
        return 0
    finally:
        if server.poll() is None:
            for token in tokens:
                try:
                    call(opener, base, 'DELETE', '/v1/me', 202, token)
                except Exception:
                    pass
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=5)


if __name__ == '__main__':
    raise SystemExit(main())
