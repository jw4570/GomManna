#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
scratch=$(mktemp -d -t gommanna-flow)
binary="$scratch/checks"
trap 'rm -rf "$scratch"' EXIT
clang -I GomManna/Services/CRules/Include -c GomManna/Services/CRules/Sources/distance.c -o "$scratch/distance.o"
clang -I GomManna/Services/CRules/Include -c GomManna/Services/CRules/Sources/meeting_point.c -o "$scratch/meeting_point.o"
swiftc -default-isolation MainActor \
  -import-objc-header GomManna/Services/CRules/Include/GomManna-Bridging-Header.h \
  "$scratch/distance.o" "$scratch/meeting_point.o" \
  GomManna/Services/DTO/*.swift GomManna/Services/API/*.swift \
  GomManna/Services/Location/*.swift \
  GomManna/Services/Auth/TokenStore.swift GomManna/Services/Mock/*.swift \
  GomManna/Repositories/*.swift GomManna/Models/Room/*.swift \
  GomManna/Models/Profile/AnimalCharacter.swift GomManna/Models/Note/*.swift \
  GomManna/Models/Location/*.swift GomManna/App/Configuration/*.swift \
  GomManna/Models/Map/*.swift \
  GomManna/Models/Intro/*.swift GomManna/Features/Intro/IntroViewModel.swift \
  GomManna/Services/Functions/DistanceService.swift GomManna/Services/Functions/MeetPointService.swift \
  GomManna/App/AppSession.swift \
  GomManna/Features/CreateRoom/CreateRoomViewModel.swift \
  GomManna/Features/JoinRoom/JoinRoomViewModel.swift \
  GomManna/Features/Home/HomeViewModel.swift \
  GomManna/Features/Room/RoomViewModel.swift \
  GomManna/Features/MeetingPoint/MeetingPointViewModel.swift \
  GomManna/Features/Settings/SettingsViewModel.swift \
  GomManna/Features/RoomSettings/RoomSettingsViewModel.swift \
  tests/ios/*.swift -o "$binary"
"$binary" "$@"
