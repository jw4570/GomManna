import Foundation
import CoreLocation

struct MapMark: Identifiable {
    let id = UUID()
    var name: String
    let latitude: Double
    let longitude: Double

    var coordinate: CLLocationCoordinate2D {
        CLLocationCoordinate2D(latitude: latitude, longitude: longitude)
    }
}
