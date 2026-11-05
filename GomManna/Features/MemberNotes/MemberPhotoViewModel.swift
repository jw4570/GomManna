import Foundation
import UIKit
import Observation
import OSLog

@MainActor
@Observable final class MemberPhotoViewModel {
    private static let logger = Logger(subsystem: "GomManna", category: "MapResponse")
    private(set) var image: UIImage?
    private(set) var error: String?
    private(set) var request: MemberPhotoRequest?
    private(set) var loadedRequest: MemberPhotoRequest?

    func clear() {
        request = nil
        loadedRequest = nil
        image = nil
        error = nil
    }

    func load(_ expected: MemberPhotoRequest, session: AppSession, maximumPixelSize: Int = 2048) async {
        clear()
        request = expected
        do {
            let token = try await session.repositories.auth.accessToken()
            let started = ContinuousClock.now
            let data = try await session.repositories.photos.fetchPhoto(roomID: expected.roomID, memberID: expected.memberID, token: token)
            Self.logger.debug("Photo response: \(String(describing: started.duration(to: .now)), privacy: .public), bytes: \(data.count)")
            try Task.checkCancellation()
            let decoded = try await MemberPhotoDecoder.shared.decode(data, maximumPixelSize: maximumPixelSize)
            try Task.checkCancellation()
            guard request == expected,
                  let current = session.current, current.room.id == expected.roomID,
                  let member = current.room.members.first(where: { $0.id == expected.memberID }),
                  MemberPhotoRequest(roomID: current.room.id, member: member) == expected else { return }
            image = UIImage(cgImage: decoded)
            loadedRequest = expected
        } catch {
            guard request == expected, !Task.isCancelled, !error.isRequestCancellation else { return }
            self.error = session.describe(error)
        }
    }
}
