import MapKit
import Observation

@Observable
final class PlaceSearchViewModel {
    var isOpen = false
    var query = ""
    private(set) var results: [MKMapItem] = []
    private(set) var isLoading = false
    private(set) var message: String?
    var showingMapResults = false
    @ObservationIgnored var region = MKCoordinateRegion(
        center: CLLocationCoordinate2D(latitude: 37.5665, longitude: 126.9780),
        span: MKCoordinateSpan(latitudeDelta: 0.03, longitudeDelta: 0.03))
    @ObservationIgnored private var search: MKLocalSearch?
    @ObservationIgnored private var generation = 0

    func searchPlaces(forMap: Bool = false) async -> [MKMapItem]? {
        generation += 1
        let requestID = generation
        search?.cancel()
        results = []
        message = nil
        let text = query.trimmingCharacters(in: .whitespacesAndNewlines)
        isLoading = !text.isEmpty && isOpen
        guard isLoading else { return nil }
        defer { if generation == requestID { isLoading = false } }
        do {
            if !forMap { try await Task.sleep(for: .milliseconds(300)) }
            try Task.checkCancellation()
            let request = MKLocalSearch.Request()
            request.naturalLanguageQuery = text
            let visibleRegion = region
            request.region = visibleRegion
            request.resultTypes = [.address, .pointOfInterest]
            let operation = MKLocalSearch(request: request)
            search = operation
            let response = try await withTaskCancellationHandler {
                try await operation.start()
            } onCancel: {
                Task { @MainActor in operation.cancel() }
            }
            try Task.checkCancellation()
            guard generation == requestID, isOpen else { return nil }
            results = forMap ? response.mapItems.filter {
                let coordinate = $0.placemark.coordinate
                let latitudeDistance = abs(coordinate.latitude - visibleRegion.center.latitude)
                let rawLongitudeDistance = abs(coordinate.longitude - visibleRegion.center.longitude)
                let longitudeDistance = min(rawLongitudeDistance, 360 - rawLongitudeDistance)
                return latitudeDistance <= visibleRegion.span.latitudeDelta / 2
                    && longitudeDistance <= visibleRegion.span.longitudeDelta / 2
            } : response.mapItems
            if results.isEmpty { message = "검색 결과가 없어요. 다른 이름으로 검색해 주세요." }
            return results
        } catch {
            guard !Task.isCancelled, generation == requestID, isOpen else { return nil }
            message = "장소를 검색하지 못했어요. 잠시 후 다시 시도해 주세요."
            return nil
        }
    }

    func close() {
        generation += 1
        search?.cancel()
        search = nil
        isOpen = false
        query = ""
        results = []
        message = nil
        isLoading = false
        showingMapResults = false
    }
}
