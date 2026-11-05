import Foundation

/// 출시 전 운영자가 확정할 공개 정보입니다. 비밀키·토큰을 넣지 않습니다.
enum LegalConfiguration {
    static let operatorName: String? = "GomManna"
    static let supportEmail: String? = "gommanna.support@gmail.com"
    static let effectiveDate: String? = nil
    static let privacyPolicyURL: URL? = nil
    static let termsURL: URL? = nil
    static let retentionDescription: String? = nil
    static let processorsAndRegions: String? = nil

    static var isDraft: Bool {
        operatorName == nil || supportEmail == nil || effectiveDate == nil ||
        retentionDescription == nil || processorsAndRegions == nil
    }
    static var replacements: [String: String] {
        ["{{operator}}": operatorName ?? "운영자 정보 확정 전",
         "{{email}}": supportEmail ?? "문의 연락처 확정 전",
         "{{effectiveDate}}": effectiveDate ?? "시행일 확정 전",
         "{{retention}}": retentionDescription ?? "서버 데이터·로그·백업의 보유 및 파기 기간 확정 전",
         "{{processors}}": processorsAndRegions ?? "AWS·Supabase의 실제 계약 주체, 처리 업무, 저장 국가·지역, 이전 근거와 보유 기간 확인 후 기재"]
    }
}
