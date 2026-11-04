"""Opt-in: HTTP server -> real test database -> HTTP JSON. No real location data."""
import argparse
import json
import math
import os
from pathlib import Path
import re
import secrets
import socket
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.request
import zlib


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def file_config(path):
    values = {}
    for line in Path(path).read_text(encoding="utf-8-sig").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, sep, value = line.partition("=")
        if not sep:
            raise RuntimeError("Invalid environment file")
        value = value.strip()
        if len(value) >= 2 and value[0] in "\"'" and value[-1] == value[0]:
            value = value[1:-1]
        values[key.strip()] = value
    return values


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def request(opener, base, method, path, token=None, body=None):
    headers = {}
    data = None
    if token:
        headers["Authorization"] = "Bearer " + token
    if body is not None:
        headers["Content-Type"] = "application/json"
        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(base + path, data=data, headers=headers, method=method)
    try:
        with opener.open(req, timeout=25) as response:
            status, payload = response.status, response.read()
    except urllib.error.HTTPError as error:
        status, payload = error.code, error.read()
    try:
        return status, json.loads(payload.decode("utf-8"))
    except (ValueError, UnicodeError):
        raise AssertionError("API response is not valid JSON") from None


def call(opener, base, method, path, expected, token=None, body=None):
    status, result = request(opener, base, method, path, token, body)
    error_code = result.get("error", {}).get("code") if isinstance(result, dict) else None
    check(status == expected, f"Unexpected HTTP status for {method} {path.rsplit('/', 1)[-1]}: "
          f"expected {expected}, got {status} ({error_code})")
    check(isinstance(result, dict) and result.get("ok") == (expected < 400), "Invalid API result envelope")
    return result.get("data") if expected < 400 else result.get("error")


def member_for(data, user_id):
    found = [member for member in data["members"] if member["user_id"] == user_id]
    check(len(found) == 1, "Expected participant missing")
    return found[0]


def photo_fixture():
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0)) +
            chunk(b"tEXt", b"Comment\0SYNTHETIC-GPS-METADATA") +
            chunk(b"IDAT", zlib.compress(b"\x00\xff\x00\x00\xff")) +
            chunk(b"IEND", b""))


def photo_upload(opener, base, path, token, note, epoch, data):
    boundary = "gmtest" + secrets.token_hex(12)
    body = bytearray()
    for name, value in (("note_id", note["note_id"]), ("written_at_ms", str(note["written_at_ms"])),
                        ("sharing_epoch", str(epoch))):
        body.extend(("--" + boundary + "\r\n").encode())
        body.extend(('Content-Disposition: form-data; name="' + name + '"\r\n\r\n').encode())
        body.extend(value.encode())
        body.extend(b"\r\n")
    body.extend(("--" + boundary + '\r\nContent-Disposition: form-data; name="file"; filename="test.png"\r\n'
                 "Content-Type: image/png\r\n\r\n").encode())
    body.extend(data)
    body.extend(("\r\n--" + boundary + "--\r\n").encode())
    req = urllib.request.Request(base + path + "/note/photo", data=bytes(body), method="PUT",
        headers={"Authorization": "Bearer " + token, "Content-Type": "multipart/form-data; boundary=" + boundary})
    try:
        with opener.open(req, timeout=60) as response:
            return response.status, json.loads(response.read().decode())
    except urllib.error.HTTPError as error:
        return error.code, json.loads(error.read().decode())


def photo_download(opener, base, path, token):
    req = urllib.request.Request(base + path, method="GET", headers={"Authorization": "Bearer " + token})
    try:
        with opener.open(req, timeout=45) as response:
            return response.status, response.headers.get("Content-Type"), response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.headers.get("Content-Type"), error.read()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--geo", required=True)
    args = parser.parse_args()
    env_path = os.environ.get("GOMMANNA_ENV_FILE")
    if os.environ.get("GOMMANNA_RUN_HTTP_DB_INTEGRATION") != "1" or not env_path:
        print("SKIP: opt in and set GOMMANNA_ENV_FILE to an isolated test database configuration")
        return 77
    values = file_config(env_path)
    url = values.get("GOMMANNA_DATABASE_URL", "")
    check(bool(url), "Test DB configuration is missing")
    photo_test = os.environ.get("GOMMANNA_RUN_PHOTO_INTEGRATION") == "1"
    if photo_test:
        check(bool(values.get("GOMMANNA_STORAGE_URL")) and bool(values.get("GOMMANNA_STORAGE_SECRET")),
              "Photo integration requested without private storage configuration")
    fixture_env = os.environ.copy()
    fixture_env["GOMMANNA_DATABASE_URL"] = url
    server_env = os.environ.copy()
    server_env.pop("GOMMANNA_DATABASE_URL", None)  # verify that the server reads the file
    port = free_port()
    server_env["GOMMANNA_HOST"] = "127.0.0.1"
    server_env["GOMMANNA_PORT"] = str(port)
    base = f"http://127.0.0.1:{port}"
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    server = subprocess.Popen([args.server], env=server_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    room_id = None
    host_id = None
    try:
        deadline = time.monotonic() + 30
        while True:
            if server.poll() is not None:
                raise RuntimeError("Server failed to start with the test DB configuration")
            try:
                call(opener, base, "GET", "/health", 200)
                break
            except (OSError, urllib.error.URLError):
                if time.monotonic() >= deadline:
                    raise RuntimeError("Server did not become ready") from None
                time.sleep(0.2)

        def guest():
            data = call(opener, base, "POST", "/v1/guests", 201, body={})
            check(len(data.get("token", "")) == 64, "Guest token was not returned")
            return data["user_id"], data["token"]

        host_id, host_token = guest()
        guest_id, guest_token = guest()
        _, outsider_token = guest()
        created = call(opener, base, "POST", "/v1/rooms", 201, host_token,
                       {"title": "HTTP integration test", "nickname": "테스트 이안"})
        room_id = created["room"]["room_id"]
        check(re.fullmatch(r"[0-9a-f-]{36}", room_id) is not None, "Invalid created room ID")
        invite_code = created["room"]["invite_code"]
        check(re.fullmatch(r"[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{6}", invite_code) is not None,
              "Room did not receive a valid six-character invite code")
        check(created["member"]["user_id"] == host_id and not created["member"]["is_sharing"],
              "Host membership was not created with sharing off")
        path = "/v1/rooms/" + room_id
        invite = call(opener, base, "GET", path + "/invite", 200, host_token)
        check(invite["invite_code"] == invite_code and
              invite["qr_payload"] == "gommanna://join?code=" + invite_code and
              invite["qr_svg"].startswith("<svg "), "Invite code, payload or QR SVG mismatch")
        call(opener, base, "GET", path + "/invite", 403, outsider_token)
        error = call(opener, base, "POST", "/v1/rooms/join", 400, guest_token,
                     {"invite_code": "ABCD2345", "nickname": "참가자"})
        check(error["code"] == "INVALID_ARGUMENT", "Old eight-character code was accepted")
        joined = call(opener, base, "POST", "/v1/rooms/join", 200, guest_token,
                      {"invite_code": "  " + invite_code.lower() + "  ", "nickname": "테스트 주원"})
        check(joined["room"]["room_id"] == room_id and joined["member"]["user_id"] == guest_id,
              "Join did not create the expected membership")
        rejoined = call(opener, base, "POST", "/v1/rooms/join", 200, guest_token,
                        {"invite_code": invite_code, "nickname": "변경되지 않음"})
        check(rejoined["member"]["member_id"] == joined["member"]["member_id"] and
              rejoined["member"]["nickname"] == joined["member"]["nickname"],
              "Repeated join was not idempotent")
        call(opener, base, "POST", path + "/leave", 403, host_token, {})
        call(opener, base, "POST", path + "/end", 403, guest_token, {})
        extras = [guest() for _ in range(6)]
        for index, (_, token) in enumerate(extras):
            call(opener, base, "POST", "/v1/rooms/join", 200, token,
                 {"invite_code": invite_code, "nickname": "테스트" + str(index)})
        check(len(call(opener, base, "GET", path + "/members", 200, host_token)["members"]) == 8,
              "Expected eight room members")
        error = call(opener, base, "POST", "/v1/rooms/join", 409, outsider_token,
                     {"invite_code": invite_code, "nickname": "초과 참가자"})
        check(error["code"] == "CONFLICT", "Room capacity was not enforced")
        call(opener, base, "POST", path + "/leave", 200, extras[0][1], {})
        call(opener, base, "GET", path + "/members", 403, extras[0][1])
        check(call(opener, base, "GET", path, 200, host_token)["room"]["room_id"] == room_id,
              "Room response mismatch")
        call(opener, base, "GET", path + "/members", 401)
        call(opener, base, "GET", path + "/members", 403, outsider_token)
        before = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(before["location"] is None and before["note"] is None, "Initial sharing must be hidden")
        guest_epoch = call(opener, base, "PUT", path + "/sharing", 200, guest_token, {"enabled": True})["member"]["sharing_epoch"]
        guest_location = {"latitude": 34.67, "longitude": 135.51, "accuracy_m": None,
                          "captured_at_ms": int(time.time() * 1000), "sharing_epoch": guest_epoch, "sequence": 1}
        call(opener, base, "PUT", path + "/location", 200, guest_token, guest_location)
        member = call(opener, base, "PUT", path + "/sharing", 200, host_token, {"enabled": True})["member"]
        epoch = member["sharing_epoch"]
        check(epoch == 1, "Sharing did not start a new epoch")
        location = {"latitude": 34.66, "longitude": 135.50, "accuracy_m": None,
                    "captured_at_ms": int(time.time() * 1000), "sharing_epoch": epoch, "sequence": 1}
        call(opener, base, "PUT", path + "/location", 200, host_token, location)
        error = call(opener, base, "PUT", path + "/location", 409, host_token, location)
        check(error["code"] == "STALE_UPDATE", "Duplicate location sequence was accepted")
        note = call(opener, base, "PUT", path + "/note", 200, host_token,
                    {"text": "3층 에스컬레이터 옆", "sharing_epoch": epoch})["note"]
        photo_path = None
        if photo_test:
            fixture_data = photo_fixture()
            check(b"SYNTHETIC-GPS-METADATA" in fixture_data, "Photo test fixture has no metadata")
            status, result = photo_upload(opener, base, path, host_token, note, epoch, fixture_data)
            check(status == 200 and result.get("ok"), "Photo upload to real private storage failed")
            photo = result["data"]["note"]
            check(photo["photo_available"] and "photo_key" not in photo, "Photo metadata leaked a storage key")
            photo_path = photo["photo_path"]
            check(photo_path == path + "/members/" + created["member"]["member_id"] + "/photo",
                  "Photo access path did not match the member")
            status, content_type, data = photo_download(opener, base, photo_path, guest_token)
            check(status == 200 and content_type.startswith("image/png") and data.startswith(b"\x89PNG"),
                  "Member could not download the photo")
            check(b"SYNTHETIC-GPS-METADATA" not in data, "Photo metadata was not removed")
            check(photo_download(opener, base, photo_path, outsider_token)[0] == 403,
                  "An outsider could access the photo")
            status, _ = photo_upload(opener, base, path, host_token, note, epoch, b"invalid image")
            check(status == 400, "Invalid photo content was accepted")
            status, _ = photo_upload(opener, base, path, host_token, note, epoch, b"x" * (5 * 1024 * 1024 + 1))
            check(status == 413, "Oversized photo was accepted")
        location.update(latitude=34.661, captured_at_ms=int(time.time() * 1000), sequence=2)
        call(opener, base, "PUT", path + "/location", 200, host_token, location)
        visible = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(abs(visible["location"]["latitude"] - 34.661) < 1e-8, "Current location mismatch")
        check(abs(visible["note"]["origin"]["latitude"] - 34.66) < 1e-8, "Note origin changed after movement")
        guest_visible = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), guest_id)
        coords = [visible["location"]["latitude"], visible["location"]["longitude"],
                  guest_visible["location"]["latitude"], guest_visible["location"]["longitude"]]
        estimate = subprocess.run([args.geo] + [str(x) for x in coords], capture_output=True, text=True, timeout=10)
        check(estimate.returncode == 0, "Distance calculation from API coordinates failed")
        meters_text, minutes_text = estimate.stdout.split()
        meters, minutes = float(meters_text), int(minutes_text)
        check(meters > 0 and minutes == math.ceil(meters / 80), "Walk estimate does not match the agreed speed")
        call(opener, base, "PUT", path + "/sharing", 200, host_token, {"enabled": False})
        hidden = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(hidden["location"] is None and hidden["note"] is None, "OFF did not hide data")
        if photo_test:
            check(photo_download(opener, base, photo_path, guest_token)[0] == 404,
                  "Photo remained accessible with sharing off")
        error = call(opener, base, "PUT", path + "/location", 409, host_token, location)
        check(error["code"] == "SHARING_OFF", "OFF accepted a location update")
        member = call(opener, base, "PUT", path + "/sharing", 200, host_token, {"enabled": True})["member"]
        check(member["sharing_epoch"] == epoch + 1, "Sharing epoch did not advance")
        hidden = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(hidden["location"] is None and hidden["note"] is None, "Old data reappeared after ON")
        if photo_test:
            check(photo_download(opener, base, photo_path, guest_token)[0] == 404,
                  "Old photo reappeared after sharing resumed")
        error = call(opener, base, "PUT", path + "/location", 409, host_token, location)
        check(error["code"] == "STALE_UPDATE", "Old epoch was not rejected")
        location.update(sharing_epoch=epoch + 1, sequence=1, captured_at_ms=int(time.time() * 1000))
        call(opener, base, "PUT", path + "/location", 200, host_token, location)
        call(opener, base, "PUT", path + "/note", 200, host_token, {"text": "새 메모", "sharing_epoch": epoch + 1})
        call(opener, base, "DELETE", path + "/note", 200, host_token)
        visible = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(visible["note"] is None, "Deleted note remained visible")
        location.update(latitude=91, sequence=2)
        call(opener, base, "PUT", path + "/location", 400, host_token, location)
        call(opener, base, "POST", path + "/end", 200, host_token, {})
        call(opener, base, "POST", path + "/end", 200, host_token, {})
        call(opener, base, "GET", path, 410, host_token)
        error = call(opener, base, "POST", "/v1/rooms/join", 410, outsider_token,
                     {"invite_code": invite_code, "nickname": "참가자"})
        check(error["code"] == "ROOM_CLOSED", "Closed room accepted a new member")
        location["latitude"] = 34.66
        call(opener, base, "PUT", path + "/location", 410, host_token, location)
        room_id = None
        for _ in range(35):
            status, result = request(opener, base, "POST", "/v1/rooms/join", outsider_token,
                                     {"invite_code": "BAD", "nickname": "참가자"})
            if status == 429:
                check(result.get("error", {}).get("code") == "RATE_LIMITED",
                      "Rate limiting returned the wrong error")
                break
            check(status == 400, "Invalid invite unexpectedly succeeded")
        else:
            raise AssertionError("Repeated invite guesses were not rate-limited")
        print("PASS: real HTTP/Supabase guests, six-character rooms, QR, membership, locations, notes, privacy and epochs" +
              ("; private photo upload/download and privacy" if photo_test else ""))
        return 0
    finally:
        if room_id and host_id:
            subprocess.run([args.fixture, "cleanup", host_id, room_id], env=fixture_env,
                           capture_output=True, timeout=30, check=False)
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=5)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print("FAIL:", str(error), file=sys.stderr)
        sys.exit(1)
