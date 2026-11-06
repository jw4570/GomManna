import SwiftUI

struct MapMemberAnnotationView: View {
    let roomID: UUID
    let member: RoomMember
    let character: AnimalCharacter
    // Map의 주석 콘텐츠 재생성을 기다리지 않고 이 뷰에서 상태를 직접 관찰합니다.
    let model: RoomViewModel
    let onNoteTap: () -> Void
    let session: AppSession
    let onTap: () -> Void
    @State private var photo = MemberPhotoViewModel()
    @State private var retry = 0

    private var selected: Bool { model.selectedMember == member.id }
    private var photoExpanded: Bool { model.expandedPhotoMember == member.id }
    private var textExpanded: Bool { model.expandedNoteMember == member.id }
    private var showsFullText: Bool { textExpanded || photoExpanded || model.noteZoom.isExpanded }

    private var photoRequest: MemberPhotoRequest? {
        photoExpanded ? MemberPhotoRequest(roomID: roomID, member: member) : nil
    }
    private struct LoadID: Hashable { let request: MemberPhotoRequest?; let retry: Int }

    private var memoBubble: some View {
        VStack(spacing: 0) {
            if member.isSharing, let note = member.note, note.sharingEpoch == member.sharingEpoch {
                VStack(alignment: .leading, spacing: 10) {
                    Button(action: onNoteTap) {
                        HStack(alignment: .top, spacing: 4) {
                            Text(showsFullText ? note.text : String(note.text.prefix(10)).replacingOccurrences(of: "\n", with: " ") + (note.text.count > 10 ? "…" : ""))
                                .font(showsFullText ? .subheadline : .caption)
                                .foregroundStyle(.primary)
                                .lineLimit(showsFullText ? nil : 1)
                                .fixedSize(horizontal: false, vertical: true)
                            if !showsFullText, note.photoAvailable {
                                Image(systemName: "photo").font(.caption2).foregroundStyle(.secondary)
                            }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("\(member.name)의 메모")
                    .accessibilityValue(note.text)
                    .accessibilityHint(textExpanded ? "메모 접기" : "메모 전체 보기")
                    if let request = photoRequest {
                        if photo.loadedRequest == request, let image = photo.image {
                            Image(uiImage: image).resizable().scaledToFill()
                                .frame(width: 184, height: 138).clipped()
                                .clipShape(RoundedRectangle(cornerRadius: 12))
                                .accessibilityLabel("\(member.name)의 메모 첨부 사진")
                        } else if photo.request == request, photo.error != nil {
                            VStack(spacing: 8) {
                                Text("사진을 불러오지 못했어요").font(.caption).foregroundStyle(.secondary)
                                Button("다시 시도") { retry += 1 }.font(.caption.bold())
                            }.frame(width: 184, height: 90)
                        } else {
                            ProgressView("사진 불러오는 중").font(.caption)
                                .frame(width: 184, height: 90)
                        }
                    }
                }
                .padding(showsFullText ? 12 : 8)
                .frame(width: showsFullText ? 208 : 120, alignment: .leading)
                .background(AppStyle.background, in: RoundedRectangle(cornerRadius: 16))
                .shadow(color: .black.opacity(0.12), radius: 7, y: 3)
                BubbleTail().fill(AppStyle.background).frame(width: 16, height: 8)
                    .padding(.bottom, 6)
            }
        }
        .fixedSize(horizontal: false, vertical: true)
        .transaction { transaction in
            transaction.animation = nil
            transaction.disablesAnimations = true
        }
    }

    var body: some View {
        Button(action: onTap) {
            MemberAvatar(character: character)
                .overlay(Circle().stroke(selected ? AppStyle.accent : .white, lineWidth: 3))
                .shadow(color: .black.opacity(0.12), radius: 5, y: 3)
                .overlay(alignment: .top) {
                    VStack(spacing: 2) {
                        Text(member.name)
                            .font(.caption2.weight(.semibold))
                            .foregroundStyle(selected ? AppStyle.accent : .primary)
                            .lineLimit(1)
                        if selected {
                            Text(model.distanceLabel(to: member))
                                .font(.system(size: 10)).foregroundStyle(.secondary)
                                .lineLimit(1)
                        }
                    }
                        .padding(.horizontal, 8).padding(.vertical, 4)
                        .background(AppStyle.background, in: RoundedRectangle(cornerRadius: 10))
                        .frame(width: 104)
                        .offset(y: 50)
                        .allowsHitTesting(false)
                        .accessibilityHidden(true)
                }
        }
        .buttonStyle(.plain)
        .contentShape(Circle())
        .accessibilityLabel("\(member.name) 위치\(member.note?.photoAvailable == true ? (photoExpanded ? ", 사진 접기" : ", 사진 보기") : "")")
        .accessibilityValue(selected ? model.distanceLabel(to: member) : "")
        .accessibilityAddTraits(selected ? .isSelected : [])
        // 지도 주석 크기는 프로필로 고정하고 말풍선은 측정 크기에 포함하지 않습니다.
        .frame(width: 44, height: 44)
        .overlay(alignment: .top) {
            memoBubble
                .alignmentGuide(.top) { dimensions in dimensions[.bottom] + 6 }
        }
        .allowsHitTesting(!model.isMarking)
        .zIndex(photoExpanded ? 1 : 0)
        .task(id: LoadID(request: photoRequest, retry: retry)) {
            photo.clear()
            guard let photoRequest else { return }
            await photo.load(photoRequest, session: session, maximumPixelSize: 640)
        }
        .onDisappear { photo.clear() }
    }
}

private struct BubbleTail: Shape {
    func path(in rect: CGRect) -> Path {
        Path { path in
            path.move(to: CGPoint(x: rect.minX, y: rect.minY))
            path.addLine(to: CGPoint(x: rect.midX, y: rect.maxY))
            path.addLine(to: CGPoint(x: rect.maxX, y: rect.minY))
            path.closeSubpath()
        }
    }
}
