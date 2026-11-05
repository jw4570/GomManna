import SwiftUI
import PhotosUI

struct MemberNotesView: View {
    let room: Room
    @State private var model: MemberNotesViewModel
    @Environment(\.dismiss) private var dismiss
    @State private var selectedPhoto: PhotosPickerItem?
    @State private var loadingPhoto = false
    init(room: Room, session: AppSession) {
        self.room = room
        _model = State(initialValue: MemberNotesViewModel(session: session))
    }
    var body: some View {
        NavigationStack {
            List {
                Section("내 메모") {
                    if model.isSharing {
                        TextField("예: 3층 입구에서 기다릴게요", text: $model.noteDraft, axis: .vertical).lineLimit(2...5)
                        HStack {
                            Button(model.savingNote ? "저장 중…" : "메모 저장") { Task { await model.saveNote(room: room) } }
                                .disabled(model.savingNote || model.noteDraft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || model.me?.latitude == nil)
                            Spacer()
                            if model.me?.note != nil {
                                Button("삭제", role: .destructive) { Task { await model.deleteNote(room: room) } }.disabled(model.savingNote)
                            }
                        }
                        PhotosPicker(selection: $selectedPhoto, matching: .images) {
                            Label(loadingPhoto ? "사진 준비 중…" : "사진 첨부", systemImage: "photo")
                        }.disabled(model.me?.note == nil || model.savingNote || loadingPhoto)
                        if model.me?.latitude == nil { Text("위치가 전송되면 메모를 저장할 수 있어요.").font(.caption).foregroundStyle(.secondary) }
                        else if model.me?.note == nil { Text("메모를 저장한 뒤 사진을 첨부해 주세요.").font(.caption).foregroundStyle(.secondary) }
                    } else { Text("위치 공유를 켜면 메모와 사진을 남길 수 있어요.").foregroundStyle(.secondary) }
                    if let error = model.errorMessage { Text(error).foregroundStyle(.red).font(.caption) }
                }
                Section("멤버 메모") {
                    ForEach(model.members) { member in
                        VStack(alignment: .leading, spacing: 10) {
                            HStack { MemberAvatar(character: member.character, size: 32); Text(member.name).font(.headline) }
                            if let note = member.note {
                                Text(note.text)
                                Text(Date(timeIntervalSince1970: Double(note.writtenAtMs) / 1000), style: .time).font(.caption).foregroundStyle(.secondary)
                                if note.photoAvailable {
                                    NavigationLink("첨부 사진 보기") { MemberPhotoView(roomID: room.id, memberID: member.id, session: model.session) }
                                }
                            } else { Text(member.isSharing ? "아직 메모가 없어요" : "공유가 꺼져 있어요").font(.subheadline).foregroundStyle(.secondary) }
                        }.padding(.vertical, 8)
                    }
                }
            }
            .navigationTitle("메모와 사진").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("닫기") { dismiss() } } }
            .onAppear { model.noteDraft = model.me?.note?.text ?? "" }
            .task(id: selectedPhoto) {
                guard let item = selectedPhoto else { return }
                loadingPhoto = true
                defer { loadingPhoto = false; selectedPhoto = nil }
                do {
                    guard let data = try await item.loadTransferable(type: Data.self) else { return }
                    try Task.checkCancellation()
                    await model.uploadPhoto(data, room: room)
                } catch { if !Task.isCancelled { model.errorMessage = error.localizedDescription } }
            }
        }.tint(AppStyle.accent)
    }
}
