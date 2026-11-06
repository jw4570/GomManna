import SwiftUI

struct MeetingPointAnnotationView: View {
    let model: RoomViewModel

    var body: some View {
        Button { model.pinMeetingPoint() } label: {
            Image(systemName: "scope")
                .font(.system(size: 24, weight: .medium))
                .foregroundStyle(AppStyle.accent.opacity(0.7))
                .frame(width: 44, height: 44)
                .background(AppStyle.background.opacity(0.8), in: Circle())
                .overlay(Circle().strokeBorder(AppStyle.accent.opacity(0.6), style: StrokeStyle(lineWidth: 2, dash: [4, 3])))
        }
        .buttonStyle(.plain)
        .frame(width: 44, height: 44)
        .overlay(alignment: .top) {
            VStack(spacing: 2) {
                Text("실시간 중간 지점").font(.caption2.weight(.semibold))
                Text(model.isMarking ? "눌러서 마킹" : model.meetingPoint.method.title)
                    .font(.system(size: 10)).foregroundStyle(.secondary)
            }
            .padding(.horizontal, 8).padding(.vertical, 5)
            .background(AppStyle.background.opacity(0.9), in: RoundedRectangle(cornerRadius: 10))
            .frame(width: 160).offset(y: 50)
            .allowsHitTesting(false)
        }
        .allowsHitTesting(model.isMarking)
        .accessibilityLabel("실시간 중간 지점")
        .accessibilityHint("현재 지점에 고정 마커 만들기")
    }
}
