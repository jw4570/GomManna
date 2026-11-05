import SwiftUI
import CoreImage.CIFilterBuiltins
import Observation

@Observable final class InviteCodeViewModel {
    var copied = false
    var invitation: RoomInvitation?
    var error: String?
    var loading = false
    func load(room: Room, session: AppSession) async {
        loading = true
        defer { loading = false }
        do {
            let token = try await session.repositories.auth.accessToken()
            invitation = try await session.repositories.rooms.fetchInvite(roomID: room.id, token: token)
            error = nil
        } catch {
            guard !Task.isCancelled, !error.isRequestCancellation else { return }
            self.error = session.describe(error)
        }
    }
    func qrImage(for code: String) -> UIImage? {
        let filter = CIFilter.qrCodeGenerator()
        filter.message = Data(code.utf8)
        guard let output = filter.outputImage,
              let image = CIContext().createCGImage(output.transformed(by: CGAffineTransform(scaleX: 12, y: 12)), from: output.extent.applying(CGAffineTransform(scaleX: 12, y: 12))) else { return nil }
        return UIImage(cgImage: image)
    }
    func copy(_ code: String) { UIPasteboard.general.string = code; copied = true }
}
