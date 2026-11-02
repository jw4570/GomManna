import SwiftUI

struct HomeView: View {
    @State private var model: HomeViewModel
    @State private var profile = ProfileViewModel.shared

    init(room: Room? = nil) {
        _model = State(initialValue: HomeViewModel(currentRoom: room))
    }

    var body: some View {
        NavigationStack(path: $model.path) {
            GeometryReader { geometry in
                ScrollView {
                    VStack(alignment: .leading, spacing: 28) {
                        if let room = model.currentRoom {
                            VStack(alignment: .leading, spacing: 10) {
                                Text("우리 곧, 만나요").font(.system(size: 29, weight: .bold))
                                Text("지금 함께하는 사람들").font(.subheadline).foregroundStyle(.secondary)
                            }
                            currentRoomCard(room)
                            Button { model.showingInvite = true } label: {
                                HStack(spacing: 14) {
                                    Image(systemName: "qrcode").font(.title2).foregroundStyle(AppStyle.accent)
                                    VStack(alignment: .leading, spacing: 5) {
                                        Text("친구 초대하기").font(.headline)
                                        Text("QR과 코드로 함께해요").font(.caption).foregroundStyle(.secondary)
                                    }
                                    Spacer()
                                    Image(systemName: "chevron.right").font(.caption.bold()).foregroundStyle(.secondary)
                                }.padding(20).background(AppStyle.field, in: RoundedRectangle(cornerRadius: 22))
                            }.buttonStyle(.plain)
                            Spacer(minLength: 24)
                        } else {
                            Spacer(minLength: 24)
                            emptyState
                            Spacer(minLength: 24)
                        }
                    }
                    .padding(.horizontal, 24).padding(.top, 18).padding(.bottom, 24)
                    .frame(minHeight: geometry.size.height, alignment: .top)
                }
            }
            .safeAreaInset(edge: .top, spacing: 0) {
                header
                    .padding(.horizontal, 24)
                    .padding(.vertical, 12)
                    .background(AppStyle.background)
            }
            .safeAreaInset(edge: .bottom, spacing: 0) {
                VStack(spacing: 10) {
                    if model.currentRoom != nil {
                        PrimaryButton(title: "모임으로 돌아가기") { model.openCurrentRoom() }
                    } else {
                        PrimaryButton(title: "새 모임 만들기") { model.showingCreate = true }
                        PrimaryButton(title: "초대받은 모임 참여", secondary: true) { model.showingJoin = true }
                    }
                }.padding(.horizontal, 24).padding(.vertical, 14).background(AppStyle.background)
            }
            .background(AppStyle.background)
            .toolbar(.hidden, for: .navigationBar)
            .navigationDestination(for: UUID.self) { id in
                if let room = model.currentRoom, room.id == id {
                    RoomView(room: room) { model.remove(room) }
                }
            }
            .sheet(isPresented: $model.showingProfile) { ProfileView() }
            .sheet(isPresented: $model.showingCreate) { CreateRoomView(onCreate: model.enter) }
            .sheet(isPresented: $model.showingJoin) { JoinRoomView(rooms: [], onJoin: model.enter) }
            .sheet(isPresented: $model.showingInvite) {
                if let room = model.currentRoom {
                    NavigationStack {
                        InviteCodeView(room: room)
                            .toolbar {
                                ToolbarItem(placement: .confirmationAction) {
                                    Button("완료") { model.showingInvite = false }
                                }
                            }
                    }.tint(AppStyle.accent)
                }
            }
        }.tint(AppStyle.accent)
    }

    private var header: some View {
        HStack(spacing: 9) {
            BrandMark(size: 27)
            Text("곰만나").font(.title3.bold())
            Spacer()
            Button { model.showingProfile = true } label: {
                MemberAvatar(character: profile.character)
            }.buttonStyle(.plain).accessibilityLabel("내 프로필 변경")
        }
    }

    private var emptyState: some View {
        VStack(spacing: 30) {
            BrandMark(size: 128)
                .frame(width: 206, height: 206)
                .background(AppStyle.accentSoft, in: Circle())
                .accessibilityHidden(true)
            VStack(spacing: 12) {
                Text("만날 준비 됐나요?").font(.system(size: 28, weight: .bold))
                Text("모임을 만들거나 초대받은 모임에 참여해요.")
                    .font(.subheadline).foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
            }
        }.frame(maxWidth: .infinity)
    }

    private func currentRoomCard(_ room: Room) -> some View {
        VStack(alignment: .leading, spacing: 28) {
            VStack(alignment: .leading, spacing: 10) {
                Text("참여 중인 모임").font(.caption.weight(.semibold)).foregroundStyle(AppStyle.accent)
                Text(room.title).font(.title2.bold())
                Text("\(room.members.count)명 함께하는 중").font(.subheadline).foregroundStyle(.secondary)
            }
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 76), spacing: 16)], spacing: 24) {
                ForEach(room.members.prefix(6)) { member in
                    VStack(spacing: 10) {
                        MemberAvatar(character: member.id == room.myMemberID ? profile.character : member.character, size: 64)
                        Text(member.id == room.myMemberID ? "\(member.name) · 나" : member.name)
                            .font(.subheadline.weight(.medium)).lineLimit(1)
                    }.frame(maxWidth: .infinity)
                }
            }
            if room.members.count > 6 {
                Text("외 \(room.members.count - 6)명도 함께하고 있어요")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
        .padding(26).frame(maxWidth: .infinity, alignment: .leading)
        .background(AppStyle.field, in: RoundedRectangle(cornerRadius: 28))
    }
}

#Preview("모임 없음") { HomeView() }
#Preview("모임 참여 중") { HomeView(room: .sample) }
