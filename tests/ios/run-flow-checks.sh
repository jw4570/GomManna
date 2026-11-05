#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
binary=$(mktemp -t gommanna-flow)
trap 'rm -f "$binary"' EXIT
swiftc -default-isolation MainActor \
  GomManna/Services/DTO/*.swift GomManna/Services/API/*.swift \
  GomManna/Services/Location/*.swift \
  GomManna/Services/Auth/TokenStore.swift GomManna/Services/Mock/*.swift \
  GomManna/Repositories/*.swift GomManna/Models/Room/*.swift \
  GomManna/Models/Profile/AnimalCharacter.swift GomManna/Models/Note/*.swift \
  GomManna/Models/Location/*.swift GomManna/App/Configuration/*.swift \
  GomManna/App/AppSession.swift \
  GomManna/Features/CreateRoom/CreateRoomViewModel.swift \
  GomManna/Features/JoinRoom/JoinRoomViewModel.swift \
  GomManna/Features/Home/HomeViewModel.swift \
  GomManna/Features/Settings/SettingsViewModel.swift \
  GomManna/Features/RoomSettings/RoomSettingsViewModel.swift \
  tests/ios/*.swift -o "$binary"
"$binary" "$@"
