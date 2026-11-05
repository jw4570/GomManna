import SwiftUI

struct MemberPhotoView: View {
    let roomID: UUID
    let memberID: UUID
    let session: AppSession
    @State private var model = MemberPhotoViewModel()
    private var request: MemberPhotoRequest? {
        guard let current = session.current, current.room.id == roomID,
              let member = current.room.members.first(where: { $0.id == memberID }) else { return nil }
        return MemberPhotoRequest(roomID: roomID, member: member)
    }
    var body: some View {
        Group {
            if request == nil { ContentUnavailableView("공유된 사진이 없어요", systemImage: "photo") }
            else if model.loadedRequest == request, let image = model.image { Image(uiImage: image).resizable().scaledToFit().padding() }
            else if model.request == request, let error = model.error {
                ContentUnavailableView("사진을 불러오지 못했어요", systemImage: "photo", description: Text(error))
            } else { ProgressView() }
        }
        .navigationTitle("첨부 사진")
        .task(id: request) {
            model.clear()
            guard let request else { return }
            await model.load(request, session: session)
        }
        .onDisappear { model.clear() }
    }
}
