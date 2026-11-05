import SwiftUI
import VisionKit
import Vision
import AVFoundation

struct QRScannerView: View {
    let onScan: (String) -> Void
    @State private var permitted = false
    @State private var error: String?
    var body: some View {
        Group {
            if permitted && error == nil {
                Scanner(onScan: onScan, onError: { error = $0 })
            } else {
                VStack(spacing: 16) {
                    Image(systemName: "qrcode.viewfinder").font(.system(size: 64)).foregroundStyle(AppStyle.accent)
                    Text(error ?? "카메라를 준비하고 있어요").font(.subheadline).multilineTextAlignment(.center)
                    if error != nil {
                        Button("설정 열기") { if let url = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(url) } }
                    }
                }.padding(24).frame(maxWidth: .infinity, maxHeight: .infinity).background(AppStyle.field)
            }
        }
        .task {
            guard DataScannerViewController.isSupported else { error = "이 기기에서는 QR 스캔을 지원하지 않아요. 코드를 입력해 주세요."; return }
            permitted = await AVCaptureDevice.requestAccess(for: .video)
            if !permitted { error = "카메라 권한을 허용하거나 초대 코드를 입력해 주세요." }
            else if !DataScannerViewController.isAvailable { error = "지금은 카메라를 사용할 수 없어요. 코드를 입력해 주세요." }
        }
    }
    private struct Scanner: UIViewControllerRepresentable {
        let onScan: (String) -> Void
        let onError: (String) -> Void
        func makeCoordinator() -> Coordinator { Coordinator(onScan: onScan, onError: onError) }
        func makeUIViewController(context: Context) -> DataScannerViewController {
            let scanner = DataScannerViewController(recognizedDataTypes: [.barcode(symbologies: [.qr])],
                qualityLevel: .balanced, recognizesMultipleItems: false, isGuidanceEnabled: true, isHighlightingEnabled: true)
            scanner.delegate = context.coordinator
            do { try scanner.startScanning() } catch { Task { onError(error.localizedDescription) } }
            return scanner
        }
        func updateUIViewController(_ controller: DataScannerViewController, context: Context) {}
        static func dismantleUIViewController(_ controller: DataScannerViewController, coordinator: Coordinator) { controller.stopScanning() }
        final class Coordinator: NSObject, DataScannerViewControllerDelegate {
            let onScan: (String) -> Void
            let onError: (String) -> Void
            private var delivered = false
            init(onScan: @escaping (String) -> Void, onError: @escaping (String) -> Void) {
                self.onScan = onScan
                self.onError = onError
            }
            func dataScanner(_ dataScanner: DataScannerViewController, didAdd addedItems: [RecognizedItem], allItems: [RecognizedItem]) {
                guard !delivered else { return }
                for item in addedItems {
                    if case .barcode(let barcode) = item, let value = barcode.payloadStringValue {
                        delivered = true
                        dataScanner.stopScanning()
                        onScan(value)
                        return
                    }
                }
            }
            func dataScanner(_ dataScanner: DataScannerViewController, becameUnavailableWithError error: DataScannerViewController.ScanningUnavailable) {
                dataScanner.stopScanning()
                onError("카메라를 사용할 수 없어요. 초대 코드를 입력해 주세요.")
            }
        }
    }
}
