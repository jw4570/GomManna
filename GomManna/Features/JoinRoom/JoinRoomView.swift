import SwiftUI

struct JoinRoomView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var model: JoinRoomViewModel
    let onJoin: (Room) -> Void
    init(session: AppSession? = nil, onJoin: @escaping (Room) -> Void) {
        _model = State(initialValue: JoinRoomViewModel(session: session ?? .shared))
        self.onJoin = onJoin
    }
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    FormHeading(title: "함께할 모임을 찾아요", subtitle: "초대받은 코드와 내 이름으로 참여하세요.")
                    Picker("참여 방법", selection: $model.method) {
                        ForEach(JoinRoomViewModel.Method.allCases, id: \.self) { Text($0.rawValue).tag($0) }
                    }.pickerStyle(.segmented)
                    if model.method == .qr {
                        QRScannerView(onScan: model.scanned)
                            .frame(height: 300).clipShape(RoundedRectangle(cornerRadius: 24))
                    }
                    InputField(title: "초대 코드", placeholder: "6자리 코드", text: $model.code)
                        .textInputAutocapitalization(.characters)
                    InputField(title: "내 이름", placeholder: "멤버들에게 보일 이름", text: $model.name)
                    if let error = model.error { Text(error).font(.subheadline).foregroundStyle(.red) }
                }.padding(24)
            }
            .scrollDismissesKeyboard(.interactively)
            .safeAreaInset(edge: .bottom) {
                PrimaryButton(title: model.isLoading ? "참여하고 있어요…" : "모임 입장", disabled: !model.canEnter) {
                    Task { if let room = await model.join() { onJoin(room) } }
                }.padding(24).background(AppStyle.background)
            }
            .background(AppStyle.background)
            .navigationTitle("모임 참여").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .cancellationAction) { Button("닫기") { dismiss() }.disabled(model.isLoading) } }
            .interactiveDismissDisabled(model.isLoading)
        }
    }
}
#Preview { JoinRoomView(session: .preview()) { _ in } }
