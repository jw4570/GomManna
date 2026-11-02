import SwiftUI

struct JoinRoomView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var model = JoinRoomViewModel()
    let rooms: [Room]
    let onJoin: (Room) -> Void
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    FormHeading(title: "함께할 모임을 찾아요", subtitle: "초대받은 코드나 QR로 참여하세요.")
                    Picker("참여 방법", selection: $model.method) {
                        ForEach(JoinRoomViewModel.Method.allCases, id: \.self) { Text($0.rawValue).tag($0) }
                    }.pickerStyle(.segmented)
                    if model.method == .qr {
                        VStack(spacing: 22) {
                            Image(systemName: "viewfinder").font(.system(size: 110, weight: .ultraLight)).foregroundStyle(AppStyle.accent)
                            Text("QR 코드를 프레임 안에 맞춰주세요").font(.subheadline.weight(.medium))
                            Text("카메라 연결 전 미리보기입니다.").font(.caption).foregroundStyle(.secondary)
                            Button("샘플 QR로 화면 확인") {
                                model.code = "SEOUL6"
                                model.findRoom(in: rooms)
                            }.font(.subheadline.bold())
                        }.frame(maxWidth: .infinity).padding(.vertical, 30).background(AppStyle.field, in: RoundedRectangle(cornerRadius: 24))
                    } else {
                        InputField(title: "초대 코드", placeholder: "6자리 코드", text: $model.code)
                            .textInputAutocapitalization(.characters)
                            .onChange(of: model.code) { _, _ in model.resolvedRoom = nil; model.error = nil }
                        PrimaryButton(title: "모임 확인", secondary: true, disabled: model.normalizedCode.isEmpty) { model.findRoom(in: rooms) }
                        Text("화면 확인용 코드: SEOUL6").font(.caption).foregroundStyle(.secondary)
                    }
                    if let error = model.error { Text(error).font(.subheadline).foregroundStyle(.red).accessibilityAddTraits(.updatesFrequently) }
                    if let room = model.resolvedRoom {
                        Label(room.title, systemImage: "checkmark.circle.fill").font(.headline).foregroundStyle(AppStyle.accent)
                        InputField(title: "내 이름", placeholder: "멤버들에게 보일 이름", text: $model.name)
                    }
                }.padding(24)
            }.scrollDismissesKeyboard(.interactively)
                .safeAreaInset(edge: .bottom) {
                    PrimaryButton(title: "모임 입장", disabled: !model.canEnter) {
                        if let room = model.joinedRoom() { onJoin(room) }
                    }.padding(24).background(AppStyle.background)
                }
                .background(AppStyle.background)
                .toolbarBackground(AppStyle.background, for: .navigationBar)
                .navigationTitle("모임 참여").navigationBarTitleDisplayMode(.inline)
                .toolbar { ToolbarItem(placement: .cancellationAction) { Button("닫기") { dismiss() } } }
        }
    }
}
#Preview { JoinRoomView(rooms: [.sample]) { _ in } }
