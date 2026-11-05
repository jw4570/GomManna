# iOS 배포 준비 현황

기준: 2026-11-05. 운영자 GomManna / 문의 gommanna.support@gmail.com.

## 구현한 항목

- 홈 고정 헤더의 설정 버튼, 프로필·화면 모드·OS 권한 상태와 설정 이동.
- 개인정보처리방침·서비스 이용약관·위치 공유 안내·라이선스의 앱 내 오프라인 열람.
- 문의 이메일 작성. 인증 토큰이나 위치를 자동 첨부하지 않음.
- 버전·빌드·앱 정보.
- 실제 서버와 연결한 위치 공유 중단, 현재 모임의 메모/사진 삭제, 모임 탈퇴/종료.
- UserDefaults 사용 사유 CA92.1과 앱의 데이터 수집 항목을 담은 PrivacyInfo.xcprivacy.
- 토큰 자동 발급/Keychain 저장과 인증 만료 안내. 명시적인 확인 없이 새 사용자로 바꾸지 않음.

## 아직 배포를 막는 항목

1. **전체 익명 사용자 삭제 API와 실제 삭제 처리**
   - 현재 API에는 사용자·세션·관련 데이터 전체 삭제가 없다.
   - 모임 탈퇴, 공유 OFF, Keychain 초기화, 새 사용자 생성은 사용자 삭제가 아니다.
   - Apple은 자동 생성된 guest 계정도 삭제할 수 있어야 한다고 명시한다.
   - 서버에 본인 인증·방장 소유 모임 처리·다른 모임 참가 정보·위치·메모·사진·토큰 폐기·삭제 대기열 및 백업 처리 정책을 구현한 뒤 앱 설정의 실제 삭제 흐름에 연결한다.
   - 이메일 문의만으로 해당 요구사항을 충족했다고 간주하지 않는다.

2. **보유/파기 기간·위탁/국외이전·시행일 확정**
   - 사용자가 아직 미정으로 확인한 사항이다. LegalConfiguration의 해당 값은 nil이고 정책은 초안으로 표시한다.
   - 사용자/인증 정보, 활성 및 종료된 모임, 위치/메모, 삭제 대기 사진, 운영/접속 로그, 백업 각각의 보유와 파기 시점을 정한다.
   - C++ 서버는 공유 OFF에서 DB 데이터를 즉시 삭제하지 않는다. 정책에 즉시 삭제라고 쓰지 않는다.
   - AWS와 Supabase의 실제 계약 주체, 처리 업무, 저장/이전 국가·지역, 이전 근거·방법·시점·보유 기간과 권리 행사 방법을 확인한다.
   - 이용 대상 연령과 아동/보호자 절차, 위치정보법 적용 및 신고/등록·동의 절차, 위치기반서비스 약관과 법정 기록 보존 여부를 검토한다.

3. **공개 정책 URL과 App Store Connect 정보**
   - 확정한 정책을 로그인 없이 접근 가능한 HTTPS 페이지에 게시한다. 앱에 문서가 있는 것만으로 공개 URL을 대신할 수 없다.
   - LegalConfiguration.privacyPolicyURL/termsURL과 App Store Connect의 Privacy Policy/Support URL을 실제 공개 주소로 설정한다.
   - 문의 메일함의 수신/회신 절차와 공개 지원 페이지를 운영한다.

## App Store 개인정보 표시 검토 초안

광고·추적 목적 없음. 앱 기능 제공 목적으로 사용자와 연결되는 다음 항목을 검토한다.

- 사용자 ID: 익명 서버 사용자 ID와 닉네임(screen name).
- 정확한/대략적 위치: 권한에 따라 측정한 좌표 및 메모 작성 당시 위치.
- 기타 사용자 콘텐츠: 모임 이름과 메모.
- 사진/비디오 분류: 실제로는 사용자가 첨부하는 사진만 전송.
- 이메일 주소/고객 지원: 사용자가 직접 보내는 문의 메일.
- 실제 서버·프록시의 IP/접속 로그 수집과 보관 설정을 확인하고 필요한 분류를 추가한다.

PrivacyInfo.xcprivacy는 App Store Connect의 개인정보 응답을 자동으로 완성하지 않는다. 수집 항목별 선택 여부와 이용 목적, 제3자 SDK/인프라를 포함한 전체 처리 현황을 다시 확인한다. 앱에서 별도 사용자 추적은 하지 않으므로 사용하지 않는 ATT 권한을 요청하지 않는다.

## 최종 검증

- 실제 아이폰에서 위치 허용/거절/대략적 위치, 화면 잠금·홈 이동·다른 앱 사용 중 전송 지속, 공유 OFF/권한 해제/탈퇴/종료 시 GPS·전송 중단, 강제 종료 제한과 공유 OFF/ON, QR 카메라, 딥링크, 사진 방향/크기, 사진 삭제·비공개 전환 확인.
- 인증 만료/통신 실패/방 종료/참가 권한 상실/정원/중복 참가/사진 저장소 503 확인.
- 메모·사진 등 사용자 콘텐츠에 대한 App Review 1.2 적용 여부 및 신고·차단·대응·필터링 기능 검토. 현재 앱의 문의·모임 나가기만으로 모든 의무가 충족된다고 단정하지 않는다.
- 지원 기기/최소 iOS 버전과 실제 보유 기기에서의 실행 가능성 확인. 현재 앱 타깃은 `$(RECOMMENDED_IPHONEOS_DEPLOYMENT_TARGET)`을 사용하며 현재 Xcode에서 iOS 17.0으로 해석됨.
- Release Archive에서 앱 아이콘·권한 문구·PrivacyInfo.xcprivacy·공개 문서와 서명/버전/암호화 수출 신고/연령 등급/스크린샷/심사 계정 및 초대 흐름 확인.
- 앱 바이너리와 공유 Scheme에 테스트 토큰·DB 비밀번호·Storage Secret이 포함되지 않았는지 확인.
- 사진 파일 삭제 대기열 처리와 로그·백업 파기를 실제 운영 환경에서 검증.

## 기준 문서

- [Apple: 익명 계정을 포함한 계정 삭제](https://developer.apple.com/help/app-review/guideline-reference/5-1-1-account-deletion/)
- [Apple: App Review Guidelines](https://developer.apple.com/app-store/review/guidelines/)
- [Apple: App Privacy Details와 공개 정책 URL](https://developer.apple.com/app-store/app-privacy-details/)
- [Apple: UserDefaults 필수 사용 사유](https://developer.apple.com/documentation/bundleresources/app-privacy-configuration/nsprivacyaccessedapitypes/nsprivacyaccessedapitype)
- [개인정보보호위원회 안내서: 개인정보 처리방침 작성지침 최신본 확인](https://pipc.go.kr/np/cop/bbs/selectBoardList.do?bbsId=BS217&mCode=D010030000.Updated)
