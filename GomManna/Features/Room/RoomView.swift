import SwiftUI
import MapKit

struct RoomView: View {
    let room: Room
    let onRemove: () -> Void
    let session: AppSession
    @Environment(\.scenePhase) private var scenePhase
    @State private var model: RoomViewModel
    @State private var profile = ProfileViewModel.shared
    init(room: Room, session: AppSession? = nil, model: RoomViewModel? = nil, onRemove: @escaping () -> Void) {
        self.room = room
        self.session = session ?? .shared
        self.onRemove = onRemove
        _model = State(initialValue: model ?? RoomViewModel(members: room.members, session: session ?? .shared))
    }

    private var currentRoom: Room {
        var current = room
        current.members = session.current?.room.id == room.id ? (session.current?.room.members ?? []) : []
        return current
    }

    var body: some View {
        GeometryReader { geometry in
            let bottomInset = geometry.safeAreaInsets.bottom
            let fullHeight = geometry.size.height + bottomInset
            ZStack(alignment: .bottom) {
                roomMap
                membersPanel(availableHeight: fullHeight, bottomInset: bottomInset)
            }
            .frame(width: geometry.size.width, height: fullHeight)
            .overlay(alignment: .topTrailing) {
                HStack(spacing: 10) {
                    Button { model.toggleMarking() } label: {
                        Image(systemName: model.isMarking ? "mappin.circle.fill" : "mappin.and.ellipse")
                            .font(.system(size: 20, weight: .semibold))
                            .foregroundStyle(model.isMarking ? AppStyle.onAccent : AppStyle.accent)
                            .frame(width: 48, height: 48)
                            .background(model.isMarking ? AppStyle.accent : AppStyle.background, in: Circle())
                            .shadow(color: .black.opacity(0.10), radius: 8, y: 3)
                    }
                    .accessibilityLabel(model.isMarking ? "마킹 취소" : "지도에 마킹하기")
                    .accessibilityAddTraits(model.isMarking ? .isSelected : [])
                    Button { model.showingNotes = true } label: {
                        Image(systemName: "note.text")
                            .font(.system(size: 20, weight: .semibold))
                            .foregroundStyle(AppStyle.accent)
                            .frame(width: 48, height: 48)
                            .background(AppStyle.background, in: Circle())
                            .shadow(color: .black.opacity(0.10), radius: 8, y: 3)
                    }
                    .accessibilityLabel("멤버 메모와 사진")
                }
                .buttonStyle(.plain)
                .padding(16)
            }
            .overlay(alignment: .top) {
                if let error = model.errorMessage {
                    Text(error).font(.caption).foregroundStyle(.red)
                        .padding(12).frame(maxWidth: .infinity)
                        .background(AppStyle.background, in: RoundedRectangle(cornerRadius: 16))
                        .padding(.horizontal, 16).padding(.top, 78)
                }
            }
            .coordinateSpace(name: "roomPanelContainer")
            .clipped()
        }
        .task(id: scenePhase) {
            if scenePhase == .active { await model.run(room: room) }
        }
        .onChange(of: session.current?.room.members) { _, members in
            model.updateMembers(session.current?.room.id == room.id ? (members ?? []) : [])
        }
        .navigationTitle(room.title).navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button { model.showingSettings = true } label: { Image(systemName: "gearshape") }.accessibilityLabel("방 설정")
            }
        }
        .alert("마킹 이름", isPresented: $model.showingMarkName) {
            TextField("예: 여기서 만나요", text: $model.markName)
            Button("취소", role: .cancel) { model.cancelMark() }
            Button("저장") { model.saveMark() }.disabled(!model.canSaveMark)
        } message: {
            Text("선택한 장소의 이름을 정해주세요.")
        }
        .sheet(isPresented: $model.showingNotes) { MemberNotesView(room: currentRoom, session: session) }
        .sheet(isPresented: $model.showingProfile) { ProfileView() }
        .sheet(isPresented: $model.showingSettings) { RoomSettingsView(room: currentRoom, onRemove: onRemove, session: session) }
    }
    private var roomMap: some View {
        MapReader { proxy in
            Map(position: $model.camera) {
                ForEach(currentRoom.members) { member in
                    if let coordinate = model.coordinate(for: member) {
                        Annotation(member.name, coordinate: coordinate, anchor: .center) {
                            MapMemberAnnotationView(roomID: room.id, member: member,
                                character: member.id == room.myMemberID ? profile.character : member.character,
                                model: model,
                                onNoteTap: { model.tapMapNote(member) },
                                session: session, onTap: { model.tapMapProfile(member) })
                        }.annotationTitles(.hidden)
                    }
                }
                ForEach(model.marks) { mark in
                    Marker(mark.name, coordinate: mark.coordinate).tint(AppStyle.accent)
                }
                if let pending = model.pendingMark {
                    Marker(pending.name, coordinate: pending.coordinate).tint(AppStyle.accent)
                }
            }
            .mapStyle(.standard(elevation: .flat, pointsOfInterest: .excludingAll))
            .onMapCameraChange(frequency: .continuous) { context in
                model.updateMapZoom(distance: context.camera.distance)
            }
            .background {
                MapBackgroundTapObserver(isMarking: model.isMarking) { coordinate in
                    if model.isMarking { model.placeMark(at: coordinate) }
                    else { model.clearMapSelection() }
                }
            }
            .overlay(alignment: .top) {
                if model.isMarking {
                    Text("마킹할 위치를 지도에서 눌러주세요")
                        .font(.subheadline.weight(.medium))
                        .padding(.horizontal, 16).padding(.vertical, 12)
                        .background(AppStyle.background, in: Capsule())
                        .padding(.top, 80)
                        .allowsHitTesting(false)
                }
            }
        }
    }

    private func membersPanel(availableHeight: CGFloat, bottomInset: CGFloat) -> some View {
        let collapsedHeight: CGFloat = 104 + bottomInset
        let expandedHeight = min(availableHeight, max(collapsedHeight, availableHeight * 0.55))
        let travel = expandedHeight - collapsedHeight
        let restingOffset = model.expanded ? 0 : travel
        let offset = min(travel, max(0, restingOffset + model.panelDragTranslation))

        return VStack(spacing: 0) {
            VStack(spacing: 0) {
                Capsule().fill(.tertiary).frame(width: 36, height: 5)
                    .padding(.top, 10).padding(.bottom, 16)
                Button {
                    withAnimation(.spring(response: 0.35, dampingFraction: 0.85)) {
                        model.expanded.toggle()
                    }
                } label: {
                    HStack {
                        VStack(alignment: .leading, spacing: 5) {
                            Text("함께하는 멤버").font(.headline)
                            Text("\(currentRoom.members.count)명").font(.caption).foregroundStyle(.secondary)
                        }
                        Spacer()
                    }
                    .foregroundStyle(.primary).padding(.horizontal, 24)
                    .padding(.bottom, 20).contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel(model.expanded ? "멤버 목록 접기" : "멤버 목록 펼치기")
            }
            .padding(.bottom, bottomInset)
            .frame(height: collapsedHeight)
            .frame(maxWidth: .infinity)
            .contentShape(Rectangle())
            .highPriorityGesture(
                DragGesture(minimumDistance: 8, coordinateSpace: .named("roomPanelContainer"))
                    .onChanged { value in
                        // 움직이지 않는 컨테이너 좌표에서 이동량을 읽습니다.
                        var transaction = Transaction()
                        transaction.disablesAnimations = true
                        withTransaction(transaction) {
                            model.panelDragTranslation = value.translation.height
                        }
                    }
                    .onEnded { value in
                        withAnimation(.spring(response: 0.32, dampingFraction: 0.88)) {
                            model.finishPanelDrag(
                                translation: value.translation.height,
                                predictedTranslation: value.predictedEndTranslation.height
                            )
                        }
                    }
            )

            // 패널과 목록 크기를 고정하고 전체 위치만 이동해 레이아웃 피드백을 막습니다.
            ScrollView {
                VStack(spacing: 6) {
                    Text("공유를 켜면 화면을 잠그거나 홈으로 돌아가도 위치를 공유해요.")
                        .font(.caption).foregroundStyle(.secondary)
                    Toggle("내 위치 공유", isOn: Binding(get: { model.isSharing }, set: { value in
                        Task { await model.setSharing(value, room: room) }
                    })).tint(AppStyle.accent).disabled(model.changingSharing)
                    if let message = model.locationMessage {
                        Text(message).font(.caption).foregroundStyle(.secondary)
                        Button("위치 권한 설정") { if let url = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(url) } }
                    }
                    if let errorMessage = model.errorMessage {
                        Text(errorMessage).font(.caption).foregroundStyle(.red)
                    }
                    ForEach(currentRoom.members) { member in
                        HStack(spacing: 14) {
                            if member.id == room.myMemberID {
                                Button { model.showingProfile = true } label: {
                                    MemberAvatar(character: profile.character)
                                        .overlay(alignment: .bottomTrailing) {
                                            Image(systemName: "pencil.circle.fill")
                                                .font(.system(size: 17))
                                                .foregroundStyle(AppStyle.onAccent, AppStyle.accent)
                                        }
                                }.buttonStyle(.plain).accessibilityLabel("내 프로필 변경")
                            } else {
                                Button { model.focus(member) } label: {
                                    MemberAvatar(character: member.character)
                                }
                                .buttonStyle(.plain)
                                .disabled(model.coordinate(for: member) == nil)
                                .accessibilityLabel("\(member.name) 위치와 메모 보기")
                            }
                            Button { model.focus(member) } label: {
                                HStack {
                                    VStack(alignment: .leading, spacing: 3) {
                                        Text(member.name).font(.body.weight(.medium))
                                        if let distance = model.distanceText(to: member) { Text(distance).font(.caption).foregroundStyle(.secondary) }
                                        if member.locationStatus == .stale || member.locationStatus == .delayed { Text("이전 위치").font(.caption).foregroundStyle(.secondary) }
                                    }
                                    if member.id == room.myMemberID {
                                        Text("나").font(.caption).foregroundStyle(.secondary)
                                    }
                                    if member.isHost {
                                        Text("방장").font(.caption2.bold()).padding(6)
                                            .background(AppStyle.accentSoft, in: Capsule())
                                            .foregroundStyle(AppStyle.accent)
                                    }
                                    Spacer()
                                    if model.coordinate(for: member) != nil {
                                        Image(systemName: "location").foregroundStyle(AppStyle.accent)
                                    } else {
                                        Text(member.isSharing ? "위치 확인 중" : "위치 공유 꺼짐")
                                            .font(.caption).foregroundStyle(.secondary)
                                    }
                                }.contentShape(Rectangle())
                            }.buttonStyle(.plain)
                            .disabled(model.coordinate(for: member) == nil)
                        }.padding(.vertical, 8)
                    }
                }.padding(.horizontal, 24).padding(.bottom, bottomInset + 18)
            }
            .frame(height: max(0, expandedHeight - collapsedHeight))
            .clipped()
            .allowsHitTesting(model.expanded && model.panelDragTranslation == 0)
            .accessibilityHidden(!model.expanded)
        }
        .frame(maxWidth: .infinity)
        .frame(height: expandedHeight, alignment: .top)
        .background {
            UnevenRoundedRectangle(topLeadingRadius: 28, topTrailingRadius: 28)
                .fill(.regularMaterial)
        }
        .offset(y: offset)
    }
}
#Preview { NavigationStack { RoomView(room: .sample) {} } }
