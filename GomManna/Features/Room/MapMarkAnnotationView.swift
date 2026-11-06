import SwiftUI

struct MapMarkAnnotationView: View {
    let markID: UUID
    let model: RoomViewModel

    private var mark: MapMark? { model.displayedMarks.first { $0.id == markID } }
    private var selected: Bool { model.selectedMark == markID || model.pendingMark?.id == markID }

    var body: some View {
        if let mark {
            Button { model.tapMark(mark) } label: {
                Image(systemName: "mappin.circle.fill")
                    .font(.system(size: 36))
                    .foregroundStyle(selected ? AppStyle.accent : .secondary, AppStyle.background)
                    .shadow(color: .black.opacity(0.12), radius: 5, y: 3)
            }
            .buttonStyle(.plain)
            .frame(width: 44, height: 44)
            .overlay(alignment: .top) {
                VStack(spacing: 2) {
                    Text(mark.name).font(.caption2.weight(.semibold)).lineLimit(1)
                    if selected {
                        Text(model.distanceLabel(to: mark))
                            .font(.system(size: 10)).foregroundStyle(.secondary).lineLimit(1)
                    }
                }
                .padding(.horizontal, 8).padding(.vertical, 4)
                .background(AppStyle.background, in: RoundedRectangle(cornerRadius: 10))
                .frame(width: 160)
                .offset(y: 46)
                .allowsHitTesting(false)
            }
            .allowsHitTesting(!model.isMarking)
            .accessibilityLabel("\(mark.name), \(model.distanceLabel(to: mark))")
            .accessibilityHint(selected ? "이름 수정 또는 마킹 삭제" : "거리 보기")
        }
    }
}
