import SwiftUI

struct InviteCodeView: View {
    let room: Room
    @State private var model = InviteCodeViewModel()
    var body: some View {
        ScrollView {
            VStack(spacing: 26) {
                FormHeading(title: "같이 가요, 우리", subtitle: "QR을 보여주거나 초대 코드를 전달하세요.")
                VStack(spacing: 24) {
                    Text(room.title).font(.headline)
                    if let image = model.qrImage(for: room.code) {
                        Image(uiImage: image).interpolation(.none).resizable().scaledToFit().frame(maxWidth: 210)
                            .padding(18).background(.white, in: RoundedRectangle(cornerRadius: 18)).accessibilityLabel("모임 초대 QR 코드")
                    }
                    Text(room.code).font(.system(size: 32, weight: .bold, design: .monospaced)).tracking(5).textSelection(.enabled)
                    Button { model.copy(room.code) } label: {
                        Label(model.copied ? "복사했어요" : "코드 복사", systemImage: model.copied ? "checkmark" : "doc.on.doc").font(.subheadline.bold())
                    }
                }.padding(28).frame(maxWidth: .infinity).background(AppStyle.field, in: RoundedRectangle(cornerRadius: 28))
                ShareLink(item: room.code) { Label("초대 코드 공유", systemImage: "square.and.arrow.up").font(.headline).frame(maxWidth: .infinity).padding(18) }
                    .buttonStyle(.bordered)
            }.padding(24)
        }.navigationTitle("초대 코드").navigationBarTitleDisplayMode(.inline)
    }
}
#Preview { NavigationStack { InviteCodeView(room: .sample) } }
