// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "SolutionFinderEnhanced",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "SolutionFinderEnhanced", targets: ["SolutionFinderEnhanced"])
    ],
    targets: [
        .target(
            name: "GameCore",
            path: "portable",
            exclude: ["tests"],
            publicHeadersPath: "include"
        ),
        .executableTarget(
            name: "SolutionFinderEnhanced",
            dependencies: ["GameCore"],
            path: "Sources/SolutionFinderGUI"
        )
    ]
)
