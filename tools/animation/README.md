# 곰만나 인트로 애니메이션

`render_intro.py`는 macOS의 Apple SD Gothic Neo로 한글을 직접 렌더링합니다.
Python Pillow·NumPy와 ffmpeg가 필요하며 `python3 tools/animation/render_intro.py`로 재생성합니다.

결과는 `GomManna/Resources/Animations/`에 저장됩니다.

- `gommanna-intro.mov`: 1080×720, ProRes 4444, 30 fps, 4.2초, 무음, 알파 포함 마스터.
- `gommanna-intro.webm`: 1080×720, VP9, 30 fps, 알파 포함 경량 영상.
- `gommanna-intro.gif`: 810×540, 25 fps, 투명 배경 반복. GIF의 반투명 제한은 알파 디더링으로 보완합니다.
- `gommanna-intro-storyboard.png`: 투명 배경의 네 장면.
- `gommanna-intro-light.mov` / `gommanna-intro-dark.mov`: 외부 미리보기용 4.2초 HEVC with Alpha 투명 영상.
- `gommanna-intro-light.gif` / `gommanna-intro-dark.gif`: 테마별 미리보기.
- `gommanna-intro-*-poster.png`: 외부 미리보기용 투명 정지 이미지.

흐름은 `우리 곧 만나요` → `곧 만나` → `곰 만나` → `곰만나`입니다.
모든 글자는 같은 코코아 브라운으로 시작하며 배경은 투명합니다. `우리`와 `요`는
위치·크기를 유지하고 제자리에서 페이드아웃합니다. 모든 글자는 처음부터 끝까지
96pt로 고정됩니다. 양끝 글자가 사라진 뒤 `곧 → 곰`으로 변하고, 이후 가운데
간격만 닫힙니다. 각 전환은 대기 없이 이어집니다. 폰트 파일은 결과물에 포함하지 않습니다.
재생 속도는 기존 대비 1.2배이며 `PLAYBACK_SPEED`로 조정합니다. 최초 문구는 `INITIAL_HOLD=1.0`으로 1초 더 유지합니다.

앱에서는 `IntroAnimationView`의 투명 Canvas에서 5개의 글자 PNG 마스크를 직접 합성합니다.
글자 PNG는 위 스크립트로 재생성되며 영상과 같은 Apple SD Gothic Neo 96pt 글자입니다.
`IntroTimeline`이 원본과 동일한 타이밍과 위치를 계산하고 `AppStyle.accent`로 색칠합니다.
배경 픽셀을 그리지 않으므로 현재 앱 배경이 그대로 보이고, 별도 포스터도 겹치지 않습니다.

첫 문구를 1초 추가 유지하고 `우리·요 삭제 → 곧/곰 전환 → 간격 닫기`를 진행합니다.
첫 Canvas 프레임이 그려진 뒤 4.2초 타임라인을 시작하며 이후 투명 브랜드마크를
1초 표시하고 진입합니다. 백그라운드에서는 시간이 멈추며 동작 줄이기 설정에서는 로고만 표시합니다.
세션 준비는 별도로 진행하고 연결 중이면 로고 이후 기존 연결 안내 화면을 표시합니다.
MOV/WebM/GIF와 포스터는 외부 미리보기·편집용이며 앱의 인트로 재생에는 사용하지 않습니다.
