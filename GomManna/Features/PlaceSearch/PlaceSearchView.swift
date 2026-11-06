import SwiftUI
import MapKit

struct PlaceSearchView: View {
    @Bindable var model: PlaceSearchViewModel
    let width: CGFloat
    let onOpen: () -> Void
    let onSelect: (MKMapItem) -> Void
    let onMapResults: ([MKMapItem]) -> Void
    @State private var searchTask: Task<Void, Never>?
    @FocusState private var focused: Bool
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 0) {
                Button {
                    onOpen()
                    withAnimation(reduceMotion ? nil : .spring(response: 0.35, dampingFraction: 0.85)) {
                        model.isOpen = true
                    }
                    focused = true
                } label: {
                    Image(systemName: "magnifyingglass")
                        .font(.system(size: 20, weight: .semibold))
                        .frame(width: 48, height: 48)
                }
                .accessibilityLabel("건물과 장소 검색")
                if model.isOpen {
                    TextField("건물, 장소 검색", text: $model.query)
                        .focused($focused)
                        .submitLabel(.search)
                        .autocorrectionDisabled()
                        .onSubmit {
                            focused = false
                            searchTask?.cancel()
                            model.showingMapResults = true
                            searchTask = Task {
                                if let places = await model.searchPlaces(forMap: true), !Task.isCancelled {
                                    onMapResults(places)
                                }
                            }
                        }
                    Button {
                        focused = false
                        withAnimation(reduceMotion ? nil : .spring(response: 0.35, dampingFraction: 0.85)) {
                            model.close()
                        }
                    } label: {
                        Image(systemName: "xmark").frame(width: 44, height: 48)
                    }
                    .accessibilityLabel("검색 닫기")
                }
            }
            .foregroundStyle(AppStyle.accent)
            .frame(width: model.isOpen ? width : 48, height: 48, alignment: .leading)
            .background(AppStyle.background, in: RoundedRectangle(cornerRadius: 24))
            .clipped()
            .shadow(color: .black.opacity(0.10), radius: 8, y: 3)

            if model.isOpen && (!model.query.isEmpty) {
                VStack(spacing: 0) {
                    if model.isLoading {
                        ProgressView().frame(maxWidth: .infinity).padding(20)
                    } else if let message = model.message {
                        Text(message).font(.subheadline).foregroundStyle(.secondary).padding(20)
                    } else if model.showingMapResults {
                        Text("지도에 \(model.results.count)개의 장소를 표시했어요")
                            .font(.caption).foregroundStyle(.secondary).padding(12)
                    } else if !model.results.isEmpty {
                        ScrollView {
                            LazyVStack(spacing: 0) {
                                ForEach(Array(model.results.enumerated()), id: \.offset) { _, place in
                                    Button {
                                        focused = false
                                        onSelect(place)
                                        withAnimation { model.close() }
                                    } label: {
                                        HStack(spacing: 12) {
                                            Image(systemName: "mappin.and.ellipse").foregroundStyle(AppStyle.accent)
                                            VStack(alignment: .leading, spacing: 4) {
                                                Text(place.name ?? "장소").foregroundStyle(.primary)
                                                Text(place.placemark.title ?? "")
                                                    .font(.caption).foregroundStyle(.secondary).lineLimit(2)
                                            }
                                            Spacer(minLength: 0)
                                        }
                                        .padding(16).frame(maxWidth: .infinity, alignment: .leading)
                                        .contentShape(Rectangle())
                                    }
                                    Divider().padding(.leading, 44)
                                }
                            }
                        }
                        .frame(maxHeight: 280)
                        .scrollDismissesKeyboard(.interactively)
                    }
                }
                .frame(width: width)
                .background(AppStyle.background, in: RoundedRectangle(cornerRadius: 20))
                .clipShape(RoundedRectangle(cornerRadius: 20))
                .shadow(color: .black.opacity(0.10), radius: 8, y: 3)
            }
        }
        .buttonStyle(.plain)
        .onChange(of: model.query) { _, _ in
            searchTask?.cancel()
            model.showingMapResults = false
            searchTask = Task { _ = await model.searchPlaces() }
        }
        .onDisappear { searchTask?.cancel(); model.close() }
    }
}
