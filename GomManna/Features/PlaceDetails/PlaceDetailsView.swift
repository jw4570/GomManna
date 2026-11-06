import SwiftUI
import MapKit

struct PlaceDetailsView: View {
    let model: PlaceDetailsViewModel
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(model.name).font(.title2.bold())
                        if let address = model.address {
                            Label(address, systemImage: "mappin.and.ellipse")
                                .font(.subheadline).foregroundStyle(.secondary)
                                .textSelection(.enabled)
                        }
                    }
                    .padding(.vertical, 8)
                }

                if model.phone != nil || model.website != nil {
                    Section("연락처") {
                        if let phone = model.phone, let url = model.phoneURL {
                            Link(destination: url) { Label(phone, systemImage: "phone") }
                        }
                        if let website = model.website {
                            Link(destination: website) {
                                Label(website.host ?? "웹사이트", systemImage: "globe")
                                    .lineLimit(1)
                            }
                        }
                    }
                }

                Section {
                    Button(action: model.showDirections) {
                        Label("도보 길찾기", systemImage: "figure.walk")
                    }
                    Button(action: model.openInMaps) {
                        Label("Apple 지도에서 자세히 보기", systemImage: "map")
                    }
                } footer: {
                    Text("Apple 지도에서 제공하는 장소 정보입니다. 추가 정보는 Apple 지도에서 확인할 수 있어요.")
                }
            }
            .scrollContentBackground(.hidden)
            .background(AppStyle.background)
            .tint(AppStyle.accent)
            .navigationTitle("장소 정보")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("닫기") { dismiss() }
                }
            }
        }
        .presentationDetents([.medium, .large])
        .presentationDragIndicator(.visible)
    }
}
