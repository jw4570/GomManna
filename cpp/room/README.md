# 방 서비스와 초대 코드

서영의 순수 C 초대 코드·QR 모듈 `c/`과 C++ 방 서비스
`room_service.cpp`를 서버에서 함께 사용한다. DB 접근은 공통
`IRepository`에 위임한다.

## 동작

- 새 코드: `ABCDEFGHJKLMNPQRSTUVWXYZ23456789` 중 6자리. OS의 보안 난수를 사용한다.
- 충돌: DB UNIQUE 충돌일 때만 새 코드를 생성해 최대 5회 시도한다. 다른 DB 오류는 바로 반환한다.
- 참가: 앞뒤 공백과 소문자를 정규화한 뒤 6자리 형식을 검증하고 DB에 참가자를 등록한다.
- 초대: 참가 권한과 활성 방 상태를 확인하고 **DB에 저장된 코드**로 QR payload와 SVG를 만든다.
- 종료·탈퇴: 기존 DB 구현이 방장 권한, 방 상태 및 관련 데이터 정리를 검사한다.

예시 초대 payload는 `gommanna://join?code=ABCD23`이다. 앱에서 QR 카메라
인식과 딥링크 처리를 연결하는 일은 별도다. 기존 종료된 8자리 시험 방은 기록으로만
남으며 새 방에는 6자리를 사용한다.

## 주요 파일

```text
cpp/room/
├── CMakeLists.txt              # gm_room: 서버와 연결하는 C++ 서비스
├── room_service.cpp
└── c/
    ├── include/gommanna/room_invite.h
    ├── src/                    # 코드 생성과 QR SVG
    ├── third_party/qrcodegen/  # Nayuki C QR 인코더, MIT
    ├── examples/               # 예제
    └── tests/                  # C 테스트, 선택적 독립 QR 디코딩
```

코드·QR 모듈만 독립적으로 확인할 수 있다.

```sh
cmake -S cpp/room/c -B build/room -DBUILD_TESTING=ON
cmake --build build/room --config Debug
ctest --test-dir build/room -C Debug --output-on-failure
```

전체 서버 빌드에서는 `gm_room_c`와 `gm_room`이 연결된다. 방 로직 단위 시험과
실제 Supabase를 이용한 HTTP 시험은 [서버 안내](../http/README.md)를 참고한다.
QR 디코딩 독립 시험은 Python의 `numpy`, `zxing-cpp`가 있을 때 선택적으로 실행한다.
