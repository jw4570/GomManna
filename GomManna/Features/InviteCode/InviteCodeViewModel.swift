import SwiftUI
import CoreImage.CIFilterBuiltins
import Observation

@Observable final class InviteCodeViewModel {
    var copied = false
    func qrImage(for code: String) -> UIImage? {
        let filter = CIFilter.qrCodeGenerator()
        filter.message = Data(code.utf8)
        guard let output = filter.outputImage,
              let image = CIContext().createCGImage(output.transformed(by: CGAffineTransform(scaleX: 12, y: 12)), from: output.extent.applying(CGAffineTransform(scaleX: 12, y: 12))) else { return nil }
        return UIImage(cgImage: image)
    }
    func copy(_ code: String) { UIPasteboard.general.string = code; copied = true }
}
