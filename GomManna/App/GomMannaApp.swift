//
//  GomMannaApp.swift
//  곰만나
//
//  Created by 이주원 on 10/8/26.
//

import SwiftUI

@main
struct GomMannaApp: App {
    var body: some Scene {
        WindowGroup {
            RootView()
                .tint(AppStyle.accent)
                .modifier(AppAppearanceModifier())
        }
    }
}
