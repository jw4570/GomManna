"""Opt-in: HTTP server -> real test database -> HTTP JSON. No real location data."""
import argparse
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request


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
    check(status == expected, f"Unexpected HTTP status for {method} {path.rsplit('/', 1)[-1]}")
    check(isinstance(result, dict) and result.get("ok") == (expected < 400), "Invalid API result envelope")
    return result.get("data") if expected < 400 else result.get("error")


def member_for(data, user_id):
    found = [member for member in data["members"] if member["user_id"] == user_id]
    check(len(found) == 1, "Expected participant missing")
    return found[0]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--room-unavailable", action="store_true")
    args = parser.parse_args()
    env_path = os.environ.get("GOMMANNA_ENV_FILE")
    if os.environ.get("GOMMANNA_RUN_HTTP_DB_INTEGRATION") != "1" or not env_path:
        print("SKIP: opt in and set GOMMANNA_ENV_FILE to an isolated test database configuration")
        return 77
    values = file_config(env_path)
    url = values.get("GOMMANNA_DATABASE_URL", "")
    check(bool(url), "Test DB configuration is missing")
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
        if args.room_unavailable:
            error = call(opener, base, "POST", "/v1/rooms", 501, host_token,
                         {"title": "test", "nickname": "테스트"})
            check(error["code"] == "NOT_IMPLEMENTED", "Missing room service was not clearly reported")
        fixture = subprocess.run([args.fixture, "prepare", host_id, guest_id], env=fixture_env,
                                 capture_output=True, text=True, timeout=30)
        check(fixture.returncode == 0, "Could not create synthetic DB fixture")
        room_id = fixture.stdout.strip()
        check(re.fullmatch(r"[0-9a-f-]{36}", room_id) is not None, "Invalid fixture room ID")
        path = "/v1/rooms/" + room_id
        check(call(opener, base, "GET", path, 200, host_token)["room"]["room_id"] == room_id,
              "Room response mismatch")
        call(opener, base, "GET", path + "/members", 401)
        call(opener, base, "GET", path + "/members", 403, outsider_token)
        before = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(before["location"] is None and before["note"] is None, "Initial sharing must be hidden")
        member = call(opener, base, "PUT", path + "/sharing", 200, host_token, {"enabled": True})["member"]
        epoch = member["sharing_epoch"]
        check(epoch == 1, "Sharing did not start a new epoch")
        location = {"latitude": 34.66, "longitude": 135.50, "accuracy_m": None,
                    "captured_at_ms": int(time.time() * 1000), "sharing_epoch": epoch, "sequence": 1}
        call(opener, base, "PUT", path + "/location", 200, host_token, location)
        call(opener, base, "PUT", path + "/note", 200, host_token, {"text": "3층 에스컬레이터 옆", "sharing_epoch": epoch})
        location.update(latitude=34.661, captured_at_ms=int(time.time() * 1000), sequence=2)
        call(opener, base, "PUT", path + "/location", 200, host_token, location)
        visible = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(abs(visible["location"]["latitude"] - 34.661) < 1e-8, "Current location mismatch")
        check(abs(visible["note"]["origin"]["latitude"] - 34.66) < 1e-8, "Note origin changed after movement")
        call(opener, base, "PUT", path + "/sharing", 200, host_token, {"enabled": False})
        hidden = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(hidden["location"] is None and hidden["note"] is None, "OFF did not hide data")
        error = call(opener, base, "PUT", path + "/location", 409, host_token, location)
        check(error["code"] == "SHARING_OFF", "OFF accepted a location update")
        member = call(opener, base, "PUT", path + "/sharing", 200, host_token, {"enabled": True})["member"]
        check(member["sharing_epoch"] == epoch + 1, "Sharing epoch did not advance")
        hidden = member_for(call(opener, base, "GET", path + "/members", 200, guest_token), host_id)
        check(hidden["location"] is None and hidden["note"] is None, "Old data reappeared after ON")
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
        cleanup = subprocess.run([args.fixture, "cleanup", host_id, room_id], env=fixture_env,
                                 capture_output=True, timeout=30)
        check(cleanup.returncode == 0, "Could not close the synthetic room")
        call(opener, base, "GET", path, 410, host_token)
        room_id = None
        print("PASS: real HTTP/Supabase guests, room reads, locations, notes, privacy and epochs")
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
