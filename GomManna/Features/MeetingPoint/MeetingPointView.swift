import SwiftUI

struct MeetingPointView: View {
    let roomModel: RoomViewModel
    private var model: MeetingPointViewModel { roomModel.meetingPoint }
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Toggle("중간 지점 계산", isOn: Binding(
                        get: { model.isEnabled },
                        set: { roomModel.setMeetingPointEnabled($0) }
                    ))
                } footer: {
                    Text("켜 두면 멤버 위치가 갱신될 때마다 임시 지점이 이동해요. 마킹 모드에서 임시 지점을 누르면 고정 마커로 저장할 수 있어요.")
                }
                Section {
                    ForEach(roomModel.members) { member in
                        let available = MeetingPointViewModel.coordinate(for: member) != nil
                        Button { model.toggle(member) } label: {
                            HStack(spacing: 12) {
                                MemberAvatar(character: member.character, size: 36)
                                VStack(alignment: .leading, spacing: 3) {
                                    Text(member.name).foregroundStyle(.primary)
                                    if !available {
                                        Text(member.isSharing ? "위치 확인 중" : "위치 공유 꺼짐")
                                            .font(.caption).foregroundStyle(.secondary)
                                    } else if member.locationStatus == .stale || member.locationStatus == .delayed {
                                        Text("이전 위치 기준").font(.caption).foregroundStyle(.secondary)
                                    }
                                }
                                Spacer()
                                Image(systemName: model.selectedIDs.contains(member.id) ? "checkmark.circle.fill" : "circle")
                                    .foregroundStyle(available ? AppStyle.accent : .secondary)
                            }.contentShape(Rectangle())
                        }
                        .buttonStyle(.plain).disabled(!available)
                        .accessibilityAddTraits(model.selectedIDs.contains(member.id) ? .isSelected : [])
                    }
                } header: {
                    Text("참여자 · \(model.selectedIDs.count)명 선택")
                } footer: {
                    Text("위치가 확인된 멤버는 기본으로 선택돼요. 마지막으로 공유된 위치를 기준으로 계산해요.")
                }
                Section("계산 방식") {
                    ForEach(MeetingPointViewModel.Method.allCases) { method in
                        Button { model.method = method; model.error = nil } label: {
                            HStack {
                                VStack(alignment: .leading, spacing: 5) {
                                    Text(method.title).foregroundStyle(.primary)
                                    Text(method.detail).font(.caption).foregroundStyle(.secondary)
                                }
                                Spacer()
                                if model.method == method { Image(systemName: "checkmark").foregroundStyle(AppStyle.accent) }
                            }.contentShape(Rectangle())
                        }.buttonStyle(.plain)
                    }
                }
                Section {
                    Text("같은 도시 안의 직선거리 기준이에요. 도로·대중교통 이동 시간은 반영하지 않아요.")
                        .font(.caption).foregroundStyle(.secondary)
                    if let error = model.error { Text(error).font(.caption).foregroundStyle(.red) }
                }
            }
            .scrollContentBackground(.hidden)
            .background(AppStyle.background)
            .tint(AppStyle.accent)
            .navigationTitle("중간 지점 찾기")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("완료") { dismiss() } }
            }
            .onChange(of: roomModel.members) { _, members in model.refresh(members: members) }
            .onChange(of: model.selectedIDs) { _, _ in model.refresh(members: roomModel.members) }
            .onChange(of: model.method) { _, _ in model.refresh(members: roomModel.members) }
        }
    }
}
