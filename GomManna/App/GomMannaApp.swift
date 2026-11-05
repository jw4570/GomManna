//
//  GomMannaApp.swift
//  곰만나
//
//  Created by 이주원 on 10/8/26.
//

import SwiftUI

@main
struct GomMannaApp: App {
    @AppStorage("GomManna.appearance") private var appearance = "system"
    private var colorScheme: ColorScheme? {
        appearance == "dark" ? .dark : appearance == "light" ? .light : nil
    }
    var body: some Scene {
        WindowGroup {
            RootView()
                .tint(AppStyle.accent)
                .preferredColorScheme(colorScheme)
        }
    }
}
