import SwiftUI

struct LegalDocumentView: View {
    let document: LegalDocument
    @State private var model = LegalDocumentViewModel()
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                if document != .licenses && LegalConfiguration.isDraft {
                    Label("보관 기간과 운영 정책을 확정하기 전의 검토용 문서예요.", systemImage: "info.circle")
                        .font(.subheadline).padding(16).frame(maxWidth: .infinity, alignment: .leading)
                        .background(AppStyle.accentSoft, in: RoundedRectangle(cornerRadius: 16))
                }
                if let error = model.error { Text(error).foregroundStyle(.red) }
                else {
                    ForEach(Array(model.text.components(separatedBy: "\n\n").enumerated()), id: \.offset) { _, paragraph in
                        if paragraph.hasPrefix("# ") { Text(String(paragraph.dropFirst(2))).font(.title2.bold()) }
                        else if paragraph.hasPrefix("## ") { Text(String(paragraph.dropFirst(3))).font(.headline).padding(.top, 8) }
                        else { Text(paragraph).font(.body).lineSpacing(5).textSelection(.enabled) }
                    }
                }
                if document == .privacy, let url = LegalConfiguration.privacyPolicyURL { Link("웹에서 보기", destination: url) }
                if document == .terms, let url = LegalConfiguration.termsURL { Link("웹에서 보기", destination: url) }
            }.frame(maxWidth: .infinity, alignment: .leading).padding(24)
        }
        .background(AppStyle.background)
        .navigationTitle(document.title).navigationBarTitleDisplayMode(.inline)
        .task(id: document) { model.load(document) }
    }
}
