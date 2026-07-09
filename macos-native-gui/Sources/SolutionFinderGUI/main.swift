import AppKit
import GameCore
import SwiftUI
import WebKit

@main
struct SolutionFinderEnhancedApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @Environment(\.openWindow) private var openWindow

    var body: some Scene {
        WindowGroup("Solution Finder Enhanced") {
            ContentView()
                .frame(minWidth: 980, minHeight: 700)
        }
        WindowGroup("Opener Importer", id: "opener-importer") {
            OpenerImporterView()
                .frame(minWidth: 820, minHeight: 640)
        }
        WindowGroup("Detection Console", id: "detection-console") {
            DetectionConsoleView()
                .frame(minWidth: 720, minHeight: 420)
        }
        .commands {
            CommandGroup(replacing: .newItem) {}
            CommandMenu("Tools") {
                Button("Opener Importer") {
                    openWindow(id: "opener-importer")
                }
                .keyboardShortcut("i", modifiers: [.command, .shift])
                Button("Detection Console") {
                    openWindow(id: "detection-console")
                }
                .keyboardShortcut("d", modifiers: [.command, .shift])
                Divider()
                Button("Install Editable Opener Database") {
                    OpeningDatabase.installEditableDatabase()
                }
                Button("Show Opener Database Folder") {
                    OpeningDatabase.showUserDatabaseFolder()
                }
            }
        }
    }
}

@MainActor
final class DetectionDebugLog: ObservableObject {
    static let shared = DetectionDebugLog()

    @Published var text = ""

    private let timestampFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss.SSS"
        return formatter
    }()

    func append(_ message: String) {
        let timestamp = timestampFormatter.string(from: Date())
        text += text.isEmpty ? "[\(timestamp)] \(message)" : "\n[\(timestamp)] \(message)"
    }

    func appendBlock(title: String, lines: [String]) {
        append(([title] + lines.map { "  \($0)" }).joined(separator: "\n"))
    }

    func clear() {
        text = ""
    }
}

struct DetectionConsoleView: View {
    @StateObject private var log = DetectionDebugLog.shared

    var body: some View {
        VStack(spacing: 12) {
            HStack {
                Text("Detection Console")
                    .font(.headline)
                Spacer()
                Button {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(log.text, forType: .string)
                } label: {
                    Label("Copy", systemImage: "doc.on.doc")
                }
                Button {
                    log.clear()
                } label: {
                    Label("Clear", systemImage: "xmark.circle")
                }
            }

            ScrollView {
                Text(log.text.isEmpty ? "Play tab opener detection details will appear here." : log.text)
                    .font(.system(size: 12.5, weight: .regular, design: .monospaced))
                    .foregroundStyle(log.text.isEmpty ? Color(nsColor: .secondaryLabelColor) : Color(white: 0.92))
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                    .textSelection(.enabled)
                    .padding(12)
            }
            .background(Color(red: 0.10, green: 0.10, blue: 0.10))
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))
        }
        .padding(16)
        .background(Color(nsColor: .windowBackgroundColor))
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }

    @MainActor
    static func restartApp() {
        let bundlePath = Bundle.main.bundleURL.path
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/bin/sh")
        process.arguments = ["-c", "sleep 0.35; /usr/bin/open \"$1\"", "restart-solution-finder", bundlePath]

        do {
            try process.run()
            NSApp.terminate(nil)
        } catch {
            Task { @MainActor in
                NSAlert(error: AppMessageError("Could not restart app: \(error.localizedDescription)")).runModal()
            }
        }
    }
}

enum FinderCommand: String, CaseIterable, Identifiable {
    case percent
    case tetris
    case tetrisPath = "tetris-path"
    case path
    case setup
    case cover
    case ren
    case spin

    var id: String { rawValue }

    var executableCommand: String {
        switch self {
        case .tetrisPath:
            return "path"
        default:
            return rawValue
        }
    }

    var supportsOutputBase: Bool {
        self == .tetris || self == .path || self == .setup || self == .tetrisPath || self == .cover || self == .ren || self == .spin
    }

    var supportsFormat: Bool {
        self == .path || self == .setup || self == .tetrisPath || self == .spin
    }

    var supportsHoldDropKicks: Bool {
        self != .spin
    }

    var supportsLines: Bool {
        self == .percent || self == .tetris || self == .tetrisPath || self == .path || self == .setup || self == .spin
    }

    var supportsThreads: Bool {
        self == .percent || self == .tetris || self == .tetrisPath || self == .path || self == .setup
    }

    var supportsCoverSettings: Bool {
        self == .cover
    }

    var supportsSpinSettings: Bool {
        self == .spin
    }

    var defaultOutputBase: String {
        switch self {
        case .percent:
            return ""
        case .tetris:
            return "output/tetris"
        case .tetrisPath:
            return "output/tetris_path"
        case .path:
            return "output/path"
        case .setup:
            return "output/setup.html"
        case .cover:
            return "output/cover.csv"
        case .ren:
            return "output/ren"
        case .spin:
            return "output/spin"
        }
    }

    var extraArgsPlaceholder: String {
        switch self {
        case .cover:
            return "-ms 2 -mc 1"
        case .ren:
            return "-lp output/ren_log.txt"
        case .spin:
            return "-fb 0 -ft auto"
        default:
            return "-td 3"
        }
    }

    var tip: String {
        switch self {
        case .percent:
            return "Estimates perfect clear success from the current board and patterns."
        case .tetris:
            return "Checks only perfect clears that finish with a 4-line I clear."
        case .tetrisPath:
            return "Generates path output for Tetris-ending perfect clears."
        case .path:
            return "Creates solution routes; use HTML for clickable fumen previews."
        case .setup:
            return "Finds ways to build the target setup across fumen pages."
        case .cover:
            return "Reports which pattern sequences can cover the selected setup pages."
        case .ren:
            return "Searches combo routes; plain piece lists like IJLOSTZ work best."
        case .spin:
            return "Searches T-spin setups; T lines controls the required clear size."
        }
    }

    var syntax: String {
        switch self {
        case .percent:
            return "Set Patterns and Lines, then run the current fumen board."
        case .tetris:
            return "Set Patterns and Lines; only Tetris-ending clears count."
        case .tetrisPath:
            return "Use Format and Output base to choose HTML or CSV solution output."
        case .path:
            return "Use Format, Output base, Lines, Hold, Drop, and Kicks."
        case .setup:
            return "Mark setup cells in the editor: I means fill, O means margin, gray means fixed."
        case .cover:
            return "Use Mode, Sort, Mirror, Accum, and Priority to tune coverage checks."
        case .ren:
            return "Set Patterns to the piece order, usually a sequence like IJLOSTZ."
        case .spin:
            return "Use T lines, Filter, Roof, Split, and Extra args to tune the spin search."
        }
    }

    var helpLines: [String] {
        switch self {
        case .percent:
            return [
                "Use this for quick success-rate checks.",
                "Patterns examples: *p5, T,*p5, [IJLOS]p5.",
                "Lines is the maximum clear height to search."
            ]
        case .tetris:
            return [
                "Same idea as percent, but only counts PCs ending in a Tetris.",
                "Use normal pattern syntax: *pN, comma steps, or bracket pools.",
                "Output is a chance summary in Command Output."
            ]
        case .tetrisPath:
            return [
                "Combines path output with the Tetris-ending condition.",
                "HTML output gives clickable fumen links for Preview.",
                "CSV output opens in Command Output."
            ]
        case .path:
            return [
                "Finds actual placement routes for successful sequences.",
                "Use HTML/link for clickable solutions, CSV for table output.",
                "Extra args can tune depth, split, keys, and filters."
            ]
        case .setup:
            return [
                "Uses I cells as required fill and O cells as margin.",
                "Setup height is detected automatically from the highest marked row.",
                "Use gray for existing fixed blocks; other editor tools are hidden in setup mode."
            ]
        case .cover:
            return [
                "Checks whether patterns can cover the fumen pages.",
                "Mode narrows the clear type, such as tetris, tss, tsd, or 4l.",
                "Results are written as CSV and open in Command Output."
            ]
        case .ren:
            return [
                "Patterns are usually a piece order like IJLOSTZ.",
                "Use Extra CLI args for log path or advanced ren options.",
                "Results are generated as HTML."
            ]
        case .spin:
            return [
                "T lines controls the required T-spin clear size.",
                "Board height is auto-applied with -fb 0 and -ft from the highest occupied row.",
                "HTML is best for previewing solutions; CSV is best for scanning."
            ]
        }
    }
}

enum HoldMode: String, CaseIterable, Identifiable {
    case use
    case avoid

    var id: String { rawValue }
}

enum DropMode: String, CaseIterable, Identifiable {
    case softdrop
    case harddrop

    var id: String { rawValue }
}

enum InputSource: String, CaseIterable, Identifiable {
    case field = "Field"
    case fumen = "Fumen"

    var id: String { rawValue }
}

enum FumenPanelTab: String, CaseIterable, Identifiable {
    case editor = "Editor"
    case play = "Play"
    case output = "Output"
    case preview = "Preview"

    var id: String { rawValue }
}

struct OutputFile: Identifiable {
    let id = UUID()
    let url: URL
    let size: Int64
    let modified: Date

    var name: String { url.lastPathComponent }
    var sizeLabel: String {
        ByteCountFormatter.string(fromByteCount: size, countStyle: .file)
    }
}

struct FumenOperation {
    var type: Int = 0
    var rotation: Int = 0
    var position: Int = 0
}

struct OpeningDetectionResult {
    let preset: FumenOpeningPreset
    let occupancyScore: Double
    let colorScore: Double
    let comparableColorCells: Int
    let mirrored: Bool
    var pageIndex: Int = 0
    var rowOffset: Int = 0

    var score: Double {
        occupancyScore
    }

    var overallScore: Double {
        comparableColorCells >= 6 ? occupancyScore * 0.82 + colorScore * 0.18 : occupancyScore
    }

    var displayName: String {
        var detail = "\(preset.name)\(mirrored ? " mirrored" : "")"
        if pageIndex > 0 {
            detail += " page \(pageIndex + 1)"
        }
        if rowOffset != 0 {
            detail += " \(rowOffset > 0 ? "+" : "")\(rowOffset) rows"
        }
        return detail
    }

    var scoreSummary: String {
        let overall = Int((overallScore * 100.0).rounded())
        let shape = Int((occupancyScore * 100.0).rounded())
        let color = comparableColorCells >= 6 ? "\(Int((colorScore * 100.0).rounded()))%" : "n/a"
        return "Overall \(overall)% | Shape \(shape)% | Color \(color)"
    }
}

struct FumenEditorSnapshot {
    let fumenCode: String
    let nativeFumenCells: [Int]
    let fumenPages: [[Int]]
    let fumenComments: [String]
    let fumenOperations: [FumenOperation]
    let currentFumenPage: Int
    let fumenComment: String
    let fumenOperation: FumenOperation
    let fumenPlaceMino: Bool
    let inputSource: InputSource
    let fumenPanelTab: FumenPanelTab
    let selectedOpeningPresetID: String
    let selectedOpeningGroupID: String
    let selectedOpeningVariationID: String
}

struct FumenOpeningPreset: Identifiable, Hashable {
    let id: String
    let name: String
    let openerName: String
    let variationName: String
    let cells: [Int]
    let code: String?
    let earlyVariantDetection: Bool

    init(id: String, name: String, openerName: String? = nil, variationName: String = "Base", cells: [Int], earlyVariantDetection: Bool = false) {
        self.id = id
        self.name = name
        self.openerName = openerName ?? name
        self.variationName = variationName
        self.cells = cells
        self.code = nil
        self.earlyVariantDetection = earlyVariantDetection
    }

    init(id: String, name: String, openerName: String? = nil, variationName: String = "Base", code: String, earlyVariantDetection: Bool = false) {
        self.id = id
        self.name = name
        self.openerName = openerName ?? name
        self.variationName = variationName
        self.cells = []
        self.code = code
        self.earlyVariantDetection = earlyVariantDetection
    }
}

struct FumenOpeningGroup: Identifiable {
    let id: String
    let name: String
    let presets: [FumenOpeningPreset]
}

func openingCells(bottomRows: [String]) -> [Int] {
    var cells = Array(repeating: 0, count: 240)
    let rows = bottomRows.prefix(23)
    for (bottomOffset, row) in rows.enumerated() {
        let boardRow = 22 - bottomOffset
        for (column, character) in row.prefix(10).enumerated() {
            cells[boardRow * 10 + column] = openingCellValue(character)
        }
    }
    return cells
}

func openingCellValue(_ character: Character) -> Int {
    switch character {
    case "I": return 1
    case "L": return 2
    case "O": return 3
    case "Z": return 4
    case "T": return 5
    case "J": return 6
    case "S": return 7
    case "X", "G": return 8
    default: return 0
    }
}

let fallbackFumenOpeningPresets: [FumenOpeningPreset] = [
    FumenOpeningPreset(id: "empty", name: "Empty board", openerName: "Empty board", variationName: "Empty", cells: openingCells(bottomRows: [])),
    FumenOpeningPreset(
        id: "honey-cup-base",
        name: "Honey Cup Base",
        openerName: "Honey Cup",
        variationName: "Base",
        code: "v115@EhBtCewwDeg0BtRpxwR4Aei0RpwwR4AezhJeAgWNAF?0VeE5oo2AiHEXEEBAAA"
    ),
    FumenOpeningPreset(
        id: "honey-cup-standard",
        name: "Honey Cup Standard Variation",
        openerName: "Honey Cup",
        variationName: "Standard",
        code: "v115@lgzhBeh0BeBtilRpg0CeBthlRpg0R4AeB8ilA8R4Be?I8AeH8AeD8JeAgHrgxSFeAtQpAewSFeAtRaAPFeAtmeAAAr?gRaHeQpQaFewSwhAtGewSwhleAAArgxhFeAPAewSwhFeQaQ?pAeAtFeRaleAAAlgyhCewhQpBegWQawhQaAegWwhQpCeSaA?egWGeQaneAAArggWAPFeAtglAeAPFeAtAegWAPFeAtglleA?AA"
    ),
    FumenOpeningPreset(
        id: "honey-cup-tst",
        name: "Honey Cup TST Variation",
        openerName: "Honey Cup",
        variationName: "TST",
        code: "v115@VgRpHeRpGewhhlAeg0BeBtAewhhlAeg0CeBtwhhlh0?R4AeB8whhlA8R4BeI8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "mountainous-stacking-2-base",
        name: "Mountainous Stacking 2 Base",
        openerName: "Mountainous Stacking 2",
        variationName: "Base",
        code: "v115@FhAtwhBewwDeBtwhRpxwR4AeAtglwhRpwwR4Aeilwh?JeAgWNAF0VeE5oo2AiHEXEEBAAA"
    ),
    FumenOpeningPreset(
        id: "mountainous-stacking-2-setup-a",
        name: "Mountainous Stacking 2 Setup A",
        openerName: "Mountainous Stacking 2",
        variationName: "Setup A",
        code: "v115@VgwhGeh0whGeh0whEehlh0whBtDeglh0RpBtR4Aegl?B8RpA8R4BeI8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "mountainous-stacking-2-setup-b",
        name: "Mountainous Stacking 2 Setup B",
        openerName: "Mountainous Stacking 2",
        variationName: "Setup B",
        code: "v115@bgzhFej0Fehlh0AeBtDeglh0RpBtR4AeglB8RpA8R4?BeI8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "mountainous-stacking-2-setup-c",
        name: "Mountainous Stacking 2 Setup C",
        openerName: "Mountainous Stacking 2",
        variationName: "Setup C",
        code: "v115@lgRphlwhBeg0BeRpAtglwhi0CeBtglwhi0R4AeAtB8?whg0A8R4BeI8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "mountainous-stacking-2-setup-d",
        name: "Mountainous Stacking 2 Setup D",
        openerName: "Mountainous Stacking 2",
        variationName: "Setup D",
        code: "v115@fgBtHewhBtg0BehlRpwhi0CeglRpwhi0R4AeglB8wh?g0A8R4BeI8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "mountainous-stacking-2-compromise",
        name: "Mountainous Stacking 2 Compromise",
        openerName: "Mountainous Stacking 2",
        variationName: "Compromise",
        code: "v115@cgBtIeBtwhBeg0BehlRpwhi0CeglRpwhi0R4AeglB8?whg0A8R4BeI8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "stray-cannon-base",
        name: "Stray Cannon Base",
        openerName: "Stray Cannon",
        variationName: "Base",
        code: "v115@9gwhIewhglQ4AewwBeg0BewhglR4xwAeg0RpwhhlQ4?wwAeh0RpJeAgH"
    ),
    FumenOpeningPreset(
        id: "stray-cannon-standard",
        name: "Stray Cannon Standard Variation",
        openerName: "Stray Cannon",
        variationName: "Standard",
        code: "v115@lgRpg0whQ4BeAtBeRpg0whR4BtCeh0whA8Q4AtilAe?BtwhC8glA8BeA8BtF8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "stray-cannon-kuromitsu-stacking",
        name: "Stray Cannon Kuromitsu Stacking",
        openerName: "Stray Cannon",
        variationName: "Kuromitsu Stacking",
        code: "v115@XgAtHeBtHeAtRpBei0whBtRpCeQ4g0whA8BtilAeR4?whC8glA8BeA8Q4whF8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "stray-cannon-tst",
        name: "Stray Cannon TST Variation",
        openerName: "Stray Cannon",
        variationName: "TST",
        code: "v115@Wgh0Heg0Q4FewhAeg0R4BeBtAewhAeRpQ4CeBtwhA8?RpilAeBtwhC8glA8BeA8BtF8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "stray-cannon-alternative",
        name: "Stray Cannon Alternative Variation",
        openerName: "Stray Cannon",
        variationName: "Alternative",
        code: "v115@dgh0BeQ4Eeg0whBeR4BeBtg0whAeRpQ4CeBtwhA8Rp?ilAeBtwhC8glA8BeA8BtF8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "satsuki-stacking-base",
        name: "Satsuki Stacking Base",
        openerName: "Satsuki Stacking",
        variationName: "Base",
        code: "v115@ygAtHeBtHeAtwhCewwDeg0whAeRpxwR4Aeg0whAeRp?wwR4Aeh0whJeAgH"
    ),
    FumenOpeningPreset(
        id: "satsuki-stacking-standard",
        name: "Satsuki Stacking Standard",
        openerName: "Satsuki Stacking",
        variationName: "Standard",
        code: "v115@cgglRpGeglRpCehlBehlA8Aeh0AtglCeB8whg0Btgl?R4AeB8whg0AtA8R4BeB8whF8AeB8whE8AeC8JeAgHsgglAt?FeAPgWAeAtFeAPAeglAtFeAPgWkeAAA"
    ),
    FumenOpeningPreset(
        id: "satsuki-stacking-standard-early-z-o",
        name: "Satsuki Stacking Standard, Early Z+O",
        openerName: "Satsuki Stacking",
        variationName: "Standard, Early Z+O",
        code: "v115@cgh0AtGeg0BtCehlBeg0AtA8AeglRpglCeB8whglRp?glR4AeB8whhlA8R4BeB8whF8AeB8whE8AeC8JeAgH"
    ),
    FumenOpeningPreset(
        id: "hachispin-base",
        name: "Hachispin Base",
        openerName: "Hachispin",
        variationName: "Base",
        code: "v115@5gAtCewhDeBtCewhAei0AtDewhilg0RpAeR4whglCe?RpR4KeAgH"
    ),
    FumenOpeningPreset(
        id: "hachispin-standard",
        name: "Hachispin Standard",
        openerName: "Hachispin",
        variationName: "Standard",
        code: "v115@pgh0EewhBeg0DeR4whRpg0AeBtR4A8whRpA8BeBtB8?whhlA8AeG8glB8CeD8glJeAgH"
    ),
    FumenOpeningPreset(
        id: "hachispin-compromise",
        name: "Hachispin Compromise",
        openerName: "Hachispin",
        variationName: "Compromise",
        code: "v115@pgh0Geglg0DeR4ilg0AeBtR4A8RpwhA8BeBtB8Rpwh?A8AeG8whB8CeD8whJeAgH"
    ),
    FumenOpeningPreset(
        id: "albatross-base",
        name: "Albatross Base",
        openerName: "Albatross",
        variationName: "Base",
        code: "v115@9gwhDeR4CewhilR4CeAtwhgli0RpAeBtwhCeg0RpAe?AtKeAgH"
    ),
    FumenOpeningPreset(
        id: "albatross-standard",
        name: "Albatross Standard",
        openerName: "Albatross",
        variationName: "Standard",
        code: "v115@3gQ4Aeh0FeR4g0CezhAtQ4g0AehlA8RpBtB8BeglA8?RpAtC8AeA8glJeAgH"
    ),
    FumenOpeningPreset(
        id: "albatross-greed",
        name: "Albatross Greed Variation",
        openerName: "Albatross",
        variationName: "Greed Variation",
        code: "v115@pgwhIewhCeQ4Aeh0BewhCeR4g0CewhCeAtQ4g0Aehl?A8RpBtB8BeglA8RpAtC8AeA8glJeAgH"
    ),
    FumenOpeningPreset(
        id: "albatross-perfect-clear-attempt",
        name: "Albatross Perfect Clear Attempt",
        openerName: "Albatross",
        variationName: "Perfect Clear Attempt",
        code: "v115@9gilDeR4whglh0CeR4AtwhA8g0RpAeB8BtwhA8g0Rp?C8AtA8whJeAgH"
    ),
    FumenOpeningPreset(
        id: "pelican-base",
        name: "Pelican Base",
        openerName: "Pelican",
        variationName: "Base",
        code: "v115@ChhlBewhQ4BtCeglRpwhR4BtAeg0glRpwhAeQ4Cei0?AewhJeAgH"
    ),
    FumenOpeningPreset(
        id: "pelican-standard",
        name: "Pelican Standard",
        openerName: "Pelican",
        variationName: "Standard",
        code: "v115@1gBtDeglDeBtCeglAeh0AezhQ4hlg0BeRpB8R4A8g0?A8AeRpC8Q4A8JeAgH"
    ),
    FumenOpeningPreset(
        id: "pelican-riviclia",
        name: "Pelican Riviclia Variation",
        openerName: "Pelican",
        variationName: "Riviclia Variation",
        code: "v115@ygwhHeAtwhAeR4BeRpBtwhR4Beg0RpAtwwwhilAeg0?B8xwA8glA8Aeh0C8wwA8JeAgH"
    ),
    FumenOpeningPreset(
        id: "c-spin-base",
        name: "C-Spin Base",
        openerName: "C-Spin",
        variationName: "Base",
        code: "v115@pgh0Heg0Ieg0AeywEeglBewwQ4CeRpglAeBtR4BeRp?hlAeBtQ4zhJeAgH"
    ),
    FumenOpeningPreset(
        id: "c-spin-standard",
        name: "C-Spin Standard",
        openerName: "C-Spin",
        variationName: "Standard",
        code: "v115@egg0Ieg0B8BeQ4hlwhh0A8CeR4glwhRpA8AeC8Q4gl?whRpA8BeB8BtwhC8AeD8BtD8AeG8JeAgH"
    ),
    FumenOpeningPreset(
        id: "c-spin-tki-signature-base",
        name: "C-Spin TKI Signature, Base",
        openerName: "C-Spin",
        variationName: "TKI Signature, Base",
        code: "v115@pgh0Heg0Ieg0FeQ4BeglBewwAeBtR4AeglAexwRpBt?Q4AehlAewwRpzhJeAgH"
    ),
    FumenOpeningPreset(
        id: "c-spin-tki-signature-standard",
        name: "C-Spin TKI Signature, Standard",
        openerName: "C-Spin",
        variationName: "TKI Signature, Standard",
        code: "v115@BgwhIewhIewhIewhDeAtQ4CeB8BeBtR4BeA8CeAtRp?Q4BeA8Aei0RpA8hlA8BeA8g0D8glA8AeG8glB8AeG8JeAgH?lgwSGeRaxSFeQaAeAtHeBtqeAAAkgRpGeBtRaFexhBPGexS?qeAAAkgBPHeAPAewhFeRpxhGeQpreAAA"
    ),
    FumenOpeningPreset(
        id: "kisaragi-stacking-base",
        name: "Kisaragi Stacking Base",
        openerName: "Kisaragi Stacking",
        variationName: "Base",
        code: "v115@xgAtHeBtHeAtwhCewwDeg0whAeRpxwR4Aeg0whAeRp?wwR4Aeh0whKeAgH"
    ),
    FumenOpeningPreset(
        id: "kisaragi-stacking-standard",
        name: "Kisaragi Stacking Standard",
        openerName: "Kisaragi Stacking",
        variationName: "Standard",
        code: "v115@bgglRpGeglRpCeAtg0BehlA8AeglBtg0CeB8AeglAt?h0R4AeB8whhlA8R4BeB8whF8AeB8whE8AeC8whJeAgHrggW?APFeAtglAeAPFeAtAegWAPFeAtglleAAA"
    ),
    FumenOpeningPreset(
        id: "kisaragi-stacking-standard-early-z-o",
        name: "Kisaragi Stacking Standard, Early Z+O",
        openerName: "Kisaragi Stacking",
        variationName: "Standard, Early Z+O",
        code: "v115@bgh0AtGeg0BtCehlBeg0AtA8AeglRpglCeB8AeglRp?glR4AeB8whhlA8R4BeB8whF8AeB8whE8AeC8whJeAgH"
    ),
    FumenOpeningPreset(
        id: "kisaragi-stacking-honey-cup",
        name: "Kisaragi Stacking Honey Cup",
        openerName: "Kisaragi Stacking",
        variationName: "Honey Cup",
        code: "v115@lgRphlBeAtg0BeRpA8hlBtg0CeB8hlAth0R4AeB8wh?hlA8R4BeB8whF8AeB8whE8AeC8whJeAgHrggWAPFeAtglAe?APFeAtAegWAPFeAtglleAAA"
    ),
    FumenOpeningPreset(
        id: "kuromitsu-stacking-base",
        name: "Kuromitsu Stacking Base",
        openerName: "Kuromitsu Stacking",
        variationName: "Base",
        code: "v115@+gRpEeAtwhAeRpg0wwBeBtwhAeR4g0xwAeAtglwhR4?h0wwAeilwhJeAgH"
    ),
    FumenOpeningPreset(
        id: "kuromitsu-stacking-standard",
        name: "Kuromitsu Stacking Standard",
        openerName: "Kuromitsu Stacking",
        variationName: "Standard",
        code: "v115@XgAtHeBtHeAtR4BehlRpwhR4g0CeglRpwhB8i0Aegl?B8whD8BeC8whE8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "kuromitsu-stacking-honey-cup",
        name: "Kuromitsu Stacking Honey Cup",
        openerName: "Kuromitsu Stacking",
        variationName: "Honey Cup",
        code: "v115@lgRphlBeR4BeRpAtglwhR4g0CeBtglwhB8i0AeAtB8?whD8BeC8whE8AeH8AeD8JeAgH"
    ),
    FumenOpeningPreset(
        id: "tki-3-base",
        name: "TKI 3 Base",
        openerName: "TKI 3",
        variationName: "Base",
        code: "v115@KhAtAeR4Beg0RpBtR4Ceg0RpAtzhAeh0JeAgH"
    ),
    FumenOpeningPreset(
        id: "tki-3-flat-top",
        name: "TKI 3 Flat Top",
        openerName: "TKI 3",
        variationName: "Flat Top",
        code: "v115@BhilFeAtglR4Beg0RpBtR4Ceg0RpAtzhAeh0JeAgHB?hxwglFeQpwwxSEehWQLwSFegWQeAAA"
    ),
    FumenOpeningPreset(
        id: "tki-3-castle-top",
        name: "TKI 3 Castle Top",
        openerName: "TKI 3",
        variationName: "Castle Top",
        code: "v115@zgglIeglIehlAeAtAeR4Beg0RpBtR4Ceg0RpAtzhAe?h0JeAgH"
    ),
    FumenOpeningPreset(
        id: "tki-3-fonzie",
        name: "TKI 3 Fonzie Variant",
        openerName: "TKI 3",
        variationName: "Fonzie Variant",
        code: "v115@/gglGeilAtAeR4Beg0RpBtR4Ceg0RpAtzhAeh0JeAg?H"
    ),
    FumenOpeningPreset(
        id: "tki-3-perfect-clear-attempt",
        name: "TKI 3 Perfect Clear Attempt",
        openerName: "TKI 3",
        variationName: "Perfect Clear Attempt",
        code: "v115@9gi0BezhglRpg0BeBtilRpA8CeBtR4D8AeB8R4A8Je?AgH"
    ),
    FumenOpeningPreset(
        id: "dt-cannon-base",
        name: "DT Cannon Base",
        openerName: "DT Cannon",
        variationName: "Base",
        code: "v115@3gwwHeywwhGeR4whBtAeRpAeR4glwhg0BtRpAeilwh?i0JeAgHLhwSQaAegWQpDexSAtAeAPRpCeglBtAeBPwhJeAA?A3gQLHeyhQaBewhFeg0AeQLwhEeQag0wwQLgHDeRaAtwwAe?gHJeAAA"
    ),
    FumenOpeningPreset(
        id: "dt-cannon-form-a",
        name: "DT Cannon Form A",
        openerName: "DT Cannon",
        variationName: "Form A",
        code: "v115@agQ4FehlAeR4FeglBtQ4Bewhh0AeglA8BtRpwhg0Be?D8Rpwhg0CeE8whB8AeI8AeG8JeAgH"
    ),
    FumenOpeningPreset(
        id: "dt-cannon-form-b",
        name: "DT Cannon Form B",
        openerName: "DT Cannon",
        variationName: "Form B",
        code: "v115@hghlDeR4CeglBtAeR4whh0AeglA8BtRpwhg0BeD8Rp?whg0CeE8whB8AeI8AeG8JeAgH"
    ),
    FumenOpeningPreset(
        id: "dt-cannon-form-c",
        name: "DT Cannon Form C",
        openerName: "DT Cannon",
        variationName: "Form C",
        code: "v115@hghlQ4BeAtEeglR4BtAewhh0AeglA8Q4AtRpwhg0Be?D8Rpwhg0CeE8whB8AeI8AeG8JeAgH"
    ),
    FumenOpeningPreset(
        id: "gassho-tsd-base",
        name: "Gassho TSD Base",
        openerName: "Gassho TSD",
        variationName: "Base",
        code: "v115@9gwhh0Feglwhg0CeAtQ4ilwhg0wwAeBtR4RpwhywAt?BeQ4RpJeAgWHAPNUFDK+1BA9gAtRaGewwQaHewwAewhHeCP?PeAAPAA"
    )
]

struct PortableOpeningDatabase: Decodable {
    let version: Int
    let openers: [PortableOpeningPreset]
}

struct PortableOpeningPreset: Decodable {
    let id: String
    let name: String
    let openerName: String
    let variationName: String
    let code: String?
    let cells: [Int]?
    let earlyVariantDetection: Bool?

    func preset() -> FumenOpeningPreset {
        if let code {
            return FumenOpeningPreset(
                id: id,
                name: name,
                openerName: openerName,
                variationName: variationName,
                code: code,
                earlyVariantDetection: earlyVariantDetection ?? false
            )
        }

        return FumenOpeningPreset(
            id: id,
            name: name,
            openerName: openerName,
            variationName: variationName,
            cells: cells ?? [],
            earlyVariantDetection: earlyVariantDetection ?? false
        )
    }
}

enum OpeningDatabase {
    static var userDatabaseURL: URL {
        WorkspaceLocator.runtimeSupportDirectory().appendingPathComponent("openers.json")
    }

    static func loadPresets(fallback: [FumenOpeningPreset]) -> [FumenOpeningPreset] {
        for url in candidateURLs() {
            guard let data = try? Data(contentsOf: url),
                  let database = try? JSONDecoder().decode(PortableOpeningDatabase.self, from: data) else {
                continue
            }
            let presets = database.openers.map { $0.preset() }
            if !presets.isEmpty {
                return presets
            }
        }

        return fallback
    }

    @MainActor
    static func installEditableDatabase() {
        let destination = userDatabaseURL
        do {
            try FileManager.default.createDirectory(at: destination.deletingLastPathComponent(), withIntermediateDirectories: true)
            if !FileManager.default.fileExists(atPath: destination.path) {
                if let bundled = Bundle.main.url(forResource: "openers", withExtension: "json") {
                    try FileManager.default.copyItem(at: bundled, to: destination)
                } else {
                    let database = PortableOpeningExportDatabase(
                        version: 1,
                        openers: fallbackFumenOpeningPresets.map { PortableOpeningExportPreset(preset: $0) }
                    )
                    let data = try JSONEncoder.prettyPrinted.encode(database)
                    try data.write(to: destination, options: .atomic)
                }
            }
            NSWorkspace.shared.activateFileViewerSelecting([destination])
        } catch {
            NSAlert(error: AppMessageError("Could not install editable opener database: \(error.localizedDescription)")).runModal()
        }
    }

    @MainActor
    static func showUserDatabaseFolder() {
        let folder = userDatabaseURL.deletingLastPathComponent()
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        NSWorkspace.shared.open(folder)
    }

    private static func candidateURLs() -> [URL] {
        var urls: [URL] = []
        urls.append(userDatabaseURL)
        let workspace = WorkspaceLocator.findWorkspace()
        urls.append(workspace.appendingPathComponent("shared/openers.json"))
        urls.append(workspace.appendingPathComponent("openers.json"))

        if let bundled = Bundle.main.url(forResource: "openers", withExtension: "json") {
            urls.append(bundled)
        }
        return urls
    }
}

struct PortableOpeningExportDatabase: Encodable {
    let version: Int
    let openers: [PortableOpeningExportPreset]
}

struct PortableOpeningExportPreset: Encodable {
    let id: String
    let name: String
    let openerName: String
    let variationName: String
    let code: String?
    let cells: [Int]?
    let earlyVariantDetection: Bool?

    init(id: String, name: String, openerName: String, variationName: String, code: String?, cells: [Int]?, earlyVariantDetection: Bool = false) {
        self.id = id
        self.name = name
        self.openerName = openerName
        self.variationName = variationName
        self.code = code
        self.cells = cells
        self.earlyVariantDetection = earlyVariantDetection ? true : nil
    }

    init(preset: FumenOpeningPreset) {
        id = preset.id
        name = preset.name
        openerName = preset.openerName
        variationName = preset.variationName
        code = preset.code
        cells = preset.code == nil ? preset.cells : nil
        earlyVariantDetection = preset.earlyVariantDetection ? true : nil
    }
}

let fumenOpeningPresets: [FumenOpeningPreset] = OpeningDatabase.loadPresets(fallback: fallbackFumenOpeningPresets)

let fumenOpeningGroups: [FumenOpeningGroup] = Dictionary(grouping: fumenOpeningPresets, by: \.openerName)
    .map { openerName, presets in
        FumenOpeningGroup(
            id: presets.first?.id ?? openerName,
            name: openerName,
            presets: presets.sorted {
                if $0.variationName == "Base" { return true }
                if $1.variationName == "Base" { return false }
                if $0.variationName == "Empty" { return true }
                if $1.variationName == "Empty" { return false }
                return $0.variationName < $1.variationName
            }
        )
    }
    .sorted { lhs, rhs in
        if lhs.name == "Empty board" { return true }
        if rhs.name == "Empty board" { return false }
        return lhs.name < rhs.name
    }

struct OpenerVariationDraft: Identifiable, Hashable {
    let id = UUID()
    var name: String
    var code: String
    var pages: [[Int]] = []
    var selectedPage = 0
    var status = "Not decoded"

    init(name: String = "", code: String = "") {
        self.name = name
        self.code = code
    }
}

struct OpenerImportFile: Codable {
    let exportedAt: String
    let openers: [OpenerImportRecord]
}

struct OpenerImportRecord: Codable, Identifiable, Hashable {
    let name: String
    let earlyVariantDetection: Bool?
    let base: OpenerImportVariation?
    let variations: [OpenerImportVariation]

    var id: String { name }
}

struct OpenerImportVariation: Codable, Hashable {
    let name: String
    let code: String
    let detectionPage: Int
    let pageCount: Int
}

@MainActor
final class OpenerImporterModel: ObservableObject {
    @Published var openerName = ""
    @Published var earlyVariantDetection = false
    @Published var baseCode = ""
    @Published var basePages: [[Int]] = []
    @Published var baseSelectedPage = 0
    @Published var baseStatus = "Not decoded"
    @Published var variations: [OpenerVariationDraft] = []
    @Published var queuedOpeners: [OpenerImportRecord] = []
    @Published var selectedPreviewID = "base"
    @Published var status = "Ready"

    let workspaceURL: URL
    let outputURL: URL
    let fumenURL: URL

    init() {
        let workspace = WorkspaceLocator.runtimeSupportDirectory()
        WorkspaceLocator.prepareRuntimeSupport(at: workspace)
        workspaceURL = workspace
        outputURL = workspace.appendingPathComponent("output")
        fumenURL = WorkspaceLocator.findFumenHTML(workspaceURL: workspace)
        variations = [OpenerVariationDraft(name: "Standard", code: "")]
        loadBook()
    }

    var selectedPreviewPages: [[Int]] {
        if selectedPreviewID == "base" {
            return basePages
        }
        guard let variation = variations.first(where: { $0.id.uuidString == selectedPreviewID }) else {
            return []
        }
        return variation.pages
    }

    var selectedPreviewPageIndex: Int {
        if selectedPreviewID == "base" {
            return min(baseSelectedPage, max(basePages.count - 1, 0))
        }
        guard let variation = variations.first(where: { $0.id.uuidString == selectedPreviewID }) else {
            return 0
        }
        return min(variation.selectedPage, max(variation.pages.count - 1, 0))
    }

    var selectedPreviewCells: [Int] {
        let pages = selectedPreviewPages
        guard pages.indices.contains(selectedPreviewPageIndex) else {
            return Array(repeating: 0, count: 240)
        }
        return pages[selectedPreviewPageIndex]
    }

    var validationMessages: [String] {
        var messages: [String] = []
        let trimmedName = openerName.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmedName.isEmpty {
            messages.append("Add an opener name.")
        } else if queuedOpeners.contains(where: { $0.name.caseInsensitiveCompare(trimmedName) == .orderedSame }) {
            messages.append("This opener name is already queued.")
        }

        let base = baseCode.trimmingCharacters(in: .whitespacesAndNewlines)
        if base.isEmpty {
            messages.append("Add a base fumen code.")
        } else {
            messages += warnings(forName: "Base", code: base, pages: basePages, selectedPage: baseSelectedPage)
        }

        var seenNames = Set<String>()
        var seenCodes = Set<String>()
        if !base.isEmpty {
            seenCodes.insert(base)
        }

        for variation in variations {
            let name = variation.name.trimmingCharacters(in: .whitespacesAndNewlines)
            let code = variation.code.trimmingCharacters(in: .whitespacesAndNewlines)
            if name.isEmpty, code.isEmpty { continue }
            if name.isEmpty {
                messages.append("A variation is missing its name.")
            } else if !seenNames.insert(name.lowercased()).inserted {
                messages.append("Duplicate variation name: \(name).")
            }
            if code.isEmpty {
                messages.append("\(name.isEmpty ? "A variation" : name) is missing its fumen code.")
            } else if !seenCodes.insert(code).inserted {
                messages.append("\(name.isEmpty ? "A variation" : name) duplicates another code in this import.")
            } else {
                messages += warnings(forName: name.isEmpty ? "Variation" : name, code: code, pages: variation.pages, selectedPage: variation.selectedPage)
            }
        }

        return messages.isEmpty ? ["Ready to export."] : messages
    }

    var canExport: Bool {
        !queuedOpeners.isEmpty || canQueueCurrentOpener
    }

    var canQueueCurrentOpener: Bool {
        let hasName = !openerName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        let hasBase = !baseCode.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        let hasInvalidRequired = validationMessages.contains { message in
            message.hasPrefix("Add ") || message.contains("missing") || message.contains("Invalid") || message.contains("duplicates another") || message.contains("already queued")
        }
        return hasName && hasBase && !hasInvalidRequired
    }

    func addVariation() {
        variations.append(OpenerVariationDraft(name: "Variation \(variations.count + 1)", code: ""))
    }

    func removeVariation(id: UUID) {
        variations.removeAll { $0.id == id }
        if selectedPreviewID == id.uuidString {
            selectedPreviewID = "base"
        }
    }

    func setBasePages(_ pages: [[Int]]) {
        basePages = normalizedPages(pages)
        baseSelectedPage = min(baseSelectedPage, max(basePages.count - 1, 0))
        baseStatus = basePages.isEmpty ? "Invalid or empty fumen" : "\(basePages.count) page\(basePages.count == 1 ? "" : "s") decoded"
    }

    func setBaseCode(_ code: String, pages: [[Int]]) {
        baseCode = code
        setBasePages(pages)
        selectedPreviewID = "base"
    }

    func setVariationPages(id: UUID, pages: [[Int]]) {
        guard let index = variations.firstIndex(where: { $0.id == id }) else { return }
        variations[index].pages = normalizedPages(pages)
        variations[index].selectedPage = min(variations[index].selectedPage, max(variations[index].pages.count - 1, 0))
        let count = variations[index].pages.count
        variations[index].status = count == 0 ? "Invalid or empty fumen" : "\(count) page\(count == 1 ? "" : "s") decoded"
    }

    func setVariationCode(id: UUID, code: String, pages: [[Int]]) {
        guard let index = variations.firstIndex(where: { $0.id == id }) else { return }
        variations[index].code = code
        setVariationPages(id: id, pages: pages)
        selectedPreviewID = id.uuidString
    }

    func queueCurrentOpener() {
        guard let record = currentImportRecord() else {
            status = "Finish the current opener before adding it to the book"
            return
        }
        queuedOpeners.removeAll { $0.name.caseInsensitiveCompare(record.name) == .orderedSame }
        queuedOpeners.append(record)
        queuedOpeners.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
        resetCurrentOpener()
        status = "Saved \(record.name) to the book draft"
    }

    func removeQueuedOpener(_ record: OpenerImportRecord) {
        queuedOpeners.removeAll { $0.id == record.id }
    }

    func editQueuedOpener(_ record: OpenerImportRecord) {
        openerName = record.name
        earlyVariantDetection = record.earlyVariantDetection ?? false
        baseCode = record.base?.code ?? ""
        basePages = []
        baseSelectedPage = record.base?.detectionPage ?? 0
        baseStatus = baseCode.isEmpty ? "No base fumen" : "Not decoded"
        variations = record.variations.map { variation in
            OpenerVariationDraft(name: variation.name, code: variation.code)
        }
        if variations.isEmpty {
            variations = [OpenerVariationDraft(name: "Standard", code: "")]
        }
        selectedPreviewID = "base"
        queuedOpeners.removeAll { $0.id == record.id }
        status = "Editing \(record.name)"
    }

    func newOpenerGroup() {
        resetCurrentOpener()
        status = "New opener group"
    }

    func loadBook() {
        let presets = OpeningDatabase.loadPresets(fallback: fallbackFumenOpeningPresets)
        queuedOpeners = Self.records(from: presets)
        resetCurrentOpener()
        status = "Loaded \(queuedOpeners.count) opener groups"
    }

    func exportJSON() {
        var records = queuedOpeners
        if let current = currentImportRecord() {
            records.removeAll { $0.name.caseInsensitiveCompare(current.name) == .orderedSame }
            records.append(current)
        }
        guard !records.isEmpty else {
            status = "Nothing to save"
            return
        }
        records.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }

        let database = PortableOpeningExportDatabase(
            version: 1,
            openers: Self.presets(from: records)
        )

        do {
            let destination = OpeningDatabase.userDatabaseURL
            try FileManager.default.createDirectory(at: destination.deletingLastPathComponent(), withIntermediateDirectories: true)
            let data = try JSONEncoder.prettyPrinted.encode(database)
            try data.write(to: destination, options: .atomic)
            queuedOpeners = records
            resetCurrentOpener()
            status = "Saved \(records.count) opener group\(records.count == 1 ? "" : "s") to \(destination.path). Restart the app to use the updated book."
        } catch {
            status = "Save failed: \(error.localizedDescription)"
        }
    }

    private func currentImportRecord() -> OpenerImportRecord? {
        guard canQueueCurrentOpener else { return nil }
        let base = OpenerImportVariation(
            name: "Base",
            code: baseCode.trimmingCharacters(in: .whitespacesAndNewlines),
            detectionPage: baseSelectedPage,
            pageCount: basePages.count
        )
        let exportedVariations = variations.compactMap { variation -> OpenerImportVariation? in
            let name = variation.name.trimmingCharacters(in: .whitespacesAndNewlines)
            let code = variation.code.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !name.isEmpty, !code.isEmpty else { return nil }
            return OpenerImportVariation(
                name: name,
                code: code,
                detectionPage: variation.selectedPage,
                pageCount: variation.pages.count
            )
        }

        return OpenerImportRecord(
            name: openerName.trimmingCharacters(in: .whitespacesAndNewlines),
            earlyVariantDetection: earlyVariantDetection,
            base: base,
            variations: exportedVariations
        )
    }

    private func resetCurrentOpener() {
        openerName = ""
        earlyVariantDetection = false
        baseCode = ""
        basePages = []
        baseSelectedPage = 0
        baseStatus = "Not decoded"
        variations = [OpenerVariationDraft(name: "Standard", code: "")]
        selectedPreviewID = "base"
    }

    private static func records(from presets: [FumenOpeningPreset]) -> [OpenerImportRecord] {
        Dictionary(grouping: presets.filter { $0.id != "empty" }, by: \.openerName)
            .map { openerName, presets in
                let sortedPresets = presets.sorted {
                    if $0.variationName == "Base" { return true }
                    if $1.variationName == "Base" { return false }
                    return $0.variationName.localizedCaseInsensitiveCompare($1.variationName) == .orderedAscending
                }
                let basePreset = sortedPresets.first { $0.variationName == "Base" }
                let variations = sortedPresets
                    .filter { $0.variationName != "Base" }
                    .compactMap { preset -> OpenerImportVariation? in
                        guard let code = preset.code else { return nil }
                        return OpenerImportVariation(name: preset.variationName, code: code, detectionPage: 0, pageCount: 0)
                    }
                return OpenerImportRecord(
                    name: openerName,
                    earlyVariantDetection: sortedPresets.contains { $0.earlyVariantDetection },
                    base: basePreset.flatMap { preset in
                        guard let code = preset.code else { return nil }
                        return OpenerImportVariation(name: "Base", code: code, detectionPage: 0, pageCount: 0)
                    },
                    variations: variations
                )
            }
            .sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
    }

    private static func presets(from records: [OpenerImportRecord]) -> [PortableOpeningExportPreset] {
        var presets = [
            PortableOpeningExportPreset(
                id: "empty",
                name: "Empty board",
                openerName: "Empty board",
                variationName: "Empty",
                code: nil,
                cells: []
            )
        ]

        for record in records {
            let openerSlug = slug(record.name)
            if let base = record.base {
                presets.append(
                    PortableOpeningExportPreset(
                        id: "\(openerSlug)-base",
                        name: "\(record.name) Base",
                        openerName: record.name,
                        variationName: "Base",
                        code: base.code,
                        cells: nil,
                        earlyVariantDetection: record.earlyVariantDetection ?? false
                    )
                )
            }

            for variation in record.variations {
                presets.append(
                    PortableOpeningExportPreset(
                        id: "\(openerSlug)-\(slug(variation.name))",
                        name: "\(record.name) \(variation.name)",
                        openerName: record.name,
                        variationName: variation.name,
                        code: variation.code,
                        cells: nil,
                        earlyVariantDetection: record.earlyVariantDetection ?? false
                    )
                )
            }
        }

        return presets
    }

    private static func slug(_ value: String) -> String {
        let allowed = CharacterSet.alphanumerics
        let parts = value.lowercased().unicodeScalars.map { scalar -> Character in
            allowed.contains(scalar) ? Character(scalar) : "-"
        }
        return String(parts)
            .split(separator: "-")
            .joined(separator: "-")
    }

    private func normalizedPages(_ pages: [[Int]]) -> [[Int]] {
        pages.map { page in
            var cells = Array(page.prefix(240))
            if cells.count < 240 {
                cells += Array(repeating: 0, count: 240 - cells.count)
            }
            for index in 230..<240 {
                cells[index] = 0
            }
            return cells
        }
    }

    private func warnings(forName name: String, code: String, pages: [[Int]], selectedPage: Int) -> [String] {
        var messages: [String] = []
        if !code.hasPrefix("v115@") {
            messages.append("Invalid fumen prefix for \(name).")
        }
        if fumenOpeningPresets.contains(where: { $0.code == code }) {
            messages.append("\(name) has the same code as an opener already in the database.")
        }
        guard pages.indices.contains(selectedPage) else { return messages }
        let cells = pages[selectedPage]
        for preset in fumenOpeningPresets where preset.id != "empty" {
            guard !preset.cells.isEmpty else { continue }
            let directScore = fumenOccupancyScore(fumenOccupancyMask(cells), fumenOccupancyMask(preset.cells))
            let mirrorScore = fumenOccupancyScore(fumenOccupancyMask(cells), fumenOccupancyMask(preset.cells, mirrored: true))
            if directScore >= 0.98 {
                messages.append("\(name) appears to match \(preset.name).")
            } else if mirrorScore >= 0.98 {
                messages.append("\(name) appears to match mirrored \(preset.name).")
            }
        }
        return messages
    }
}

extension JSONEncoder {
    static var prettyPrinted: JSONEncoder {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return encoder
    }
}

struct AppMessageError: LocalizedError {
    let errorDescription: String?

    init(_ message: String) {
        self.errorDescription = message
    }
}

@MainActor
final class AppModel: ObservableObject {
    @Published var command: FinderCommand = .percent
    @Published var inputSource: InputSource = .fumen
    @Published var hold: HoldMode = .use
    @Published var drop: DropMode = .softdrop
    @Published var kicks = "srs"
    @Published var lines = "4"
    @Published var threads = ""
    @Published var format = "html"
    @Published var outputBase = FinderCommand.percent.defaultOutputBase
    @Published var coverMode = "normal"
    @Published var coverSort = "input"
    @Published var coverMirror = false
    @Published var coverAccum = false
    @Published var coverPriority = false
    @Published var coverFumenCodes: [String] = []
    @Published var selectedCoverFumenIndex = 0
    @Published var spinFilter = "none"
    @Published var spinRoof = true
    @Published var spinSplit = false
    @Published var spinAutoHeight = "1"
    @Published var extraArgs = ""
    @Published var field = ""
    @Published var patterns = ""
    @Published var log = ""
    @Published var rawLog = ""
    @Published var verboseOutput = false {
        didSet {
            refreshDisplayedLog()
        }
    }
    @Published var fumenCode = ""
    @Published var nativeFumenCells = Array(repeating: 0, count: 240)
    @Published var fumenPages = [Array(repeating: 0, count: 240)]
    @Published var fumenComments = [""]
    @Published var fumenOperations = [FumenOperation()]
    @Published var currentFumenPage = 0
    @Published var fumenComment = ""
    @Published var fumenOperation = FumenOperation()
    @Published var fumenPlaceMino = false
    @Published var selectedOpeningPresetID = fumenOpeningPresets[0].id
    @Published var selectedOpeningGroupID = fumenOpeningGroups[0].id
    @Published var selectedOpeningVariationID = fumenOpeningGroups[0].presets[0].id
    @Published var selectedFumenColor = 8
    @Published var fumenRowFill = false
    @Published var importScreenshotColors = true
    @Published var previewFumenCode = ""
    @Published var previewFumenPages = [Array(repeating: 0, count: 240)]
    @Published var previewFumenComments = [""]
    @Published var previewFumenOperations = [FumenOperation()]
    @Published var currentPreviewFumenPage = 0
    @Published var status = "Ready"
    @Published var isRunning = false
    @Published var outputFiles: [OutputFile] = []
    @Published var selectedOutputFile: OutputFile?
    @Published var fumenPanelTab: FumenPanelTab = .editor
    @Published var embeddedOutputURL: URL?
    @Published var playSettingsApplyToken = 0
    private var isAutoUpdatingCommandHeight = false

    let workspaceURL: URL
    let runnerURL: URL
    let inputURL: URL
    let outputURL: URL
    let fieldURL: URL
    let patternsURL: URL
    let setupFieldURL: URL
    let fumenURL: URL
    private var runningProcess: Process?
    private var terminationObserver: NSObjectProtocol?

    init() {
        let workspace = WorkspaceLocator.runtimeSupportDirectory()
        WorkspaceLocator.prepareRuntimeSupport(at: workspace)
        workspaceURL = workspace
        runnerURL = WorkspaceLocator.findRunnerURL(workspaceURL: workspace)
        inputURL = workspace.appendingPathComponent("input")
        outputURL = workspace.appendingPathComponent("output")
        fieldURL = inputURL.appendingPathComponent("field.txt")
        patternsURL = inputURL.appendingPathComponent("patterns.txt")
        setupFieldURL = inputURL.appendingPathComponent("setup-field.txt")
        fumenURL = WorkspaceLocator.findFumenHTML(workspaceURL: workspace)
        terminationObserver = NotificationCenter.default.addObserver(
            forName: NSApplication.willTerminateNotification,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            MainActor.assumeIsolated {
                self?.terminateRunningProcess()
            }
        }
        loadInputs()
        refreshFiles()
    }

    deinit {
        MainActor.assumeIsolated {
            if let terminationObserver {
                NotificationCenter.default.removeObserver(terminationObserver)
            }
            runningProcess?.terminate()
        }
    }

    func loadInputs() {
        field = (try? String(contentsOf: fieldURL, encoding: .utf8)) ?? ""
        patterns = (try? String(contentsOf: patternsURL, encoding: .utf8)) ?? ""
        status = "Inputs loaded"
    }

    func loadSample() {
        let sample = workspaceURL.appendingPathComponent("samples/template")
        field = (try? String(contentsOf: sample.appendingPathComponent("field.txt"), encoding: .utf8)) ?? field
        patterns = (try? String(contentsOf: sample.appendingPathComponent("patterns.txt"), encoding: .utf8)) ?? patterns
        status = "Sample loaded"
    }

    func saveInputs() {
        do {
            try FileManager.default.createDirectory(at: inputURL, withIntermediateDirectories: true)
            try normalized(field).write(to: fieldURL, atomically: true, encoding: .utf8)
            try normalized(patterns).write(to: patternsURL, atomically: true, encoding: .utf8)
            status = "Inputs saved"
        } catch {
            status = "Save failed: \(error.localizedDescription)"
        }
    }

    func updateForCommand() {
        applyDefaultOutputBaseForCommand()

        switch command {
        case .percent:
            status = "Ready: percent writes probability results to the output log"
        case .tetris:
            status = "Ready: tetris reports PCs that end with an I-piece 4-line clear"
        case .tetrisPath:
            if format.isEmpty { format = "html" }
            status = "Ready: tetris-path outputs PC paths whose final clear is an I-piece Tetris"
        case .path:
            if format.isEmpty { format = "html" }
            status = "Ready: path can generate link/html/csv outputs"
        case .setup:
            if format == "link" { format = "html" }
            if ![0, 1, 3, 8].contains(selectedFumenColor) {
                selectedFumenColor = 1
            }
            convertCurrentFumenToSetupGray()
            fumenPlaceMino = false
            fumenOperation = FumenOperation()
            importScreenshotColors = false
            saveCurrentFumenPage()
            updateCommandHeightIfNeeded(force: true)
            status = "Ready: setup searches for ways to fill the requested form"
        case .cover:
            if !["input", "success", "success-desc", "success-asc"].contains(coverSort.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()) {
                coverSort = "input"
            }
            status = "Ready: cover checks which sequences can build selected fumen pages"
        case .ren:
            if patterns.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || patterns.contains("*") {
                patterns = "IJLOSTZ"
            }
            status = "Ready: ren searches combo routes from the current setup"
        case .spin:
            if !(1...3).contains(Int(lines.trimmingCharacters(in: .whitespacesAndNewlines)) ?? 0) {
                lines = "2"
            }
            if !["strict", "ignore-t", "ignore_t", "none"].contains(spinFilter.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()) {
                spinFilter = "none"
            }
            updateCommandHeightIfNeeded(force: true)
            if !["html", "csv"].contains(format.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()) { format = "html" }
            status = "Ready: spin searches T-spin setups from the current field"
        }
    }

    func addCoverFumen(_ code: String) {
        let trimmed = code.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else {
            status = "No fumen code to store"
            return
        }
        coverFumenCodes.append(trimmed)
        selectedCoverFumenIndex = coverFumenCodes.count - 1
        status = "Stored cover fumen \(coverFumenCodes.count)"
    }

    func replaceSelectedCoverFumen(_ code: String) {
        let trimmed = code.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else {
            status = "No fumen code to store"
            return
        }
        guard coverFumenCodes.indices.contains(selectedCoverFumenIndex) else {
            addCoverFumen(trimmed)
            return
        }
        coverFumenCodes[selectedCoverFumenIndex] = trimmed
        status = "Updated cover fumen \(selectedCoverFumenIndex + 1)"
    }

    func removeSelectedCoverFumen() {
        guard coverFumenCodes.indices.contains(selectedCoverFumenIndex) else { return }
        coverFumenCodes.remove(at: selectedCoverFumenIndex)
        selectedCoverFumenIndex = min(selectedCoverFumenIndex, max(coverFumenCodes.count - 1, 0))
        status = coverFumenCodes.isEmpty ? "Cover fumen list cleared" : "Removed cover fumen"
    }

    func selectOpeningPreset(_ preset: FumenOpeningPreset) {
        selectedOpeningPresetID = preset.id
        if let group = fumenOpeningGroups.first(where: { $0.presets.contains(preset) }) {
            selectedOpeningGroupID = group.id
        }
        selectedOpeningVariationID = preset.id
    }

    func markSetupHeightEdited() {
        guard command == .setup, !isAutoUpdatingCommandHeight else { return }
        updateCommandHeightIfNeeded(force: true)
    }

    func updateCommandHeightIfNeeded(force: Bool = false) {
        let detectedHeight = autoCommandHeight()
        let nextValue = String(detectedHeight)
        if command == .spin {
            if spinAutoHeight != nextValue {
                spinAutoHeight = nextValue
            }
            return
        }
        guard command == .setup else { return }
        guard lines != nextValue else { return }
        isAutoUpdatingCommandHeight = true
        lines = nextValue
        isAutoUpdatingCommandHeight = false
    }

    private func autoCommandHeight() -> Int {
        max(1, detectedCommandHeight())
    }

    private func detectedCommandHeight() -> Int {
        saveCurrentFumenPage()
        let cells = solverFumenCells()
        for row in fumenVisibleTopRow..<fumenSetupBottomRowExclusive {
            let rowStart = row * 10
            if cells[rowStart..<rowStart + 10].contains(where: { $0 != 0 }) {
                return fumenSetupBottomRowExclusive - row
            }
        }
        return 1
    }

    private func applyDefaultOutputBaseForCommand() {
        let knownDefaults = Set([
            "",
            "output/gui_result",
            "output/gui_path",
            "output/gui_setup",
            "output/gui_tetris_path",
            "output/tetris",
            "output/tetris_path",
            "output/path",
            "output/setup",
            "output/setup.html",
            "output/cover.csv",
            "output/ren",
            "output/spin"
        ])

        if knownDefaults.contains(outputBase.trimmingCharacters(in: .whitespacesAndNewlines)) {
            outputBase = command.defaultOutputBase
        }
    }

    func run() {
        guard !isRunning else { return }
        guard FileManager.default.isExecutableFile(atPath: runnerURL.path) else {
            status = "Missing runner at \(runnerURL.path)"
            return
        }
        let hasCoverFumens = command == .cover && !coverFumenCodes.isEmpty
        if command != .setup, inputSource == .fumen, !hasCoverFumens, fumenCode.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            status = "Export or enter a fumen code before running from Fumen"
            return
        }

        guard prepareInputsForRun() else { return }
        log = ""
        rawLog = ""
        isRunning = true
        status = "Running..."

        let args = buildArguments()
        let commandLine = "$ \(shellLine([runnerURL.path] + args))\n\n"
        if verboseOutput {
            rawLog = commandLine
            refreshDisplayedLog()
        }

        let process = Process()
        process.executableURL = runnerURL
        process.arguments = args
        process.currentDirectoryURL = workspaceURL

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe

        do {
            try process.run()
            runningProcess = process
        } catch {
            rawLog = "Failed to start process: \(error.localizedDescription)\n"
            refreshDisplayedLog()
            isRunning = false
            status = "Failed to start search"
            return
        }

        let started = Date()
        Task.detached(priority: .userInitiated) {
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            process.waitUntilExit()
            let output = String(data: data, encoding: .utf8) ?? ""
            let exitCode = process.terminationStatus
            await MainActor.run {
                guard self.runningProcess === process else { return }

                if self.verboseOutput {
                    self.rawLog += output
                    self.rawLog += "\n[exit code \(exitCode), \(String(format: "%.1f", Date().timeIntervalSince(started)))s]\n"
                } else {
                    self.rawLog = output
                }
                self.refreshDisplayedLog()
                self.runningProcess = nil
                self.isRunning = false
                if exitCode == 0 {
                    self.status = "Finished"
                } else if exitCode == 15 {
                    self.status = "Canceled"
                } else {
                    self.status = "Finished with an error"
                }
                self.refreshFiles()
            }
        }
    }

    private func prepareInputsForRun() -> Bool {
        if command == .setup {
            do {
                try FileManager.default.createDirectory(at: inputURL, withIntermediateDirectories: true)
                try normalized(patterns).write(to: patternsURL, atomically: true, encoding: .utf8)
                try setupFieldText().write(to: setupFieldURL, atomically: true, encoding: .utf8)
                return true
            } catch {
                status = "Setup input failed: \(error.localizedDescription)"
                return false
            }
        }

        saveInputs()
        return true
    }

    private func setupFieldText() throws -> String {
        saveCurrentFumenPage()
        let maxHeight = autoCommandHeight()
        isAutoUpdatingCommandHeight = true
        lines = String(maxHeight)
        isAutoUpdatingCommandHeight = false
        guard maxHeight <= 12 else {
            throw AppMessageError("setup field input supports heights up to 12")
        }

        var rows: [String] = []
        var hasFilledBlock = false

        for y in stride(from: maxHeight - 1, through: 0, by: -1) {
            let nativeRow = fumenSetupBottomRowExclusive - 1 - y
            var row = ""
            for column in 0..<10 {
                let value = nativeFumenCells[nativeRow * 10 + column]
                switch value {
                case 1:
                    row.append("*")
                    hasFilledBlock = true
                case 3:
                    row.append(".")
                case 8:
                    row.append("X")
                default:
                    row.append("_")
                }
            }
            rows.append(row)
        }

        guard hasFilledBlock else {
            throw AppMessageError("setup needs at least one I cell inside the selected height")
        }

        return ([String(maxHeight)] + rows).joined(separator: "\n") + "\n"
    }

    func cancelRun() {
        guard isRunning else { return }
        status = "Canceling..."
        runningProcess?.terminate()
    }

    func paintFumenCell(_ index: Int, value: Int) {
        guard nativeFumenCells.indices.contains(index) else { return }
        if fumenRowFill {
            let rowStart = (index / 10) * 10
            let openColumn = index % 10
            for offset in 0..<10 {
                nativeFumenCells[rowStart + offset] = offset == openColumn ? 0 : value
            }
        } else {
            nativeFumenCells[index] = value
        }
        saveCurrentFumenPage()
        updateCommandHeightIfNeeded()
    }

    func setFumenOperationPosition(_ index: Int) {
        guard (fumenVisibleTopRow * 10..<fumenVisibleBottomRow * 10).contains(index) else { return }
        if fumenOperation.type == 0 {
            fumenOperation.type = 1
        }
        fumenOperation.position = index
        fumenPlaceMino = true
        saveCurrentFumenPage()
    }

    func setFumenOperationType(_ type: Int) {
        fumenOperation.type = type
        fumenPlaceMino = type > 0
        saveCurrentFumenPage()
    }

    func rotateFumenOperation(_ delta: Int) {
        guard fumenOperation.type > 0 else { return }
        fumenOperation.rotation = (fumenOperation.rotation + delta + 4) % 4
        saveCurrentFumenPage()
    }

    func moveFumenOperation(dx: Int, dy: Int) {
        guard fumenOperation.type > 0 else { return }
        let x = fumenOperation.position % 10
        let y = fumenOperation.position / 10
        let nextX = min(max(0, x + dx), 9)
        let nextY = min(max(fumenVisibleTopRow, y + dy), fumenVisibleBottomRow - 1)
        fumenOperation.position = nextY * 10 + nextX
        saveCurrentFumenPage()
    }

    func clearFumenOperation() {
        fumenOperation = FumenOperation()
        fumenPlaceMino = false
        saveCurrentFumenPage()
    }

    func clearFumenCells() {
        nativeFumenCells = Array(repeating: 0, count: nativeFumenCells.count)
        saveCurrentFumenPage()
        updateCommandHeightIfNeeded()
    }

    func convertCurrentFumenToSetupGray() {
        var changed = false
        for index in nativeFumenCells.indices where nativeFumenCells[index] != 0 && nativeFumenCells[index] != 8 {
            nativeFumenCells[index] = 8
            changed = true
        }
        if changed {
            for pageIndex in fumenPages.indices {
                for cellIndex in fumenPages[pageIndex].indices where fumenPages[pageIndex][cellIndex] != 0 && fumenPages[pageIndex][cellIndex] != 8 {
                    fumenPages[pageIndex][cellIndex] = 8
                }
            }
        }
    }

    func replaceCurrentFumenCells(_ cells: [Int]) {
        var normalized = Array(cells.prefix(240))
        if normalized.count < 240 {
            normalized += Array(repeating: 0, count: 240 - normalized.count)
        }
        for index in 230..<240 {
            normalized[index] = 0
        }
        nativeFumenCells = normalized
        clearFumenOperation()
        saveCurrentFumenPage()
        updateCommandHeightIfNeeded()
    }

    func shiftFumenCells(dx: Int, dy: Int) {
        var shifted = Array(repeating: 0, count: nativeFumenCells.count)
        for row in 0..<24 {
            for column in 0..<10 {
                let sourceRow = row - dy
                let sourceColumn = column - dx
                guard (0..<24).contains(sourceRow), (0..<10).contains(sourceColumn) else { continue }
                shifted[row * 10 + column] = nativeFumenCells[sourceRow * 10 + sourceColumn]
            }
        }
        nativeFumenCells = shifted
        saveCurrentFumenPage()
        updateCommandHeightIfNeeded()
    }

    func mirrorFumenCells() {
        var mirrored = nativeFumenCells
        for row in 0..<24 {
            for column in 0..<10 {
                let sourceIndex = row * 10 + column
                let targetIndex = row * 10 + (9 - column)
                mirrored[targetIndex] = mirroredFumenValue(nativeFumenCells[sourceIndex])
            }
        }
        nativeFumenCells = mirrored
        fumenOperation = mirroredFumenOperation(fumenOperation)
        saveCurrentFumenPage()
        updateCommandHeightIfNeeded()
    }

    func mirrorSetupCells() {
        var mirrored = nativeFumenCells
        for row in 0..<24 {
            for column in 0..<10 {
                let sourceIndex = row * 10 + column
                let targetIndex = row * 10 + (9 - column)
                mirrored[targetIndex] = nativeFumenCells[sourceIndex]
            }
        }
        nativeFumenCells = mirrored
        saveCurrentFumenPage()
        updateCommandHeightIfNeeded()
    }

    func solverFumenCells() -> [Int] {
        var cells = nativeFumenCells
        for index in 230..<240 {
            cells[index] = 0
        }
        return cells
    }

    func solverFumenPages() -> [[Int]] {
        saveCurrentFumenPage()
        return fumenPages.map { page in
            var cells = page
            for index in 230..<240 {
                cells[index] = 0
            }
            return cells
        }
    }

    func replaceFumenPages(_ pages: [[Int]], comments: [String], operations: [FumenOperation], selectedPage: Int = 0) {
        let normalizedPages = pages.isEmpty ? [Array(repeating: 0, count: 240)] : pages.map { page in
            var cells = Array(page.prefix(240))
            if cells.count < 240 {
                cells += Array(repeating: 0, count: 240 - cells.count)
            }
            for index in 230..<240 {
                cells[index] = 0
            }
            return cells
        }
        fumenPages = normalizedPages
        fumenComments = normalizedPages.indices.map { index in
            index < comments.count ? comments[index] : ""
        }
        fumenOperations = normalizedPages.indices.map { index in
            index < operations.count ? operations[index] : FumenOperation()
        }
        currentFumenPage = min(max(0, selectedPage), normalizedPages.count - 1)
        nativeFumenCells = fumenPages[currentFumenPage]
        fumenComment = fumenComments[currentFumenPage]
        fumenOperation = fumenOperations[currentFumenPage]
        fumenPlaceMino = fumenOperation.type > 0
        updateCommandHeightIfNeeded()
    }

    func saveCurrentFumenPage() {
        guard fumenPages.indices.contains(currentFumenPage) else { return }
        var cells = nativeFumenCells
        for index in 230..<240 {
            cells[index] = 0
        }
        fumenPages[currentFumenPage] = cells
        fumenComments[currentFumenPage] = fumenComment
        fumenOperations[currentFumenPage] = fumenPlaceMino ? fumenOperation : FumenOperation()
    }

    func goToFumenPage(_ page: Int) {
        guard fumenPages.indices.contains(page) else { return }
        saveCurrentFumenPage()
        currentFumenPage = page
        nativeFumenCells = fumenPages[page]
        fumenComment = fumenComments[page]
        fumenOperation = fumenOperations[page]
        fumenPlaceMino = fumenOperation.type > 0
        updateCommandHeightIfNeeded()
    }

    func addFumenPage() {
        saveCurrentFumenPage()
        let insertionIndex = currentFumenPage + 1
        fumenPages.insert(fumenCellsAfterLockAndLineClear(), at: insertionIndex)
        fumenComments.insert("", at: insertionIndex)
        fumenOperations.insert(FumenOperation(), at: insertionIndex)
        goToFumenPage(insertionIndex)
    }

    private func fumenCellsAfterLockAndLineClear() -> [Int] {
        var cells = nativeFumenCells

        if fumenPlaceMino, fumenOperation.type > 0 {
            for index in fumenOperationCells(fumenOperation) where cells.indices.contains(index) {
                cells[index] = fumenOperation.type
            }
        }

        return lineClearedFumenCells(cells)
    }

    func deleteFollowingFumenPages() {
        saveCurrentFumenPage()
        guard currentFumenPage + 1 < fumenPages.count else { return }
        fumenPages.removeSubrange((currentFumenPage + 1)..<fumenPages.count)
        fumenComments.removeSubrange((currentFumenPage + 1)..<fumenComments.count)
        fumenOperations.removeSubrange((currentFumenPage + 1)..<fumenOperations.count)
        updateCommandHeightIfNeeded()
    }

    func deletePreviousFumenPages() {
        saveCurrentFumenPage()
        guard currentFumenPage > 0 else { return }
        fumenPages.removeSubrange(0..<currentFumenPage)
        fumenComments.removeSubrange(0..<currentFumenPage)
        fumenOperations.removeSubrange(0..<currentFumenPage)
        currentFumenPage = 0
        nativeFumenCells = fumenPages[currentFumenPage]
        fumenComment = fumenComments[currentFumenPage]
        fumenOperation = fumenOperations[currentFumenPage]
        fumenPlaceMino = fumenOperation.type > 0
        updateCommandHeightIfNeeded()
    }

    var fumenPageLabel: String {
        "\(currentFumenPage + 1)/\(fumenPages.count)"
    }

    func replacePreviewFumenPages(_ pages: [[Int]], comments: [String], operations: [FumenOperation], selectedPage: Int = 0) {
        let normalizedPages = pages.isEmpty ? [Array(repeating: 0, count: 240)] : pages.map { page in
            var cells = Array(page.prefix(240))
            if cells.count < 240 {
                cells += Array(repeating: 0, count: 240 - cells.count)
            }
            for index in 230..<240 {
                cells[index] = 0
            }
            return cells
        }
        previewFumenPages = normalizedPages
        previewFumenComments = normalizedPages.indices.map { index in
            index < comments.count ? comments[index] : ""
        }
        previewFumenOperations = normalizedPages.indices.map { index in
            index < operations.count ? operations[index] : FumenOperation()
        }
        currentPreviewFumenPage = min(max(0, selectedPage), normalizedPages.count - 1)
    }

    func goToPreviewFumenPage(_ page: Int) {
        guard previewFumenPages.indices.contains(page) else { return }
        currentPreviewFumenPage = page
    }

    var previewFumenPageLabel: String {
        "\(currentPreviewFumenPage + 1)/\(previewFumenPages.count)"
    }

    func sendPreviewToEditor() {
        guard !previewFumenCode.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            status = "No preview to send"
            return
        }
        fumenCode = previewFumenCode
        replaceFumenPages(
            previewFumenPages,
            comments: previewFumenComments,
            operations: previewFumenOperations,
            selectedPage: currentPreviewFumenPage
        )
        updateCommandHeightIfNeeded()
        inputSource = .fumen
        fumenPanelTab = .editor
        status = "Sent preview to editor"
    }

    private func terminateRunningProcess() {
        if runningProcess?.isRunning == true {
            runningProcess?.terminate()
        }
        runningProcess = nil
    }

    func refreshFiles() {
        let urls = (try? FileManager.default.contentsOfDirectory(
            at: outputURL,
            includingPropertiesForKeys: [.contentModificationDateKey, .fileSizeKey],
            options: [.skipsHiddenFiles]
        )) ?? []

        outputFiles = urls.compactMap { url in
            guard let values = try? url.resourceValues(forKeys: [.contentModificationDateKey, .fileSizeKey]),
                  let size = values.fileSize,
                  let modified = values.contentModificationDate
            else { return nil }
            return OutputFile(url: url, size: Int64(size), modified: modified)
        }
        .sorted { $0.modified > $1.modified }
    }

    func preview(_ file: OutputFile) {
        selectedOutputFile = file
        if let text = try? String(contentsOf: file.url, encoding: .utf8) {
            rawLog = ""
            log = String(text.prefix(180_000))
            if text.count > 180_000 {
                log += "\n\n[preview truncated]\n"
            }
            status = "Previewing \(file.name)"
        } else {
            status = "Cannot preview \(file.name)"
        }
    }

    func open(_ file: OutputFile) {
        let fileExtension = file.url.pathExtension.lowercased()
        if fileExtension == "html" || fileExtension == "htm" {
            embeddedOutputURL = file.url
            fumenPanelTab = .output
            selectedOutputFile = file
            status = "Viewing \(file.name)"
        } else if ["txt", "csv", "tsv", "log"].contains(fileExtension) {
            preview(file)
        } else {
            NSWorkspace.shared.open(file.url)
        }
    }

    func openOutputFolder() {
        NSWorkspace.shared.open(outputURL)
    }

    private func buildArguments() -> [String] {
        var args = [command.executableCommand]
        let userExtraArgs = splitArguments(extraArgs)

        if command == .spin {
            if !userExtraArgs.contains("-fb") && !userExtraArgs.contains("--fill-bottom") {
                args += ["-fb", "0"]
            }
            if !userExtraArgs.contains("-ft") && !userExtraArgs.contains("--fill-top") {
                args += ["-ft", String(autoCommandHeight())]
            }
        }

        args += userExtraArgs

        switch inputSource {
        case .field:
            args += ["-fp", fieldURL.path]
        case .fumen:
            if command == .setup {
                args += ["-fp", setupFieldURL.path]
            } else if command == .cover {
                let codes = coverFumenCodes.isEmpty
                    ? [fumenCode.trimmingCharacters(in: .whitespacesAndNewlines)]
                    : coverFumenCodes.map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }.filter { !$0.isEmpty }
                if !codes.isEmpty {
                    args += ["-t"] + codes
                }
            } else {
                args += ["-t", fumenCode.trimmingCharacters(in: .whitespacesAndNewlines)]
            }
        }

        args += ["-pp", patternsURL.path]

        if command.supportsHoldDropKicks {
            args += [
                "-H", hold.rawValue,
                "-d", drop.rawValue,
                "-K", kicks
            ]
        }

        if command == .tetrisPath {
            args += ["-sc", "tetris-end"]
        }

        if command == .cover {
            if !coverMode.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                args += ["-M", coverMode]
            }
            if !coverSort.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                args += ["-s", coverSort]
            }
            args += ["-m", coverMirror ? "yes" : "no"]
            args += ["-a", coverAccum ? "yes" : "no"]
            args += ["-P", coverPriority ? "yes" : "no"]
        }

        if command == .spin {
            if !spinFilter.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                args += ["-f", spinFilter]
            }
            args += ["-r", spinRoof ? "true" : "false"]
            args += ["-s", spinSplit ? "true" : "false"]
        }

        if command.supportsThreads, !threads.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            args += ["-th", threads]
        }

        if command.supportsLines, !lines.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            args += [command == .setup ? "-l" : "-c", lines]
        }

        if command.supportsOutputBase {
            if !outputBase.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                args += [command == .tetris ? "-lp" : "-o", normalizedOutputBaseForRun()]
            }
        }

        if command.supportsFormat {
            if !format.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                args += [command == .setup || command == .spin ? "-fo" : "-f", format]
            }
        }

        return args
    }

    private func normalizedOutputBaseForRun() -> String {
        let trimmed = outputBase.trimmingCharacters(in: .whitespacesAndNewlines)
        guard command == .setup, format == "html" else { return trimmed }
        let url = URL(fileURLWithPath: trimmed)
        return url.pathExtension.isEmpty ? trimmed + ".html" : trimmed
    }

    func clearOutput() {
        rawLog = ""
        log = ""
    }

    private func refreshDisplayedLog() {
        log = verboseOutput ? rawLog : compactOutput(from: rawLog)
    }

    private func compactOutput(from output: String) -> String {
        let lines = output.components(separatedBy: .newlines)
        var compact: [String] = []
        var includeSearchSection = false
        var includeOutputSection = false

        for line in lines {
            let trimmedLine = line.trimmingCharacters(in: .whitespaces)
            if line.hasPrefix("Searching pattern size") {
                compact.append(line)
                continue
            }

            if trimmedLine == "# Setup Field" || trimmedLine == "# Initialize / User-defined" {
                if !compact.isEmpty && compact.last != "" {
                    compact.append("")
                }
                compact.append(line)
                continue
            }

            if isCompactResultLine(trimmedLine) {
                compact.append(line)
                continue
            }

            if line == "# Search" {
                if !compact.isEmpty && compact.last != "" {
                    compact.append("")
                }
                compact.append(line)
                includeSearchSection = true
                includeOutputSection = false
                continue
            }

            if line == "# Output" {
                if !compact.isEmpty && compact.last != "" {
                    compact.append("")
                }
                compact.append(line)
                includeSearchSection = false
                includeOutputSection = true
                continue
            }

            if includeSearchSection {
                if line.hasPrefix("  -> Stopwatch") {
                    compact.append(line)
                    continue
                }

                if line.hasPrefix("# ") {
                    includeSearchSection = false
                }
            }

            if includeOutputSection {
                if line.hasPrefix("# ") || line.hasPrefix("Success pattern tree") || line.hasPrefix("Tetris-ending PC pattern tree") || line == "-------------------" {
                    includeOutputSection = false
                }
            }
        }

        return compact.joined(separator: "\n").trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func isCompactResultLine(_ line: String) -> Bool {
        line.hasPrefix("success = ")
            || line.hasPrefix("tetris = ")
            || line.hasPrefix("Found solutions = ")
            || line.hasPrefix("Found path ")
            || line.hasPrefix("Found path[")
            || line.hasPrefix("Found pattern ")
            || line == "success:"
            || line.hasPrefix("OR  = ")
            || line.hasPrefix("AND = ")
            || line == ">>>"
            || line.range(of: #"^\d+(\.\d+)? % \[\d+/\d+\]:"#,
                          options: .regularExpression) != nil
    }

    private func normalized(_ value: String) -> String {
        value.trimmingCharacters(in: .newlines) + "\n"
    }
}

struct ProcessResult {
    let output: String
    let exitCode: Int32
}

enum ProcessRunner {
    static func run(executableURL: URL, arguments: [String], workingDirectory: URL) async -> ProcessResult {
        let process = Process()
        process.executableURL = executableURL
        process.arguments = arguments
        process.currentDirectoryURL = workingDirectory

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe

        do {
            try process.run()
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            process.waitUntilExit()
            let output = String(data: data, encoding: .utf8) ?? ""
            return ProcessResult(output: output, exitCode: process.terminationStatus)
        } catch {
            return ProcessResult(output: "Failed to start process: \(error.localizedDescription)\n", exitCode: 127)
        }
    }
}

enum WorkspaceLocator {
    static func runtimeSupportDirectory() -> URL {
        let fileManager = FileManager.default
        if let applicationSupport = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask).first {
            return applicationSupport.appendingPathComponent("Solution Finder Enhanced", isDirectory: true)
        }

        return URL(fileURLWithPath: fileManager.currentDirectoryPath)
            .appendingPathComponent("Solution Finder Enhanced Data", isDirectory: true)
    }

    static func prepareRuntimeSupport(at url: URL) {
        let fileManager = FileManager.default
        do {
            try fileManager.createDirectory(at: url, withIntermediateDirectories: true)
            try fileManager.createDirectory(at: url.appendingPathComponent("input"), withIntermediateDirectories: true)
            try fileManager.createDirectory(at: url.appendingPathComponent("output"), withIntermediateDirectories: true)

            if let bundledSolver = Bundle.main.url(forResource: "solution-finder-1.43", withExtension: nil) {
                copyDirectoryIfMissing(
                    from: bundledSolver.appendingPathComponent("kicks"),
                    to: url.appendingPathComponent("kicks")
                )
                copyDirectoryIfMissing(
                    from: bundledSolver.appendingPathComponent("theme"),
                    to: url.appendingPathComponent("theme")
                )
                copyDirectoryIfMissing(
                    from: bundledSolver.appendingPathComponent("samples"),
                    to: url.appendingPathComponent("samples")
                )
                copyFileIfMissing(
                    from: bundledSolver.appendingPathComponent("input/field.txt"),
                    to: url.appendingPathComponent("input/field.txt")
                )
                copyFileIfMissing(
                    from: bundledSolver.appendingPathComponent("input/patterns.txt"),
                    to: url.appendingPathComponent("input/patterns.txt")
                )
            }
        } catch {
            NSLog("Failed to prepare Solution Finder Enhanced app data: \(error.localizedDescription)")
        }
    }

    static func copyDirectoryIfMissing(from source: URL, to destination: URL) {
        let fileManager = FileManager.default
        guard fileManager.fileExists(atPath: source.path),
              !fileManager.fileExists(atPath: destination.path) else { return }
        try? fileManager.copyItem(at: source, to: destination)
    }

    static func copyFileIfMissing(from source: URL, to destination: URL) {
        let fileManager = FileManager.default
        guard fileManager.fileExists(atPath: source.path),
              !fileManager.fileExists(atPath: destination.path) else { return }
        try? fileManager.createDirectory(at: destination.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? fileManager.copyItem(at: source, to: destination)
    }

    static func findRunnerURL(workspaceURL: URL) -> URL {
        if let bundled = Bundle.main.url(forResource: "sfinder", withExtension: nil, subdirectory: "native-macos/bin") {
            return bundled
        }

        let bundledFallback = Bundle.main.bundleURL
            .appendingPathComponent("Contents/Resources/native-macos/bin/sfinder")
        if FileManager.default.isExecutableFile(atPath: bundledFallback.path) {
            return bundledFallback
        }

        return findWorkspace().appendingPathComponent("native-macos/bin/sfinder")
    }

    static func findWorkspace() -> URL {
        let fileManager = FileManager.default
        var candidates: [URL] = []

        candidates.append(URL(fileURLWithPath: fileManager.currentDirectoryPath))

        let bundleURL = Bundle.main.bundleURL
        candidates.append(bundleURL.deletingLastPathComponent())
        candidates.append(bundleURL.deletingLastPathComponent().deletingLastPathComponent())
        candidates.append(bundleURL.deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent())

        let executable = URL(fileURLWithPath: CommandLine.arguments[0]).standardizedFileURL
        candidates.append(executable.deletingLastPathComponent())
        candidates.append(executable.deletingLastPathComponent().deletingLastPathComponent())
        candidates.append(executable.deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent())

        for candidate in candidates {
            let runner = candidate.appendingPathComponent("native-macos/bin/sfinder")
            let input = candidate.appendingPathComponent("solution-finder-1.43/input")
            if fileManager.fileExists(atPath: runner.path), fileManager.fileExists(atPath: input.path) {
                return candidate
            }
        }

        return URL(fileURLWithPath: fileManager.currentDirectoryPath)
    }

    static func findFumenHTML(workspaceURL: URL) -> URL {
        if let bundled = Bundle.main.url(forResource: "fumen_en", withExtension: "html") {
            return bundled
        }

        let english = workspaceURL.appendingPathComponent("fumen_en.html")
        if FileManager.default.fileExists(atPath: english.path) {
            return english
        }

        if let bundled = Bundle.main.url(forResource: "fumen", withExtension: "html") {
            return bundled
        }

        return workspaceURL.appendingPathComponent("fumen.html")
    }
}

struct FumenPanel: View {
    @ObservedObject var model: AppModel
    @State private var webView = WKWebView()

    var body: some View {
        VStack(spacing: 0) {
            HStack(alignment: .top, spacing: 12) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Fumen Editor")
                        .font(.system(size: 22, weight: .bold))
                    Text(model.fumenURL.path)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }

                Spacer()

                Button {
                    reloadFumen()
                } label: {
                    Label("Reload", systemImage: "arrow.clockwise")
                }

                Button {
                    importCode()
                } label: {
                    Label("Import Code", systemImage: "square.and.arrow.down")
                }

                Button {
                    exportCode()
                } label: {
                    Label("Export Code", systemImage: "square.and.arrow.up")
                }

                Button {
                    exportCode {
                        model.inputSource = .fumen
                        model.status = "Fumen is ready as solver input"
                    }
                } label: {
                    Label("Use in Solver", systemImage: "arrowshape.turn.up.left")
                }

                Button {
                    exportCode {
                        model.inputSource = .fumen
                        model.run()
                    }
                } label: {
                    Label("Run Solver", systemImage: "play.fill")
                }
                .buttonStyle(.borderedProminent)
                .disabled(model.isRunning)
            }
            .padding(.horizontal, 18)
            .padding(.vertical, 12)

            Divider()

            HSplitView {
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("Fumen Code")
                            .font(.headline)
                        Spacer()
                        Button {
                            NSPasteboard.general.clearContents()
                            NSPasteboard.general.setString(model.fumenCode, forType: .string)
                            model.status = "Fumen code copied"
                        } label: {
                            Label("Copy", systemImage: "doc.on.doc")
                        }
                    }

                    TextEditor(text: $model.fumenCode)
                        .font(.system(.body, design: .monospaced))
                        .frame(minWidth: 260)
                        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))

                    Text("Paste a v115@... code here, import it into the editor, then export after making changes.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .padding(14)
                .frame(minWidth: 300, idealWidth: 360)

                WebView(webView: webView, url: model.fumenURL)
                    .frame(minWidth: 640)
            }

            Divider()

            HStack {
                Text(model.status)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                Spacer()
                Button {
                    NSWorkspace.shared.open(model.fumenURL)
                } label: {
                    Label("Open HTML File", systemImage: "doc")
                }
            }
            .padding(.horizontal, 18)
            .padding(.vertical, 10)
        }
        .onAppear {
            if webView.url == nil {
                reloadFumen()
            }
        }
    }

    private func reloadFumen() {
        webView.loadFileURL(model.fumenURL, allowingReadAccessTo: model.fumenURL.deletingLastPathComponent())
        model.status = "Fumen editor loaded"
    }

    private func importCode() {
        let code = model.fumenCode.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !code.isEmpty else {
            model.status = "No fumen code to import"
            return
        }

        let js = """
        (function() {
          var tx = document.getElementById('tx');
          if (!tx) return 'Fumen input field was not found';
          tx.value = \(javascriptStringLiteral(code));
          if (typeof versioncheck === 'function') versioncheck(0);
          return 'Imported fumen code';
        })();
        """

        webView.evaluateJavaScript(js) { value, error in
            DispatchQueue.main.async {
                if let error {
                    model.status = "Import failed: \(error.localizedDescription)"
                } else {
                    model.status = (value as? String) ?? "Imported fumen code"
                }
            }
        }
    }

    private func exportCode(completion: (() -> Void)? = nil) {
        let js = """
        (function() {
          var tx = document.getElementById('tx');
          if (!tx) return '';
          if (typeof encode === 'function') encode(0);
          return tx.value || '';
        })();
        """

        webView.evaluateJavaScript(js) { value, error in
            DispatchQueue.main.async {
                if let error {
                    model.status = "Export failed: \(error.localizedDescription)"
                    return
                }

                model.fumenCode = value as? String ?? ""
                model.status = model.fumenCode.isEmpty ? "No fumen code exported" : "Fumen code exported"
                if !model.fumenCode.isEmpty {
                    completion?()
                }
            }
        }
    }
}

struct WebView: NSViewRepresentable {
    let webView: WKWebView
    let url: URL
    var injectedCSS: String? = nil
    var onFumenLink: ((String) -> Void)? = nil

    func makeCoordinator() -> Coordinator {
        Coordinator(injectedCSS: injectedCSS, onFumenLink: onFumenLink)
    }

    func makeNSView(context: Context) -> WKWebView {
        webView.configuration.preferences.javaScriptCanOpenWindowsAutomatically = true
        webView.navigationDelegate = context.coordinator
        webView.uiDelegate = context.coordinator
        webView.setValue(false, forKey: "drawsBackground")
        webView.loadFileURL(url, allowingReadAccessTo: url.deletingLastPathComponent())
        return webView
    }

    func updateNSView(_ nsView: WKWebView, context: Context) {
        context.coordinator.injectedCSS = injectedCSS
        context.coordinator.onFumenLink = onFumenLink
        if nsView.url?.standardizedFileURL != url.standardizedFileURL {
            nsView.loadFileURL(url, allowingReadAccessTo: url.deletingLastPathComponent())
        }
    }

    final class Coordinator: NSObject, WKNavigationDelegate, WKUIDelegate {
        var injectedCSS: String?
        var onFumenLink: ((String) -> Void)?

        init(injectedCSS: String?, onFumenLink: ((String) -> Void)?) {
            self.injectedCSS = injectedCSS
            self.onFumenLink = onFumenLink
        }

        func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, decisionHandler: @escaping @MainActor @Sendable (WKNavigationActionPolicy) -> Void) {
            if let onFumenLink,
               let url = navigationAction.request.url,
               let code = fumenCode(from: url) {
                DispatchQueue.main.async {
                    onFumenLink(code)
                }
                decisionHandler(.cancel)
                return
            }
            decisionHandler(.allow)
        }

        func webView(_ webView: WKWebView, createWebViewWith configuration: WKWebViewConfiguration, for navigationAction: WKNavigationAction, windowFeatures: WKWindowFeatures) -> WKWebView? {
            if let onFumenLink,
               let url = navigationAction.request.url,
               let code = fumenCode(from: url) {
                DispatchQueue.main.async {
                    onFumenLink(code)
                }
            }
            return nil
        }

        func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
            guard let injectedCSS, !injectedCSS.isEmpty else { return }
            let script = """
            (function() {
                var existing = document.getElementById('solution-finder-output-style');
                if (existing) existing.remove();
                var style = document.createElement('style');
                style.id = 'solution-finder-output-style';
                style.textContent = \(javascriptStringLiteral(injectedCSS));
                document.head.appendChild(style);
            })();
            """
            webView.evaluateJavaScript(script)
        }
    }
}

struct OpenerImporterView: View {
    @StateObject private var model = OpenerImporterModel()
    @State private var webView = WKWebView()

    private let boardCellSize: CGFloat = 18
    private var boardColumns: [GridItem] {
        Array(repeating: GridItem(.fixed(boardCellSize), spacing: 2), count: 10)
    }

    var body: some View {
        GeometryReader { proxy in
            Group {
                if proxy.size.width < 760 {
                    ScrollView {
                        VStack(alignment: .leading, spacing: 12) {
                            openerImporterEditorContent
                            Divider()
                            openerImporterInfoContent
                        }
                        .padding(14)
                        .frame(maxWidth: .infinity, alignment: .topLeading)
                    }
                } else {
                    HSplitView {
                        openerImporterEditorPane
                            .frame(minWidth: 320, idealWidth: 560, maxWidth: .infinity)
                        openerImporterInfoPane
                            .frame(minWidth: 260, idealWidth: 360, maxWidth: .infinity)
                    }
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .background(Color(nsColor: .windowBackgroundColor))
        .background(
            WebView(webView: webView, url: model.fumenURL)
                .frame(width: 1, height: 1)
                .opacity(0.01)
                .accessibilityHidden(true)
        )
        .onAppear {
            if webView.url == nil {
                webView.loadFileURL(model.fumenURL, allowingReadAccessTo: model.fumenURL.deletingLastPathComponent())
            }
        }
    }

    private var openerImporterEditorPane: some View {
        ScrollView {
            openerImporterEditorContent
                .padding(14)
                .frame(maxWidth: .infinity, alignment: .topLeading)
        }
    }

    private var openerImporterEditorContent: some View {
        VStack(alignment: .leading, spacing: 12) {
            ViewThatFits(in: .horizontal) {
                HStack {
                    openerImporterTitle
                    Spacer()
                    openerImporterToolbar
                }
                VStack(alignment: .leading, spacing: 8) {
                    openerImporterTitle
                    openerImporterToolbar
                }
            }

            GroupBox("Opener") {
                VStack(alignment: .leading, spacing: 8) {
                    TextField("Opener name", text: $model.openerName)
                        .textFieldStyle(.roundedBorder)
                    Text("Example: TKI 3, DT Cannon, Gassho TSD")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Toggle("Detect variant after 6 pieces", isOn: $model.earlyVariantDetection)
                        .font(.caption)
                    ViewThatFits(in: .horizontal) {
                        HStack {
                            addUpdateOpenerButton

                            Text("\(model.queuedOpeners.count) groups in book")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                            Spacer()
                        }
                        VStack(alignment: .leading, spacing: 6) {
                            addUpdateOpenerButton
                            Text("\(model.queuedOpeners.count) groups in book")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    }
                }
                .padding(.vertical, 4)
            }

            GroupBox("Base Fumen") {
                VStack(alignment: .leading, spacing: 8) {
                    TextEditor(text: $model.baseCode)
                        .font(.system(.body, design: .monospaced))
                        .frame(height: 72)
                        .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color(nsColor: .separatorColor)))
                    ViewThatFits(in: .horizontal) {
                        HStack {
                            baseFumenButtons
                            Spacer()
                        }
                        VStack(alignment: .leading, spacing: 6) {
                            baseFumenButtons
                        }
                    }
                }
                .padding(.vertical, 4)
            }

            HStack {
                Text("Variations")
                    .font(.headline)
                Spacer()
                Button {
                    model.addVariation()
                } label: {
                    Label("Add Variation", systemImage: "plus")
                }
            }

            VStack(alignment: .leading, spacing: 10) {
                ForEach($model.variations) { $variation in
                    variationRow($variation)
                }
            }
        }
    }

    private var openerImporterInfoPane: some View {
        ScrollView {
            openerImporterInfoContent
                .padding(14)
                .frame(maxWidth: .infinity, alignment: .topLeading)
        }
    }

    private var openerImporterInfoContent: some View {
        VStack(alignment: .leading, spacing: 12) {
            GroupBox("Preview") {
                VStack(alignment: .leading, spacing: 10) {
                    Picker("Source", selection: $model.selectedPreviewID) {
                        Text("Base").tag("base")
                        ForEach(model.variations) { variation in
                            Text(variation.name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? "Unnamed variation" : variation.name)
                                .tag(variation.id.uuidString)
                        }
                    }
                    .pickerStyle(.menu)

                    HStack {
                        Text("Page \(model.selectedPreviewPageIndex + 1)/\(max(model.selectedPreviewPages.count, 1))")
                            .font(.headline.monospacedDigit())
                        Spacer()
                    }

                    OpenerBoardPreview(cells: model.selectedPreviewCells, cellSize: boardCellSize)
                }
                .padding(.vertical, 4)
            }

            GroupBox("Opener Book") {
                VStack(alignment: .leading, spacing: 8) {
                    if model.queuedOpeners.isEmpty {
                        Text("No opener groups loaded.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(model.queuedOpeners) { opener in
                            openerBookRow(opener)
                        }
                    }
                }
                .padding(.vertical, 4)
                .frame(maxWidth: .infinity, alignment: .leading)
            }

            GroupBox("Checks") {
                VStack(alignment: .leading, spacing: 6) {
                    ForEach(model.validationMessages, id: \.self) { message in
                        HStack(alignment: .top, spacing: 6) {
                            Image(systemName: message == "Ready to export." ? "checkmark.circle.fill" : "exclamationmark.triangle.fill")
                                .foregroundStyle(message == "Ready to export." ? Color.green : Color.yellow)
                            Text(message)
                                .font(.caption)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                    }
                }
                .padding(.vertical, 4)
                .frame(maxWidth: .infinity, alignment: .leading)
            }

            Text(model.status)
                .font(.caption)
                .foregroundStyle(.secondary)
                .lineLimit(3)
        }
    }

    private var openerImporterTitle: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text("Opener Importer")
                .font(.system(size: 22, weight: .bold))
            Text("Edit the opener book without touching JSON by hand.")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private var openerImporterToolbar: some View {
        Group {
            Button {
                model.loadBook()
            } label: {
                Label("Load Book", systemImage: "arrow.clockwise")
            }
            Button {
                model.newOpenerGroup()
            } label: {
                Label("New Group", systemImage: "plus")
            }
            Button {
                model.exportJSON()
            } label: {
                Label("Save Book", systemImage: "square.and.arrow.up")
            }
            .buttonStyle(.borderedProminent)
            .disabled(!model.canExport)
        }
    }

    private var addUpdateOpenerButton: some View {
        Button {
            model.queueCurrentOpener()
        } label: {
            Label("Add/Update Group", systemImage: "tray.and.arrow.down")
        }
        .disabled(!model.canQueueCurrentOpener)
    }

    private var baseFumenButtons: some View {
        Group {
            Button {
                importBaseScreenshot()
            } label: {
                Label("Screenshot Base", systemImage: "camera.viewfinder")
            }
            Button {
                decodeBase()
            } label: {
                Label("Decode Base", systemImage: "square.and.arrow.down")
            }
            pageStepper(page: $model.baseSelectedPage, pageCount: model.basePages.count)
            Text(model.baseStatus)
                .font(.caption)
                .foregroundStyle(.secondary)
                .lineLimit(1)
        }
    }

    private func openerBookRow(_ opener: OpenerImportRecord) -> some View {
        ViewThatFits(in: .horizontal) {
            HStack(spacing: 8) {
                openerBookLabel(opener)
                Spacer()
                openerBookActions(opener)
            }
            VStack(alignment: .leading, spacing: 6) {
                openerBookLabel(opener)
                HStack {
                    Spacer()
                    openerBookActions(opener)
                }
            }
        }
        .padding(.vertical, 2)
    }

    private func openerBookLabel(_ opener: OpenerImportRecord) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(opener.name)
                .font(.subheadline.weight(.semibold))
                .lineLimit(1)
            Text("\(1 + opener.variations.count) fumen code\(opener.variations.count == 0 ? "" : "s")\(opener.earlyVariantDetection == true ? " - variant at 6" : "")")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private func openerBookActions(_ opener: OpenerImportRecord) -> some View {
        Group {
            Button {
                model.editQueuedOpener(opener)
            } label: {
                Image(systemName: "pencil")
            }
            .buttonStyle(.borderless)
            Button(role: .destructive) {
                model.removeQueuedOpener(opener)
            } label: {
                Image(systemName: "trash")
            }
            .buttonStyle(.borderless)
        }
    }

    @ViewBuilder
    private func pageStepper(page: Binding<Int>, pageCount: Int) -> some View {
        Stepper("Page \(min(page.wrappedValue + 1, max(pageCount, 1)))/\(max(pageCount, 1))", value: page, in: 0...max(pageCount - 1, 0))
            .disabled(pageCount <= 1)
            .frame(width: 130)
    }

    private func variationRow(_ variation: Binding<OpenerVariationDraft>) -> some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 8) {
                ViewThatFits(in: .horizontal) {
                    HStack(spacing: 8) {
                        variationHeaderControls(variation)
                    }
                    VStack(alignment: .leading, spacing: 8) {
                        variationHeaderControls(variation)
                    }
                }

                TextEditor(text: variation.code)
                    .font(.system(.body, design: .monospaced))
                    .frame(height: 58)
                    .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color(nsColor: .separatorColor)))

                HStack {
                    pageStepper(page: variation.selectedPage, pageCount: variation.wrappedValue.pages.count)
                    Text(variation.wrappedValue.status)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Button {
                        model.selectedPreviewID = variation.wrappedValue.id.uuidString
                    } label: {
                        Label("Preview", systemImage: "eye")
                    }
                }
            }
            .padding(.vertical, 4)
        }
    }

    private func variationHeaderControls(_ variation: Binding<OpenerVariationDraft>) -> some View {
        Group {
            TextField("Variation name", text: variation.name)
                .textFieldStyle(.roundedBorder)
            Button {
                decodeVariation(id: variation.wrappedValue.id, code: variation.wrappedValue.code)
            } label: {
                Label("Decode", systemImage: "square.and.arrow.down")
            }
            Button {
                importVariationScreenshot(id: variation.wrappedValue.id)
            } label: {
                Label("Screenshot", systemImage: "camera.viewfinder")
            }
            Button(role: .destructive) {
                model.removeVariation(id: variation.wrappedValue.id)
            } label: {
                Label("Remove", systemImage: "trash")
            }
        }
    }

    private func decodeBase() {
        model.status = "Decoding base..."
        decodeFumenCode(model.baseCode) { pages in
            model.setBasePages(pages)
            model.selectedPreviewID = "base"
            model.status = model.baseStatus
        }
    }

    private func decodeVariation(id: UUID, code: String) {
        model.status = "Decoding variation..."
        decodeFumenCode(code) { pages in
            model.setVariationPages(id: id, pages: pages)
            model.selectedPreviewID = id.uuidString
            if let variation = model.variations.first(where: { $0.id == id }) {
                model.status = variation.status
            }
        }
    }

    private func importBaseScreenshot() {
        captureBoardScreenshot(prompt: "Drag around the opener base board") { cells in
            encodeOnePageFumen(cells: cells) { code in
                guard let code else { return }
                model.setBaseCode(code, pages: [cells])
                model.status = "Imported base screenshot"
            }
        }
    }

    private func importVariationScreenshot(id: UUID) {
        captureBoardScreenshot(prompt: "Drag around the opener variation board") { cells in
            let maskedCells = variationCellsWithGrayBase(cells)
            encodeOnePageFumen(cells: maskedCells) { code in
                guard let code else { return }
                model.setVariationCode(id: id, code: code, pages: [maskedCells])
                if maskedCells != cells {
                    model.status = "Imported variation screenshot; base cells were marked gray"
                } else {
                    model.status = "Imported variation screenshot"
                }
            }
        }
    }

    private func captureBoardScreenshot(prompt: String, completion: @escaping ([Int]) -> Void) {
        let destination = FileManager.default.temporaryDirectory
            .appendingPathComponent("solution-finder-opener-\(UUID().uuidString).png")
        model.status = prompt
        NSApp.activate(ignoringOtherApps: true)

        DispatchQueue.global(qos: .userInitiated).async {
            let process = Process()
            process.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
            process.arguments = ["-i", "-x", destination.path]

            do {
                try process.run()
                process.waitUntilExit()
            } catch {
                DispatchQueue.main.async {
                    model.status = "Screenshot failed: \(error.localizedDescription)"
                }
                return
            }

            DispatchQueue.main.async {
                guard process.terminationStatus == 0,
                      FileManager.default.fileExists(atPath: destination.path) else {
                    model.status = "Screenshot canceled"
                    return
                }
                guard let image = NSImage(contentsOf: destination),
                      let cells = fumenCells(fromBoardImage: image, preservingColors: true) else {
                    model.status = "Could not read screenshot as a board"
                    return
                }

                completion(cells)
            }
        }
    }

    private func variationCellsWithGrayBase(_ cells: [Int]) -> [Int] {
        let basePages = model.basePages
        guard basePages.indices.contains(model.baseSelectedPage) else { return cells }
        let baseCells = basePages[model.baseSelectedPage]
        var result = cells
        for index in result.indices where baseCells.indices.contains(index) {
            if baseCells[index] != 0, result[index] != 0 {
                result[index] = 8
            }
        }
        return result
    }

    private func encodeOnePageFumen(cells: [Int], completion: @escaping (String?) -> Void) {
        let page = Array(cells.prefix(240)) + Array(repeating: 0, count: max(0, 240 - cells.count))
        let js = openerImporterFumenCodecScript(pages: [Array(page.prefix(240))])

        webView.evaluateJavaScript(js) { value, error in
            DispatchQueue.main.async {
                if let error {
                    model.status = "Encode failed: \(error.localizedDescription)"
                    completion(nil)
                    return
                }

                let code = (value as? String ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
                guard !code.isEmpty else {
                    model.status = "No fumen code exported"
                    completion(nil)
                    return
                }
                completion(code)
            }
        }
    }

    private func openerImporterFumenCodecScript(pages: [[Int]]) -> String {
        let pageJSON = "[" + pages
            .map { "[" + $0.map(String.init).joined(separator: ",") + "]" }
            .joined(separator: ",") + "]"

        return """
        (function() {
          var tx = document.getElementById('tx');
          if (!tx) return '';
          var pages = \(pageJSON);
          if (!pages.length) return '';
          frame = 0;
          framemax = Math.max(0, pages.length - 1);
          for (var e = 0; e < pages.length; e++) {
            for (var i = 0; i < fldblks; i++) af[e * fldblks + i] = pages[e][i] || 0;
            ap[e * 3 + 0] = 0;
            ap[e * 3 + 1] = 0;
            ap[e * 3 + 2] = 0;
            au[e] = 0;
            am[e] = 0;
            ac[e] = '';
            ad[e] = 0;
          }
          for (var i = 0; i < fldblks; i++) f[i] = pages[0][i] || 0;
          if (typeof p !== 'undefined') p = [0, 0, 0];
          var up = document.getElementById('up'); if (up) up.checked = false;
          var mr = document.getElementById('mr'); if (mr) mr.checked = false;
          var dc = document.getElementById('dc'); if (dc) dc.checked = true;
          var cm = document.getElementById('cm'); if (cm) cm.value = '';
          if (typeof refresh === 'function') refresh();
          if (typeof updated === 'function') updated();
          else if (typeof encode === 'function') encode(0);
          return tx.value || '';
        })();
        """
    }

    private func decodeFumenCode(_ code: String, completion: @escaping ([[Int]]) -> Void) {
        let trimmedCode = code.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedCode.isEmpty else {
            completion([])
            return
        }

        let js = """
        (function() {
          var tx = document.getElementById('tx');
          if (!tx) return [];
          if (typeof updateflag !== 'undefined') updateflag = 0;
          if (typeof newdata === 'function') newdata(0);
          if (typeof updateflag !== 'undefined') updateflag = 0;
          tx.value = \(javascriptStringLiteral(trimmedCode));
          if (typeof versioncheck === 'function') versioncheck(0);
          var pages = [];
          for (var e = 0; e <= framemax; e++) {
            pages.push(Array.prototype.slice.call(af, e * fldblks, (e + 1) * fldblks));
          }
          return pages;
        })();
        """

        webView.evaluateJavaScript(js) { value, error in
            DispatchQueue.main.async {
                if let error {
                    model.status = "Decode failed: \(error.localizedDescription)"
                    completion([])
                    return
                }
                let pages = (value as? [Any])?.compactMap { page -> [Int]? in
                    guard let rawCells = page as? [Any] else { return nil }
                    return rawCells.compactMap { cell -> Int? in
                        if let number = cell as? NSNumber { return number.intValue }
                        return cell as? Int
                    }
                } ?? []
                completion(pages)
            }
        }
    }
}

struct OpenerBoardPreview: View {
    let cells: [Int]
    let cellSize: CGFloat

    private var columns: [GridItem] {
        Array(repeating: GridItem(.fixed(cellSize), spacing: 2), count: 10)
    }

    var body: some View {
        LazyVGrid(columns: columns, spacing: 2) {
            ForEach(previewIndices, id: \.self) { index in
                Rectangle()
                    .fill(fumenCellColor(safeCells[index]))
                    .overlay(Rectangle().stroke(Color(nsColor: .separatorColor), lineWidth: 0.5))
                    .frame(width: cellSize, height: cellSize)
            }
        }
        .padding(8)
        .background(Color.black)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))
    }

    private var safeCells: [Int] {
        cells.count >= 240 ? cells : cells + Array(repeating: 0, count: max(0, 240 - cells.count))
    }

    private var previewIndices: [Int] {
        (fumenVisibleTopRow..<fumenVisibleBottomRow).flatMap { row in
            (0..<10).map { column in row * 10 + column }
        }
    }
}

struct FumenPaletteItem {
    let value: Int
    let color: Color
}

let fumenPalette: [FumenPaletteItem] = [
    FumenPaletteItem(value: 0, color: Color.black),
    FumenPaletteItem(value: 8, color: Color(nsColor: .systemGray)),
    FumenPaletteItem(value: 1, color: Color(red: 0.00, green: 0.78, blue: 0.82)),
    FumenPaletteItem(value: 2, color: Color(red: 0.95, green: 0.66, blue: 0.00)),
    FumenPaletteItem(value: 3, color: Color(red: 0.92, green: 0.88, blue: 0.00)),
    FumenPaletteItem(value: 4, color: Color(red: 0.88, green: 0.04, blue: 0.08)),
    FumenPaletteItem(value: 5, color: Color(red: 0.78, green: 0.04, blue: 0.88)),
    FumenPaletteItem(value: 6, color: Color(red: 0.04, green: 0.15, blue: 0.86)),
    FumenPaletteItem(value: 7, color: Color(red: 0.05, green: 0.78, blue: 0.12))
]

let setupFumenPalette: [FumenPaletteItem] = [
    FumenPaletteItem(value: 1, color: Color(red: 0.00, green: 0.78, blue: 0.82)),
    FumenPaletteItem(value: 3, color: Color(red: 0.92, green: 0.88, blue: 0.00)),
    FumenPaletteItem(value: 8, color: Color(nsColor: .systemGray))
]

let fumenVisibleTopRow = 3
let fumenVisibleRows = 20
let fumenVisibleBottomRow = fumenVisibleTopRow + fumenVisibleRows
let fumenSetupBottomRowExclusive = fumenVisibleBottomRow

func fumenOccupancyMask(_ cells: [Int], mirrored: Bool = false) -> [[Bool]] {
    let safeCells = cells.count >= 240 ? cells : cells + Array(repeating: 0, count: max(0, 240 - cells.count))
    var usedHeight = 0
    for bottomOffset in 0..<fumenVisibleRows {
        let row = fumenVisibleBottomRow - 1 - bottomOffset
        let rowStart = row * 10
        if safeCells[rowStart..<rowStart + 10].contains(where: { $0 != 0 }) {
            usedHeight = bottomOffset + 1
        }
    }

    guard usedHeight > 0 else { return [] }

    return (0..<usedHeight).map { bottomOffset in
        let row = fumenVisibleBottomRow - 1 - bottomOffset
        return (0..<10).map { column in
            let sourceColumn = mirrored ? 9 - column : column
            return safeCells[row * 10 + sourceColumn] != 0
        }
    }
}

func fumenOccupancyScore(_ lhs: [[Bool]], _ rhs: [[Bool]]) -> Double {
    let height = max(lhs.count, rhs.count)
    guard height > 0 else { return 0 }

    var intersection = 0
    var union = 0
    var leftCount = 0
    var rightCount = 0
    for row in 0..<height {
        for column in 0..<10 {
            let left = row < lhs.count ? lhs[row][column] : false
            let right = row < rhs.count ? rhs[row][column] : false
            if left { leftCount += 1 }
            if right { rightCount += 1 }
            if left || right {
                union += 1
                if left && right {
                    intersection += 1
                }
            }
        }
    }

    guard union > 0, leftCount > 0, rightCount > 0 else { return 0 }

    let jaccard = Double(intersection) / Double(union)
    let leftCoverage = Double(intersection) / Double(leftCount)
    let rightCoverage = Double(intersection) / Double(rightCount)
    let coverage = min(leftCoverage, rightCoverage)
    return max(jaccard, coverage * 0.92)
}

func shiftedOccupancyMask(_ mask: [[Bool]], by offset: Int) -> [[Bool]] {
    guard !mask.isEmpty, offset != 0 else { return mask }
    let emptyRow = Array(repeating: false, count: 10)
    if offset > 0 {
        return Array(repeating: emptyRow, count: offset) + mask
    }

    let dropCount = min(mask.count, abs(offset))
    return Array(mask.dropFirst(dropCount))
}

func fumenColorMask(_ cells: [Int], mirrored: Bool = false) -> [[Int]] {
    let safeCells = cells.count >= 240 ? cells : cells + Array(repeating: 0, count: max(0, 240 - cells.count))
    var usedHeight = 0
    for bottomOffset in 0..<fumenVisibleRows {
        let row = fumenVisibleBottomRow - 1 - bottomOffset
        let rowStart = row * 10
        if safeCells[rowStart..<rowStart + 10].contains(where: { $0 != 0 }) {
            usedHeight = bottomOffset + 1
        }
    }

    guard usedHeight > 0 else { return [] }

    return (0..<usedHeight).map { bottomOffset in
        let row = fumenVisibleBottomRow - 1 - bottomOffset
        return (0..<10).map { column in
            let sourceColumn = mirrored ? 9 - column : column
            let value = safeCells[row * 10 + sourceColumn]
            return mirrored ? mirroredFumenValue(value) : value
        }
    }
}

func fumenColorScore(_ lhs: [[Int]], _ rhs: [[Int]]) -> Double {
    let match = fumenColorMatch(lhs, rhs)
    return match.comparable == 0 ? 0 : Double(match.matching) / Double(match.comparable)
}

func fumenColorMatch(_ lhs: [[Int]], _ rhs: [[Int]]) -> (matching: Int, comparable: Int) {
    let height = max(lhs.count, rhs.count)
    guard height > 0 else { return (0, 0) }

    var matching = 0
    var comparable = 0
    for row in 0..<height {
        for column in 0..<10 {
            let left = row < lhs.count ? lhs[row][column] : 0
            let right = row < rhs.count ? rhs[row][column] : 0
            guard left != 0, right != 0, left != 8, right != 8 else { continue }
            comparable += 1
            if left == right {
                matching += 1
            }
        }
    }

    return (matching, comparable)
}

func shiftedColorMask(_ mask: [[Int]], by offset: Int) -> [[Int]] {
    guard !mask.isEmpty, offset != 0 else { return mask }
    let emptyRow = Array(repeating: 0, count: 10)
    if offset > 0 {
        return Array(repeating: emptyRow, count: offset) + mask
    }

    let dropCount = min(mask.count, abs(offset))
    return Array(mask.dropFirst(dropCount))
}

func openingDetectionCandidate(
    preset: FumenOpeningPreset,
    pageIndex: Int,
    cells: [Int],
    targetMask: [[Bool]],
    targetColors: [[Int]],
    mirrored: Bool,
    rowOffset: Int
) -> OpeningDetectionResult {
    let sampleMask = shiftedOccupancyMask(fumenOccupancyMask(cells, mirrored: mirrored), by: rowOffset)
    let sampleColors = shiftedColorMask(fumenColorMask(cells, mirrored: mirrored), by: rowOffset)
    let occupancyScore = fumenOccupancyScore(targetMask, sampleMask)
    let colorMatch = fumenColorMatch(targetColors, sampleColors)
    let colorScore = colorMatch.comparable == 0 ? 0 : Double(colorMatch.matching) / Double(colorMatch.comparable)
    return OpeningDetectionResult(
        preset: preset,
        occupancyScore: occupancyScore,
        colorScore: colorScore,
        comparableColorCells: colorMatch.comparable,
        mirrored: mirrored,
        pageIndex: pageIndex,
        rowOffset: rowOffset
    )
}

func bestOpeningDetectionResult(
    preset: FumenOpeningPreset,
    pages: [[Int]],
    targetMask: [[Bool]],
    targetColors: [[Int]]
) -> OpeningDetectionResult? {
    let offsets = [-2, -1, 0, 1, 2]
    var best: OpeningDetectionResult?
    for (pageIndex, cells) in pages.enumerated() {
        for mirrored in [false, true] {
            for offset in offsets {
                let candidate = openingDetectionCandidate(
                    preset: preset,
                    pageIndex: pageIndex,
                    cells: cells,
                    targetMask: targetMask,
                    targetColors: targetColors,
                    mirrored: mirrored,
                    rowOffset: offset
                )
                if betterOpeningResult(candidate, than: best) {
                    best = candidate
                }
            }
        }
    }
    return best
}

func betterOpeningResult(_ candidate: OpeningDetectionResult, than current: OpeningDetectionResult?) -> Bool {
    guard let current else { return true }

    let occupancyDelta = candidate.occupancyScore - current.occupancyScore
    if abs(occupancyDelta) > 0.04 {
        return occupancyDelta > 0
    }

    if candidate.comparableColorCells >= 6, current.comparableColorCells >= 6 {
        let candidateCombined = candidate.overallScore
        let currentCombined = current.overallScore
        let combinedDelta = candidateCombined - currentCombined
        if abs(combinedDelta) > 0.01 {
            return combinedDelta > 0
        }
    }

    if abs(occupancyDelta) > 0.015 {
        return occupancyDelta > 0
    }

    let smallColorDelta = candidate.colorScore - current.colorScore
    if abs(smallColorDelta) > 0.08 {
        return smallColorDelta > 0
    }

    if candidate.preset.variationName == "Base", current.preset.variationName != "Base" {
        return true
    }
    if current.preset.variationName == "Base", candidate.preset.variationName != "Base" {
        return false
    }

    if candidate.mirrored != current.mirrored {
        return !candidate.mirrored
    }

    return candidate.preset.name < current.preset.name
}

func fumenCellColor(_ value: Int) -> Color {
    if value == 0 { return Color.black }
    return fumenPalette.first { $0.value == value }?.color ?? Color(nsColor: .systemGray)
}

func fumenCells(fromBoardImage image: NSImage, preservingColors: Bool) -> [Int]? {
    guard let cgImage = image.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
        return nil
    }
    let bitmap = NSBitmapImageRep(cgImage: cgImage)
    let width = bitmap.pixelsWide
    let height = bitmap.pixelsHigh
    guard width >= 10, height >= 10 else { return nil }

    let inferredRows = min(fumenVisibleRows, max(1, Int((Double(height) / Double(width) * 10.0).rounded())))
    let targetTopRow = fumenVisibleBottomRow - inferredRows
    let sampleRadius = max(1, min(width / 120, height / max(inferredRows * 12, 1)))
    var cells = Array(repeating: 0, count: 240)

    for sourceRow in 0..<inferredRows {
        for column in 0..<10 {
            let centerX = Int((Double(column) + 0.5) * Double(width) / 10.0)
            let centerY = Int((Double(sourceRow) + 0.5) * Double(height) / Double(inferredRows))
            let value = sampledFumenValue(
                bitmap: bitmap,
                centerX: centerX,
                centerY: centerY,
                radius: sampleRadius
            )
            cells[(targetTopRow + sourceRow) * 10 + column] = preservingColors || value == 0 ? value : 8
        }
    }

    return cells
}

func sampledFumenValue(bitmap: NSBitmapImageRep, centerX: Int, centerY: Int, radius: Int) -> Int {
    var red = 0.0
    var green = 0.0
    var blue = 0.0
    var alpha = 0.0
    var count = 0.0

    for y in max(0, centerY - radius)...min(bitmap.pixelsHigh - 1, centerY + radius) {
        for x in max(0, centerX - radius)...min(bitmap.pixelsWide - 1, centerX + radius) {
            guard let color = bitmap.colorAt(x: x, y: y)?.usingColorSpace(.deviceRGB) else {
                continue
            }
            red += color.redComponent
            green += color.greenComponent
            blue += color.blueComponent
            alpha += color.alphaComponent
            count += 1
        }
    }

    guard count > 0 else { return 0 }
    return classifiedFumenValue(
        red: red / count,
        green: green / count,
        blue: blue / count,
        alpha: alpha / count
    )
}

func classifiedFumenValue(red: Double, green: Double, blue: Double, alpha: Double) -> Int {
    guard alpha > 0.2 else { return 0 }
    let maximum = max(red, green, blue)
    let minimum = min(red, green, blue)
    let brightness = maximum
    let chroma = maximum - minimum
    let saturation = maximum == 0 ? 0 : chroma / maximum

    if brightness < 0.14 {
        return 0
    }

    let hue = colorHue(red: red, green: green, blue: blue, maximum: maximum, chroma: chroma)

    if saturation >= 0.26 && chroma >= 0.11 {
        let pieceHues: [(value: Int, hue: Double)] = [
            (1, 186.0),
            (2, 34.0),
            (3, 58.0),
            (4, 358.0),
            (5, 292.0),
            (6, 232.0),
            (7, 124.0)
        ]

        var bestValue = 0
        var bestScore = Double.greatestFiniteMagnitude
        for piece in pieceHues {
            let score = hueDistance(hue, piece.hue)
            if score < bestScore {
                bestScore = score
                bestValue = piece.value
            }
        }

        if bestScore <= 42.0 && brightness > 0.20 {
            return bestValue
        }
    }

    if isRecognizedGray(red: red, green: green, blue: blue, brightness: brightness, chroma: chroma, saturation: saturation) {
        return 8
    }

    if saturation < 0.30 || chroma < 0.13 {
        return 0
    }

    if hue >= 165.0 && hue <= 205.0 && blue > red * 1.35 {
        return 1
    }

    if green == maximum && blue < green * 0.82 {
        if hue >= 72.0 && hue < 158.0 {
            return 7
        }
        if hue >= 45.0 && hue < 72.0 && red > green * 0.74 {
            return 3
        }
    }

    return 0
}

func isRecognizedGray(red: Double, green: Double, blue: Double, brightness: Double, chroma: Double, saturation: Double) -> Bool {
    guard brightness > 0.32, brightness < 0.82 else { return false }
    guard saturation < 0.12, chroma < 0.09 else { return false }

    let average = (red + green + blue) / 3.0
    let redDelta = abs(red - average)
    let greenDelta = abs(green - average)
    let blueDelta = abs(blue - average)
    return max(redDelta, greenDelta, blueDelta) < 0.045
}

func colorHue(red: Double, green: Double, blue: Double, maximum: Double, chroma: Double) -> Double {
    guard chroma > 0 else { return 0 }
    let hue: Double
    if maximum == red {
        hue = 60.0 * ((green - blue) / chroma).truncatingRemainder(dividingBy: 6.0)
    } else if maximum == green {
        hue = 60.0 * ((blue - red) / chroma + 2.0)
    } else {
        hue = 60.0 * ((red - green) / chroma + 4.0)
    }
    return hue < 0 ? hue + 360.0 : hue
}

func hueDistance(_ lhs: Double, _ rhs: Double) -> Double {
    let distance = abs(lhs - rhs).truncatingRemainder(dividingBy: 360.0)
    return min(distance, 360.0 - distance)
}

func mirroredFumenValue(_ value: Int) -> Int {
    switch value {
    case 2: return 6
    case 6: return 2
    case 4: return 7
    case 7: return 4
    default: return value
    }
}

func lineClearedFumenCells(_ source: [Int]) -> [Int] {
    var cells = Array(source.prefix(240))
    if cells.count < 240 {
        cells += Array(repeating: 0, count: 240 - cells.count)
    }

    var remainingRows: [[Int]] = []
    var clearedCount = 0
    for row in 0..<23 {
        let start = row * 10
        let rowCells = Array(cells[start..<start + 10])
        if rowCells.allSatisfy({ $0 != 0 }) {
            clearedCount += 1
        } else {
            remainingRows.append(rowCells)
        }
    }

    guard clearedCount > 0 else {
        for index in 230..<240 {
            cells[index] = 0
        }
        return cells
    }

    let emptyRows = Array(repeating: Array(repeating: 0, count: 10), count: clearedCount)
    let clearedRows = emptyRows + remainingRows
    var result = Array(repeating: 0, count: 240)
    for row in 0..<23 {
        for column in 0..<10 {
            result[row * 10 + column] = clearedRows[row][column]
        }
    }
    return result
}

struct FumenPiece {
    let type: Int
    let name: String
    let color: Color
}

let fumenPieces: [FumenPiece] = [
    FumenPiece(type: 1, name: "I", color: Color(red: 0.00, green: 0.78, blue: 0.82)),
    FumenPiece(type: 2, name: "L", color: Color(red: 0.95, green: 0.66, blue: 0.00)),
    FumenPiece(type: 3, name: "O", color: Color(red: 0.92, green: 0.88, blue: 0.00)),
    FumenPiece(type: 4, name: "Z", color: Color(red: 0.88, green: 0.04, blue: 0.08)),
    FumenPiece(type: 5, name: "T", color: Color(red: 0.78, green: 0.04, blue: 0.88)),
    FumenPiece(type: 6, name: "J", color: Color(red: 0.04, green: 0.15, blue: 0.86)),
    FumenPiece(type: 7, name: "S", color: Color(red: 0.05, green: 0.78, blue: 0.12))
]

let fumenPieceOffsets: [Int: [[(x: Int, y: Int)]]] = [
    1: [[(0,1),(1,1),(2,1),(3,1)], [(1,0),(1,1),(1,2),(1,3)], [(0,1),(1,1),(2,1),(3,1)], [(1,0),(1,1),(1,2),(1,3)]],
    2: [[(0,1),(1,1),(2,1),(0,2)], [(1,0),(1,1),(1,2),(2,2)], [(2,0),(0,1),(1,1),(2,1)], [(0,0),(1,0),(1,1),(1,2)]],
    3: [[(1,1),(2,1),(1,2),(2,2)], [(1,1),(2,1),(1,2),(2,2)], [(1,1),(2,1),(1,2),(2,2)], [(1,1),(2,1),(1,2),(2,2)]],
    4: [[(0,1),(1,1),(1,2),(2,2)], [(2,0),(1,1),(2,1),(1,2)], [(0,1),(1,1),(1,2),(2,2)], [(2,0),(1,1),(2,1),(1,2)]],
    5: [[(0,1),(1,1),(2,1),(1,2)], [(1,0),(1,1),(2,1),(1,2)], [(1,0),(0,1),(1,1),(2,1)], [(1,0),(0,1),(1,1),(1,2)]],
    6: [[(0,1),(1,1),(2,1),(2,2)], [(1,0),(2,0),(1,1),(1,2)], [(0,0),(0,1),(1,1),(2,1)], [(1,0),(1,1),(0,2),(1,2)]],
    7: [[(1,1),(2,1),(0,2),(1,2)], [(1,0),(1,1),(2,1),(2,2)], [(1,1),(2,1),(0,2),(1,2)], [(1,0),(1,1),(2,1),(2,2)]]
]

let gamePiecePreviewOffsets: [Int: [(x: Int, y: Int)]] = [
    1: [(0,2),(1,2),(2,2),(3,2)],
    2: [(0,1),(1,1),(2,1),(2,2)],
    3: [(1,1),(2,1),(1,2),(2,2)],
    4: [(0,2),(1,2),(1,1),(2,1)],
    5: [(0,1),(1,1),(2,1),(1,2)],
    6: [(0,1),(1,1),(2,1),(0,2)],
    7: [(1,2),(2,2),(0,1),(1,1)]
]

func fumenOperationCells(_ operation: FumenOperation) -> [Int] {
    guard operation.type > 0,
          let rotations = fumenPieceOffsets[operation.type]
    else { return [] }

    let originX = operation.position % 10
    let originY = operation.position / 10
    return rotations[operation.rotation % 4].compactMap { offset in
        let x = originX + offset.x - 1
        let y = originY + offset.y - 1
        guard (0..<10).contains(x), (0..<23).contains(y) else { return nil }
        return y * 10 + x
    }
}

func mirroredFumenOperation(_ operation: FumenOperation) -> FumenOperation {
    guard operation.type > 0 else { return operation }

    let mirroredCells = Set(fumenOperationCells(operation).map { index in
        let row = index / 10
        let column = index % 10
        return row * 10 + (9 - column)
    })
    let mirroredType = mirroredFumenValue(operation.type)

    for rotation in 0..<4 {
        for position in 0..<230 {
            let candidate = FumenOperation(type: mirroredType, rotation: rotation, position: position)
            if Set(fumenOperationCells(candidate)) == mirroredCells {
                return candidate
            }
        }
    }

    let row = operation.position / 10
    let column = operation.position % 10
    return FumenOperation(type: mirroredType, rotation: operation.rotation, position: row * 10 + (9 - column))
}

struct HeldGameInput {
    var command: SFTGameCommand
    var pressOrder = 0
    var elapsedMs = 0.0
    var repeatElapsedMs = 0.0
    var repeated = false
}

struct PlayableGameView: View {
    @ObservedObject var model: AppModel
    let screenshotAction: () -> Void
    let commitAction: () -> Void
    let detectOpeningAction: ([Int], String?, @escaping ([OpeningDetectionResult]) -> Void) -> Void
    @State private var game = SFTGameState()
    @State private var heldInputs: [String: HeldGameInput] = [:]
    @State private var focusToken = 0
    @State private var lastAutoExportedLockCount: Int32 = 0
    @State private var suppressPlayFumenReload = false
    @State private var undoStack: [SFTGameState] = []
    @State private var inputPressCounter = 0
    @State private var gameStartedAt = Date()
    @State private var gameHasStarted = false
    @State private var autoOpenerDetectionDone = false
    @State private var autoVariantDetectionDone = false
    @State private var autoOpeningDetectionInProgress = false
    @State private var autoOpeningDetectionStatus = ""
    @State private var autoOpenerDetectionStatus = ""
    @State private var autoDetectedOpenerName: String?
    @State private var autoDetectedOpenerMirrored: Bool?
    @State private var autoDetectedEarlyVariantDetection = false
    @State private var autoOpeningCycleStartPieceCount: Int32 = 0
    @AppStorage("play.control.left") private var controlLeft = "a"
    @AppStorage("play.control.right") private var controlRight = "d"
    @AppStorage("play.control.softDrop") private var controlSoftDrop = "s"
    @AppStorage("play.control.hardDrop") private var controlHardDrop = "space"
    @AppStorage("play.control.rotateCW") private var controlRotateCW = "w"
    @AppStorage("play.control.rotateCCW") private var controlRotateCCW = "q"
    @AppStorage("play.control.rotate180") private var controlRotate180 = "e"
    @AppStorage("play.control.hold") private var controlHold = "c"
    @AppStorage("play.control.undo") private var controlUndo = "z"
    @AppStorage("play.control.reset") private var controlReset = "r"
    @AppStorage("play.tuning.das") private var tuningAutoShiftMs = 130.0
    @AppStorage("play.tuning.arr") private var tuningRepeatMs = 28.0
    @AppStorage("play.tuning.softDrop") private var tuningSoftDropMs = 75.0
    @AppStorage("play.tuning.gravity") private var tuningGravityMs = 1000.0
    @AppStorage("play.tuning.lockDelay") private var tuningLockDelayMs = 500.0
    @AppStorage("play.tuning.gravityEnabled") private var gravityEnabled = true
    @AppStorage("play.tuning.infiniteLockDelay") private var infiniteLockDelay = false
    @AppStorage("play.tuning.infiniteHold") private var infiniteHold = false
    @AppStorage("play.previewCellSize") private var previewCellSize = 14.0
    @AppStorage("play.customQueue") private var customQueue = ""
    @AppStorage("play.customHold") private var customHold = "-"
    @AppStorage("play.exportIncludeActive") private var exportIncludeActive = false
    private let frameTimer = Timer.publish(every: 1.0 / 60.0, on: .main, in: .common).autoconnect()

    private let gameCellSize: CGFloat = 20
    private let gameCellSpacing: CGFloat = 2
    private let gameBoardPadding: CGFloat = 8
    private let columns = Array(repeating: GridItem(.fixed(20), spacing: 2), count: 10)

    var body: some View {
        HStack(alignment: .top, spacing: 18) {
            VStack(alignment: .leading, spacing: 10) {
                HStack(alignment: .top, spacing: 12) {
                    piecePreview(title: "Hold", piece: Int(game.hold))
                        .frame(width: previewColumnWidth, alignment: .topLeading)

                    FocusableGameBoard(focusToken: focusToken, onKey: handleKeyDown, onKeyUp: handleKeyUp, onUndo: undoGameAction) {
                        gameBoard
                    }
                    .frame(width: gameBoardOuterWidth, height: gameBoardOuterHeight)
                    .onTapGesture {
                        focusToken += 1
                    }

                    nextPreviewStack
                        .frame(width: previewColumnWidth, alignment: .topLeading)
                }
                .fixedSize(horizontal: true, vertical: true)
                gameBoardActions
            }
            .frame(minWidth: 430, idealWidth: 460, alignment: .topLeading)

            VStack(alignment: .leading, spacing: 12) {
                gameButtonPanel
                playStatsPanel
            }
            .frame(minWidth: 300, idealWidth: 360, maxWidth: 440)
        }
        .onAppear {
            loadFromEditor()
            focusToken += 1
        }
        .onReceive(frameTimer) { _ in
            advanceFrame(elapsedMs: 1000.0 / 60.0)
        }
        .onChange(of: model.fumenCode) { _ in
            guard model.fumenPanelTab == .play else { return }
            if suppressPlayFumenReload {
                suppressPlayFumenReload = false
                return
            }
            loadFromEditor()
        }
        .onChange(of: model.playSettingsApplyToken) { _ in
            applyQueueAndHold()
        }
    }

    private var gameBoardActions: some View {
        HStack(spacing: 6) {
            iconButton("Load from Editor", systemImage: "square.and.arrow.down") {
                loadFromEditor()
            }
            iconButton("Screenshot", systemImage: "camera.viewfinder") {
                screenshotAction()
            }
            iconButton("Send to Editor", systemImage: "square.and.arrow.up") {
                sendBoardToEditor()
            }
            Divider()
                .frame(height: 18)
            iconButton("New Game", systemImage: "arrow.clockwise") {
                resetGame()
            }
            iconButton("Undo Placement", systemImage: "arrow.uturn.backward") {
                undoGameAction()
            }
            .disabled(undoStack.isEmpty)
            iconButton("Focus Game", systemImage: "keyboard") {
                focusToken += 1
            }
        }
        .controlSize(.small)
    }

    private var gameBoard: some View {
        LazyVGrid(columns: columns, spacing: 2) {
            ForEach(0..<(Int(SFT_GAME_HEIGHT) * Int(SFT_GAME_WIDTH)), id: \.self) { index in
                let x = index % Int(SFT_GAME_WIDTH)
                let y = Int(SFT_GAME_HEIGHT) - 1 - (index / Int(SFT_GAME_WIDTH))
                let cellValue = Int(sft_game_cell(&game, Int32(x), Int32(y), 1))
                let ghostValue = Int(sft_game_ghost_cell(&game, Int32(x), Int32(y)))
                Rectangle()
                    .fill(cellValue == 0 && ghostValue > 0 ? fumenCellColor(ghostValue).opacity(0.28) : fumenCellColor(cellValue))
                    .overlay(
                        Rectangle()
                            .stroke(ghostValue > 0 && cellValue == 0 ? fumenCellColor(ghostValue).opacity(0.85) : Color(nsColor: .separatorColor), lineWidth: ghostValue > 0 && cellValue == 0 ? 1.2 : 0.5)
                    )
                    .frame(width: 20, height: 20)
            }
        }
        .frame(width: gameBoardGridWidth, height: gameBoardGridHeight)
        .padding(8)
        .background(Color.black)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))
        .frame(width: gameBoardOuterWidth, height: gameBoardOuterHeight)
    }

    private var nextPreviewStack: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Next")
                .font(.caption)
                .foregroundStyle(.secondary)
            ForEach(0..<5, id: \.self) { index in
                pieceMini(Int(sft_game_queue_piece(&game, Int32(index))))
            }
        }
        .font(.caption)
    }

    private var playStatsPanel: some View {
        GroupBox("Stats") {
            VStack(alignment: .leading, spacing: 8) {
                Text("Pieces: \(game.pieces_locked)")
                    .font(.body.monospacedDigit())
                Text("Lines: \(game.lines_cleared)")
                    .font(.body.monospacedDigit())
                if !lastClearText.isEmpty {
                    Text(lastClearText)
                        .font(.body.weight(.semibold))
                }
                Text(String(format: "PPS: %.2f", piecesPerSecond))
                    .font(.body.monospacedDigit())
                Text(autoOpeningDetectionStatus)
                    .font(.callout)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(minHeight: 18, alignment: .leading)
            }
            .padding(.vertical, 4)
        }
    }

    private var piecesPerSecond: Double {
        guard gameHasStarted else { return 0 }
        let elapsed = max(0.001, Date().timeIntervalSince(gameStartedAt))
        return Double(game.pieces_locked) / elapsed
    }

    private var lastClearText: String {
        let lineCount = Int(game.last_clear_lines)
        let isTSpin = game.last_clear_t_spin != 0
        let isMini = game.last_clear_t_spin_mini != 0
        guard lineCount > 0 || isTSpin else { return "" }

        let lineName: String
        switch lineCount {
        case 0: lineName = ""
        case 1: lineName = "Single"
        case 2: lineName = "Double"
        case 3: lineName = "Triple"
        case 4: lineName = "Tetris"
        default: lineName = "\(lineCount) Lines"
        }

        if isTSpin {
            let spinName = isMini ? "Mini T-Spin" : "T-Spin"
            return lineName.isEmpty ? spinName : "\(spinName) \(lineName)"
        }
        return lineName
    }

    private var gameButtonPanel: some View {
        GroupBox("Moves") {
            VStack(spacing: 8) {
                HStack {
                    moveButton("Left", systemImage: "arrow.left", command: SFT_CMD_LEFT)
                    moveButton("Right", systemImage: "arrow.right", command: SFT_CMD_RIGHT)
                    moveButton("Soft", systemImage: "arrow.down", command: SFT_CMD_SOFT_DROP)
                }
                HStack {
                    moveButton("CW", systemImage: "rotate.right", command: SFT_CMD_ROTATE_CW)
                    moveButton("CCW", systemImage: "rotate.left", command: SFT_CMD_ROTATE_CCW)
                    Button("180") { runCommand(SFT_CMD_ROTATE_180) }
                    moveButton("Hold", systemImage: "tray.and.arrow.down", command: SFT_CMD_HOLD)
                }
                HStack {
                    Button {
                        runCommand(SFT_CMD_HARD_DROP)
                    } label: {
                        Label("Hard Drop", systemImage: "arrow.down.to.line")
                    }
                }
            }
            .padding(.vertical, 4)
        }
    }

    private func iconButton(_ title: String, systemImage: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: systemImage)
                .frame(width: 18, height: 18)
        }
        .help(title)
    }

    private func moveButton(_ title: String, systemImage: String, command: SFTGameCommand) -> some View {
        Button {
            runCommand(command)
        } label: {
            Label(title, systemImage: systemImage)
        }
    }

    private func piecePreview(title: String, piece: Int) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title)
                .foregroundStyle(.secondary)
            pieceMini(piece)
        }
        .font(.caption)
    }

    private var previewBoxWidth: CGFloat {
        let size = CGFloat(previewCellSize.rounded())
        let spacing = max(1, size / 8)
        return size * 4 + spacing * 3
    }

    private var previewColumnWidth: CGFloat {
        max(74, previewBoxWidth)
    }

    private var gameBoardGridWidth: CGFloat {
        CGFloat(SFT_GAME_WIDTH) * gameCellSize + CGFloat(SFT_GAME_WIDTH - 1) * gameCellSpacing
    }

    private var gameBoardGridHeight: CGFloat {
        CGFloat(SFT_GAME_HEIGHT) * gameCellSize + CGFloat(SFT_GAME_HEIGHT - 1) * gameCellSpacing
    }

    private var gameBoardOuterWidth: CGFloat {
        gameBoardGridWidth + gameBoardPadding * 2
    }

    private var gameBoardOuterHeight: CGFloat {
        gameBoardGridHeight + gameBoardPadding * 2
    }

    private func pieceMini(_ piece: Int) -> some View {
        let cells = Set((gamePiecePreviewOffsets[piece] ?? []).map { (3 - $0.y) * 4 + $0.x })
        let size = CGFloat(previewCellSize.rounded())
        let spacing: CGFloat = max(1, size / 8)
        return LazyVGrid(columns: Array(repeating: GridItem(.fixed(size), spacing: spacing), count: 4), spacing: spacing) {
            ForEach(0..<16, id: \.self) { index in
                Rectangle()
                    .fill(cells.contains(index) ? fumenCellColor(piece) : Color.clear)
                    .frame(width: size, height: size)
            }
        }
        .frame(width: size * 4 + spacing * 3, height: size * 4 + spacing * 3)
    }

    private func pieceName(_ value: Int) -> String {
        switch value {
        case 1: return "I"
        case 2: return "L"
        case 3: return "O"
        case 4: return "Z"
        case 5: return "T"
        case 6: return "J"
        case 7: return "S"
        default: return "-"
        }
    }

    private func loadFromEditor() {
        var cells = model.solverFumenCells().map(Int32.init)
        cells.withUnsafeMutableBufferPointer { buffer in
            sft_game_init_seeded(&game, randomSeed())
            sft_game_load_fumen_cells(&game, buffer.baseAddress)
        }
        applyTuning()
        applyQueueAndHold(spawnFromQueue: false)
        heldInputs.removeAll()
        undoStack.removeAll()
        lastAutoExportedLockCount = game.pieces_locked
        resetPlayStats()
    }

    private func resetGame() {
        sft_game_reset_seeded(&game, randomSeed())
        applyTuning()
        applyQueueAndHold(spawnFromQueue: false)
        heldInputs.removeAll()
        undoStack.removeAll()
        lastAutoExportedLockCount = game.pieces_locked
        resetPlayStats()
        focusToken += 1
    }

    private func resetPlayStats() {
        gameStartedAt = Date()
        gameHasStarted = false
        autoOpenerDetectionDone = false
        autoVariantDetectionDone = false
        autoOpeningDetectionInProgress = false
        autoOpeningDetectionStatus = ""
        autoOpenerDetectionStatus = ""
        autoDetectedOpenerName = nil
        autoDetectedOpenerMirrored = nil
        autoDetectedEarlyVariantDetection = false
        autoOpeningCycleStartPieceCount = game.pieces_locked
    }

    private func randomSeed() -> UInt32 {
        UInt32(Date().timeIntervalSince1970.truncatingRemainder(dividingBy: Double(UInt32.max))) ^ UInt32.random(in: 1...UInt32.max)
    }

    private func runCommand(_ command: SFTGameCommand) {
        applyTuning()
        startGameIfNeeded()
        _ = performGameCommand(command)
        focusToken += 1
    }

    private func startGameIfNeeded() {
        guard !gameHasStarted else { return }
        gameHasStarted = true
        gameStartedAt = Date()
    }

    @discardableResult
    private func performGameCommand(_ command: SFTGameCommand, allowSoftDropFollowup: Bool = true) -> Bool {
        if command == SFT_CMD_SOFT_DROP && tuningSoftDropMs <= 0 {
            return performInstantSoftDrop()
        }

        let before = game
        let changed = sft_game_command(&game, Int32(command.rawValue)) != 0
        guard changed else { return false }

        if game.pieces_locked != before.pieces_locked {
            pushUndoSnapshot(before)
            _ = applyEntryMovementForHeldInputs()
        } else if command == SFT_CMD_HOLD {
            _ = applyEntryMovementForHeldInputs()
        } else if allowSoftDropFollowup && shouldApplyHeldMovementFollowup(after: command) {
            let appliedKeys = applyEntryMovementForHeldInputs()
            let appliedHeldCommands = appliedKeys.compactMap { heldInputs[$0]?.command }
            let appliedHorizontal = appliedHeldCommands.contains(SFT_CMD_LEFT) || appliedHeldCommands.contains(SFT_CMD_RIGHT)
            let appliedSoftDrop = appliedHeldCommands.contains(SFT_CMD_SOFT_DROP)
            if !appliedHorizontal && !appliedSoftDrop {
                applyHeldSoftDropAfterHorizontalMove()
            }
        } else if allowSoftDropFollowup && shouldApplySoftDropFollowup(after: command) {
            applyHeldSoftDropAfterHorizontalMove()
        }

        exportFumenIfPieceLocked()
        return true
    }

    @discardableResult
    private func performInstantSoftDrop() -> Bool {
        var changed = false
        while sft_game_command(&game, Int32(SFT_CMD_SOFT_DROP.rawValue)) != 0 {
            changed = true
        }
        guard changed else { return false }
        exportFumenIfPieceLocked()
        return true
    }

    private func pushUndoSnapshot(_ snapshot: SFTGameState) {
        undoStack.append(snapshot)
        if undoStack.count > 50 {
            undoStack.removeFirst(undoStack.count - 50)
        }
    }

    private func undoGameAction() {
        guard let previous = undoStack.popLast() else { return }
        game = previous
        heldInputs.removeAll()
        lastAutoExportedLockCount = game.pieces_locked
        if game.pieces_locked < autoOpeningCycleStartPieceCount {
            autoOpeningCycleStartPieceCount = 0
            autoOpenerDetectionDone = false
            autoVariantDetectionDone = false
            autoOpeningDetectionInProgress = false
            autoOpeningDetectionStatus = ""
            autoOpenerDetectionStatus = ""
            autoDetectedOpenerName = nil
            autoDetectedOpenerMirrored = nil
            autoDetectedEarlyVariantDetection = false
        }
        let cyclePieces = game.pieces_locked - autoOpeningCycleStartPieceCount
        if cyclePieces < 12 {
            autoVariantDetectionDone = false
            autoOpeningDetectionStatus = cyclePieces >= 6 ? autoOpenerDetectionStatus : ""
        }
        if cyclePieces < 6 {
            autoOpenerDetectionDone = false
            autoOpeningDetectionInProgress = false
            autoOpeningDetectionStatus = ""
            autoOpenerDetectionStatus = ""
        }
        exportGameBoardToFumen(includeActive: false, switchToEditor: false, status: "Undid play move")
        focusToken += 1
    }

    private func applyTuning() {
        sft_game_set_tuning(&game, Int32(tuningGravityMs.rounded()), Int32(tuningLockDelayMs.rounded()))
        sft_game_set_options(&game, gravityEnabled ? 1 : 0, infiniteLockDelay ? 1 : 0, infiniteHold ? 1 : 0)
    }

    private func applyQueueAndHold(spawnFromQueue: Bool = true) {
        let pieces = parsedQueuePieces()
        if !pieces.isEmpty {
            var values = pieces.map(Int32.init)
            let valueCount = Int32(values.count)
            values.withUnsafeMutableBufferPointer { buffer in
                sft_game_set_custom_queue(&game, buffer.baseAddress, valueCount)
            }
        } else if spawnFromQueue {
            sft_game_clear_custom_queue(&game)
        }
        sft_game_set_hold_piece(&game, Int32(pieceValue(customHold)))
        applyTuning()
        undoStack.removeAll()
        lastAutoExportedLockCount = game.pieces_locked
        resetPlayStats()
        focusToken += 1
    }

    private func parsedQueuePieces() -> [Int] {
        customQueue.uppercased().compactMap { character in
            pieceValue(String(character))
        }.filter { $0 > 0 }
    }

    private func pieceValue(_ text: String) -> Int {
        switch text.trimmingCharacters(in: .whitespacesAndNewlines).uppercased() {
        case "I": return 1
        case "L": return 2
        case "O": return 3
        case "Z": return 4
        case "T": return 5
        case "J": return 6
        case "S": return 7
        default: return 0
        }
    }

    private func sendBoardToEditor() {
        exportGameBoardToFumen(includeActive: exportIncludeActive, switchToEditor: true, status: "Sent playable board to fumen")
    }

    private func exportFumenIfPieceLocked() {
        guard game.pieces_locked != lastAutoExportedLockCount else { return }
        lastAutoExportedLockCount = game.pieces_locked
        exportGameBoardToFumen(includeActive: false, switchToEditor: false, status: "Updated fumen after lock")
        if detectPerfectClearIfNeeded() {
            return
        }
        checkAutoOpeningDetection()
    }

    private func detectPerfectClearIfNeeded() -> Bool {
        guard game.pieces_locked > autoOpeningCycleStartPieceCount,
              lockedFumenCells().allSatisfy({ $0 == 0 }) else {
            return false
        }
        autoOpeningCycleStartPieceCount = game.pieces_locked
        autoOpenerDetectionDone = false
        autoVariantDetectionDone = false
        autoOpeningDetectionInProgress = false
        autoOpenerDetectionStatus = ""
        autoDetectedOpenerName = nil
        autoDetectedOpenerMirrored = nil
        autoDetectedEarlyVariantDetection = false
        autoOpeningDetectionStatus = "Perfect clear"
        return true
    }

    private func checkAutoOpeningDetection() {
        let cyclePieces = game.pieces_locked - autoOpeningCycleStartPieceCount
        let variantDetectionPieces: Int32 = autoDetectedEarlyVariantDetection ? 6 : 12
        if cyclePieces >= variantDetectionPieces, autoDetectedOpenerName != nil, !autoVariantDetectionDone {
            runAutoOpeningDetection(stage: .variant)
        } else if cyclePieces >= 6, !autoOpenerDetectionDone {
            runAutoOpeningDetection(stage: .opener)
        }
    }

    private enum AutoOpeningDetectionStage {
        case opener
        case variant
    }

    private func runAutoOpeningDetection(stage: AutoOpeningDetectionStage) {
        guard !autoOpeningDetectionInProgress else { return }
        autoOpeningDetectionInProgress = true
        let openerFilter = stage == .variant ? autoDetectedOpenerName : nil
        detectOpeningAction(lockedFumenCells(), openerFilter) { results in
            autoOpeningDetectionInProgress = false
            if stage == .opener {
                autoOpenerDetectionDone = true
            } else {
                autoVariantDetectionDone = true
            }
            let filteredResults = stage == .variant
                ? results.filter { result in
                    guard let autoDetectedOpenerMirrored else { return true }
                    return result.mirrored == autoDetectedOpenerMirrored
                }
                : results
            logAutoOpeningDetection(stage: stage, results: results, filteredResults: filteredResults, openerFilter: openerFilter)
            guard let best = filteredResults.first, best.overallScore >= autoOpeningDetectionMinimumScore else {
                return
            }
            let name = autoOpeningName(for: best, stage: stage)
            let statusPrefix = stage == .opener ? "Opener" : "Variant"
            let status = "\(statusPrefix): \(name)"
            if stage == .opener {
                autoDetectedOpenerName = best.preset.openerName
                autoDetectedOpenerMirrored = best.mirrored
                autoDetectedEarlyVariantDetection = best.preset.earlyVariantDetection || isDPCPatternsOpener(best.preset.openerName)
                autoOpenerDetectionStatus = status
            }
            autoOpeningDetectionStatus = status
        }
    }

    private var autoOpeningDetectionMinimumScore: Double {
        0.77
    }

    private func logAutoOpeningDetection(
        stage: AutoOpeningDetectionStage,
        results: [OpeningDetectionResult],
        filteredResults: [OpeningDetectionResult],
        openerFilter: String?
    ) {
        let stageName = stage == .opener ? "opener" : "variant"
        let filterText = openerFilter.map { "opener=\($0)" } ?? "opener=any"
        let mirrorText = autoDetectedOpenerMirrored.map { "mirror=\($0 ? "mirrored" : "normal")" } ?? "mirror=any"
        let best = filteredResults.first.map(playAutoDetectionSummary) ?? "none"
        let accepted = (filteredResults.first?.overallScore ?? 0) >= autoOpeningDetectionMinimumScore
        DetectionDebugLog.shared.appendBlock(
            title: "Play auto \(stageName) gate: raw \(results.count), mirror-filtered \(filteredResults.count), \(filterText), \(mirrorText), accepted=\(accepted)",
            lines: ["Best: \(best)", String(format: "Minimum overall: %.1f%%", autoOpeningDetectionMinimumScore * 100.0)]
        )
    }

    private func playAutoDetectionSummary(_ result: OpeningDetectionResult) -> String {
        let overall = String(format: "%.1f%%", result.overallScore * 100.0)
        let shape = String(format: "%.1f%%", result.occupancyScore * 100.0)
        let color = result.comparableColorCells >= 6 ? String(format: "%.1f%%", result.colorScore * 100.0) : "n/a"
        let mirror = result.mirrored ? " mirrored" : ""
        let rowOffset = result.rowOffset == 0 ? "" : " rows \(result.rowOffset > 0 ? "+" : "")\(result.rowOffset)"
        return "\(result.preset.openerName) - \(result.preset.variationName)\(mirror)\(rowOffset) | overall \(overall) shape \(shape) color \(color) comparable \(result.comparableColorCells)"
    }

    private func isDPCPatternsOpener(_ name: String) -> Bool {
        name.trimmingCharacters(in: .whitespacesAndNewlines)
            .localizedCaseInsensitiveCompare("DPC Patterns") == .orderedSame
    }

    private func autoOpeningName(for result: OpeningDetectionResult, stage: AutoOpeningDetectionStage) -> String {
        var name: String
        switch stage {
        case .opener:
            name = result.preset.openerName
        case .variant:
            name = result.preset.variationName == "Base"
                ? result.preset.openerName
                : "\(result.preset.openerName) - \(result.preset.variationName)"
        }
        if result.mirrored {
            name += " mirrored"
        }
        return name
    }

    private func lockedFumenCells() -> [Int] {
        var cells = Array(repeating: Int32(0), count: 240)
        cells.withUnsafeMutableBufferPointer { buffer in
            sft_game_write_fumen_cells(&game, 0, buffer.baseAddress)
        }
        return cells.map(Int.init)
    }

    private func exportGameBoardToFumen(includeActive: Bool, switchToEditor: Bool, status: String) {
        var cells = Array(repeating: Int32(0), count: 240)
        cells.withUnsafeMutableBufferPointer { buffer in
            sft_game_write_fumen_cells(&game, includeActive ? 1 : 0, buffer.baseAddress)
        }
        suppressPlayFumenReload = true
        model.replaceCurrentFumenCells(cells.map(Int.init))
        model.inputSource = .fumen
        if switchToEditor {
            model.fumenPanelTab = .editor
        }
        model.status = status
        commitAction()
    }

    private func normalizedKey(_ value: String) -> String {
        value.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
    }

    private func command(for key: String) -> SFTGameCommand? {
        let key = normalizedKey(key)
        if key == normalizedKey(controlLeft) {
            return SFT_CMD_LEFT
        } else if key == normalizedKey(controlRight) {
            return SFT_CMD_RIGHT
        } else if key == normalizedKey(controlSoftDrop) {
            return SFT_CMD_SOFT_DROP
        } else if key == normalizedKey(controlHardDrop) {
            return SFT_CMD_HARD_DROP
        } else if key == normalizedKey(controlRotateCW) {
            return SFT_CMD_ROTATE_CW
        } else if key == normalizedKey(controlRotateCCW) {
            return SFT_CMD_ROTATE_CCW
        } else if key == normalizedKey(controlRotate180) {
            return SFT_CMD_ROTATE_180
        } else if key == normalizedKey(controlHold) {
            return SFT_CMD_HOLD
        }
        return nil
    }

    private func handleKeyDown(_ key: String) {
        let key = normalizedKey(key)
        if key == normalizedKey(controlUndo) {
            undoGameAction()
            return
        }
        if key == normalizedKey(controlReset) {
            resetGame()
            return
        }
        guard let command = command(for: key) else { return }
        if heldInputs[key] == nil {
            inputPressCounter += 1
            runCommand(command)
            heldInputs[key] = HeldGameInput(command: command, pressOrder: inputPressCounter)
        }
    }

    private func handleKeyUp(_ key: String) {
        heldInputs.removeValue(forKey: normalizedKey(key))
    }

    private func advanceFrame(elapsedMs: Double) {
        guard gameHasStarted else { return }
        applyTuning()
        let softDropHeld = tuningSoftDropMs > 0 && heldInputs.values.contains { $0.command == SFT_CMD_SOFT_DROP }
        let beforeTick = game
        sft_game_tick(&game, Int32(elapsedMs.rounded()), softDropHeld ? 1 : 0)
        var entryAppliedKeys = Set<String>()
        if game.pieces_locked != beforeTick.pieces_locked {
            pushUndoSnapshot(beforeTick)
            entryAppliedKeys = applyEntryMovementForHeldInputs()
        }
        exportFumenIfPieceLocked()

        for key in sortedHeldInputKeys() {
            guard var input = heldInputs[key] else { continue }
            input.elapsedMs += elapsedMs
            input.repeatElapsedMs += elapsedMs
            let command = input.command
            if isBlockedHorizontalRepeat(input) {
                input.elapsedMs = 0
                input.repeatElapsedMs = 0
                input.repeated = false
                heldInputs[key] = input
                continue
            }
            if entryAppliedKeys.contains(key) {
                if input.command == SFT_CMD_SOFT_DROP {
                    input.elapsedMs = max(input.elapsedMs, tuningSoftDropMs)
                } else {
                    input.elapsedMs = max(input.elapsedMs, tuningAutoShiftMs)
                }
                input.repeatElapsedMs = 0
                input.repeated = true
                heldInputs[key] = input
                continue
            }
            let delay = command == SFT_CMD_SOFT_DROP ? tuningSoftDropMs : (input.repeated ? tuningRepeatMs : tuningAutoShiftMs)
            let repeatInterval = command == SFT_CMD_SOFT_DROP ? tuningSoftDropMs : tuningRepeatMs
            let isRepeatable = command == SFT_CMD_LEFT || command == SFT_CMD_RIGHT || command == SFT_CMD_SOFT_DROP
            if isRepeatable && input.elapsedMs >= delay {
                if repeatInterval <= 0 {
                    let maxRepeats = command == SFT_CMD_SOFT_DROP ? Int(SFT_GAME_HEIGHT) : Int(SFT_GAME_WIDTH)
                    for _ in 0..<maxRepeats {
                        let lockCount = game.pieces_locked
                        let changed = performGameCommand(command)
                        if !changed || game.pieces_locked != lockCount {
                            break
                        }
                    }
                    input.repeatElapsedMs = 0
                } else {
                    while input.repeatElapsedMs >= repeatInterval {
                        input.repeatElapsedMs -= repeatInterval
                        let lockCount = game.pieces_locked
                        let changed = performGameCommand(command)
                        if !changed || game.pieces_locked != lockCount {
                            break
                        }
                    }
                }
                input.repeated = true
            }
            heldInputs[key] = input
        }
    }

    private func sortedHeldInputKeys() -> [String] {
        heldInputs.keys.sorted { leftKey, rightKey in
            guard let left = heldInputs[leftKey], let right = heldInputs[rightKey] else {
                return leftKey < rightKey
            }
            let leftPriority = heldInputPriority(left.command)
            let rightPriority = heldInputPriority(right.command)
            if leftPriority != rightPriority {
                return leftPriority < rightPriority
            }
            return left.pressOrder < right.pressOrder
        }
    }

    private func heldInputPriority(_ command: SFTGameCommand) -> Int {
        if command == SFT_CMD_SOFT_DROP { return 0 }
        if command == SFT_CMD_LEFT || command == SFT_CMD_RIGHT { return 1 }
        return 2
    }

    private func shouldApplySoftDropFollowup(after command: SFTGameCommand) -> Bool {
        command == SFT_CMD_LEFT
            || command == SFT_CMD_RIGHT
            || command == SFT_CMD_ROTATE_CW
            || command == SFT_CMD_ROTATE_CCW
            || command == SFT_CMD_ROTATE_180
    }

    private func shouldApplyHeldMovementFollowup(after command: SFTGameCommand) -> Bool {
        command == SFT_CMD_ROTATE_CW
            || command == SFT_CMD_ROTATE_CCW
            || command == SFT_CMD_ROTATE_180
    }

    private func applyEntryMovementForHeldInputs() -> Set<String> {
        var appliedKeys = Set<String>()
        if let horizontal = latestHeldHorizontalInput(),
           horizontal.input.repeated || horizontal.input.elapsedMs >= tuningAutoShiftMs {
            let repeats = tuningRepeatMs <= 0 ? Int(SFT_GAME_WIDTH) : 1
            if runEntryMovement(horizontal.input.command, maxRepeats: repeats) {
                appliedKeys.insert(horizontal.key)
            }
        }
        if let softDrop = heldInputs.first(where: { $0.value.command == SFT_CMD_SOFT_DROP }),
           tuningSoftDropMs <= 0 || softDrop.value.repeated || softDrop.value.elapsedMs >= tuningSoftDropMs,
           runEntryMovement(SFT_CMD_SOFT_DROP, maxRepeats: Int(SFT_GAME_HEIGHT)) {
            appliedKeys.insert(softDrop.key)
        }
        return appliedKeys
    }

    private func runEntryMovement(_ command: SFTGameCommand, maxRepeats: Int) -> Bool {
        var changedAny = false
        for _ in 0..<max(1, maxRepeats) {
            let lockCount = game.pieces_locked
            let changed = sft_game_command(&game, Int32(command.rawValue)) != 0
            guard changed else { break }
            changedAny = true
            if shouldApplySoftDropFollowup(after: command) {
                applyHeldSoftDropAfterHorizontalMove()
            }
            if game.pieces_locked != lockCount {
                break
            }
        }
        return changedAny
    }

    private func applyHeldSoftDropAfterHorizontalMove() {
        guard let softDropKey = heldInputs.first(where: { $0.value.command == SFT_CMD_SOFT_DROP })?.key else {
            return
        }
        let beforeLockCount = game.pieces_locked
        let repeats = tuningSoftDropMs <= 0 ? Int(SFT_GAME_HEIGHT) : 1
        guard runEntryMovement(SFT_CMD_SOFT_DROP, maxRepeats: repeats) else { return }

        if var input = heldInputs[softDropKey] {
            input.elapsedMs = max(input.elapsedMs, tuningSoftDropMs)
            input.repeatElapsedMs = 0
            input.repeated = true
            heldInputs[softDropKey] = input
        }

        if game.pieces_locked != beforeLockCount {
            _ = applyEntryMovementForHeldInputs()
        }
    }

    private func latestHeldHorizontalInput() -> (key: String, input: HeldGameInput)? {
        heldInputs
            .filter { $0.value.command == SFT_CMD_LEFT || $0.value.command == SFT_CMD_RIGHT }
            .max { $0.value.pressOrder < $1.value.pressOrder }
            .map { (key: $0.key, input: $0.value) }
    }

    private func isBlockedHorizontalRepeat(_ input: HeldGameInput) -> Bool {
        guard input.command == SFT_CMD_LEFT || input.command == SFT_CMD_RIGHT else { return false }
        return heldInputs.values.contains { other in
            (other.command == SFT_CMD_LEFT || other.command == SFT_CMD_RIGHT)
                && other.command != input.command
                && other.pressOrder > input.pressOrder
        }
    }
}

struct FocusableGameBoard<Content: View>: NSViewRepresentable {
    let focusToken: Int
    let onKey: (String) -> Void
    let onKeyUp: (String) -> Void
    let onUndo: () -> Void
    @ViewBuilder let content: () -> Content

    func makeCoordinator() -> Coordinator {
        Coordinator()
    }

    func makeNSView(context: Context) -> KeyHostingView<Content> {
        let view = KeyHostingView(rootView: content())
        view.onKey = onKey
        view.onKeyUp = onKeyUp
        view.onUndo = onUndo
        DispatchQueue.main.async {
            view.window?.makeFirstResponder(view)
        }
        return view
    }

    func updateNSView(_ nsView: KeyHostingView<Content>, context: Context) {
        nsView.rootView = content()
        nsView.onKey = onKey
        nsView.onKeyUp = onKeyUp
        nsView.onUndo = onUndo
        guard context.coordinator.lastFocusToken != focusToken else {
            return
        }
        context.coordinator.lastFocusToken = focusToken
        DispatchQueue.main.async {
            nsView.window?.makeFirstResponder(nsView)
        }
    }

    final class Coordinator {
        var lastFocusToken: Int?
    }

    final class KeyHostingView<Root: View>: NSHostingView<Root> {
        var onKey: ((String) -> Void)?
        var onKeyUp: ((String) -> Void)?
        var onUndo: (() -> Void)?

        override var acceptsFirstResponder: Bool { true }

        override func mouseDown(with event: NSEvent) {
            window?.makeFirstResponder(self)
            super.mouseDown(with: event)
        }

        override func keyDown(with event: NSEvent) {
            if event.isUndoShortcut {
                onUndo?()
                return
            }
            if event.keyCode == 49 {
                onKey?("space")
                return
            }
            guard let characters = event.charactersIgnoringModifiers?.lowercased(), !characters.isEmpty else {
                return
            }
            onKey?(characters)
        }

        override func keyUp(with event: NSEvent) {
            if event.keyCode == 49 {
                onKeyUp?("space")
                return
            }
            guard let characters = event.charactersIgnoringModifiers?.lowercased(), !characters.isEmpty else {
                return
            }
            onKeyUp?(characters)
        }
    }
}

private extension NSEvent {
    var isUndoShortcut: Bool {
        guard let characters = charactersIgnoringModifiers?.lowercased(), characters == "z" else {
            return false
        }
        let relevantModifiers = modifierFlags.intersection([.command, .control, .option, .shift])
        return relevantModifiers == .command || relevantModifiers == .control
    }
}

struct FumenEditorPane: View {
    @State private var lastPaintedIndex: Int?
    @State private var strokePaintValue: Int?
    @State private var outputWebView = WKWebView()

    @ObservedObject var model: AppModel
    let screenshotAction: () -> Void
    let detectOpeningAction: () -> Void
    let detectPlayableOpeningAction: ([Int], String?, @escaping ([OpeningDetectionResult]) -> Void) -> Void
    let runAction: () -> Void
    let loadOpeningAction: (FumenOpeningPreset) -> Void
    let addCoverFumenAction: () -> Void
    let replaceCoverFumenAction: () -> Void
    let loadCoverFumenAction: (String) -> Void
    let commitAction: () -> Void
    let previewFumenAction: (String) -> Void

    private let columns = Array(repeating: GridItem(.fixed(24), spacing: 2), count: 10)
    private let visibleCellIndices = Array(fumenVisibleTopRow * 10..<fumenVisibleBottomRow * 10)
    private let cellSize: CGFloat = 24
    private let cellSpacing: CGFloat = 2
    private var boardSize: CGSize {
        CGSize(width: 10 * cellSize + 9 * cellSpacing, height: CGFloat(fumenVisibleRows) * cellSize + CGFloat(fumenVisibleRows - 1) * cellSpacing)
    }
    private var isSetupMode: Bool {
        model.command == .setup
    }
    private var isCoverMode: Bool {
        model.command == .cover
    }
    private var activePalette: [FumenPaletteItem] {
        isSetupMode ? setupFumenPalette : fumenPalette
    }
    private var selectedCoverFumenCode: Binding<String> {
        Binding(
            get: {
                guard model.coverFumenCodes.indices.contains(model.selectedCoverFumenIndex) else { return "" }
                return model.coverFumenCodes[model.selectedCoverFumenIndex]
            },
            set: { value in
                guard model.coverFumenCodes.indices.contains(model.selectedCoverFumenIndex) else { return }
                model.coverFumenCodes[model.selectedCoverFumenIndex] = value
            }
        )
    }
    private let outputHTMLCSS = """
    html, body {
        background: #1f1f1f !important;
        color: #eeeeee !important;
        font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", "Segoe UI", "Helvetica Neue", Arial, sans-serif !important;
        font-size: 14px !important;
        line-height: 1.45 !important;
    }
    body {
        margin: 14px !important;
    }
    h1, h2, h3, h4, th {
        color: #ffffff !important;
        font-weight: 650 !important;
    }
    p, div, td, li, pre, code {
        color: #eeeeee !important;
    }
    a {
        color: #8cc4ff !important;
    }
    table {
        border-color: #555555 !important;
    }
    """

    @ViewBuilder
    private var outputHTMLView: some View {
        if let url = model.embeddedOutputURL {
            WebView(webView: outputWebView, url: url, injectedCSS: outputHTMLCSS, onFumenLink: previewFumenAction)
                .background(Color(nsColor: .textBackgroundColor))
                .clipShape(RoundedRectangle(cornerRadius: 8))
                .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))
        } else {
            VStack(spacing: 8) {
                Image(systemName: "doc.richtext")
                    .font(.largeTitle)
                    .foregroundStyle(.secondary)
                Text("Open an HTML output file to view it here.")
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color(nsColor: .textBackgroundColor))
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))
        }
    }

    private var previewBoardView: some View {
        ZStack(alignment: .topLeading) {
            LazyVGrid(columns: columns, spacing: 2) {
                ForEach(visibleCellIndices, id: \.self) { index in
                    Rectangle()
                        .fill(previewBoardColor(for: index))
                        .overlay(Rectangle().stroke(Color(nsColor: .separatorColor), lineWidth: 0.5))
                        .frame(width: cellSize, height: cellSize)
                }
            }
            .padding(8)
            .background(Color.black)
            .clipShape(RoundedRectangle(cornerRadius: 8))
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))
        }
        .frame(width: boardSize.width + 16, height: boardSize.height + 16)
    }

    var body: some View {
        VStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 8) {
                Picker("View", selection: $model.fumenPanelTab) {
                    ForEach(FumenPanelTab.allCases) { tab in
                        Text(tab.rawValue).tag(tab)
                    }
                }
                .labelsHidden()
                .pickerStyle(.segmented)
                .frame(maxWidth: 280)

                ViewThatFits(in: .horizontal) {
                    HStack(spacing: 10) {
                        fumenToolbarControls
                    }
                    VStack(alignment: .leading, spacing: 8) {
                        fumenToolbarControls
                    }
                }
            }

            if model.fumenPanelTab == .output {
                outputHTMLView
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if model.fumenPanelTab == .play {
                ScrollView {
                    PlayableGameView(
                        model: model,
                        screenshotAction: screenshotAction,
                        commitAction: commitAction,
                        detectOpeningAction: detectPlayableOpeningAction
                    )
                        .frame(maxWidth: .infinity, alignment: .topLeading)
                }
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: 12) {
                        ViewThatFits(in: .horizontal) {
                            HStack(alignment: .top, spacing: 14) {
                                boardAndOpeningsColumn
                                fumenSideControls
                            }
                            VStack(alignment: .leading, spacing: 14) {
                                boardAndOpeningsColumn
                                fumenSideControls
                            }
                        }
                        if model.fumenPanelTab == .editor && !isSetupMode {
                            openingsSection
                                .frame(maxWidth: .infinity)
                        }
                        if model.fumenPanelTab == .editor && isCoverMode {
                            coverFumensSection
                                .frame(maxWidth: .infinity)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .topLeading)
                }
            }
        }
        .padding(14)
        .frame(minWidth: boardSize.width + 44, maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .onChange(of: model.command) { command in
            if command == .setup {
                model.importScreenshotColors = false
            }
        }
    }

    private var fumenToolbarControls: some View {
        Group {
            Toggle(isSetupMode ? "Screenshot as gray" : "Screenshot colors", isOn: $model.importScreenshotColors)
                .toggleStyle(.switch)
                .disabled(isSetupMode)
            Button {
                screenshotAction()
            } label: {
                Label("Screenshot", systemImage: "camera.viewfinder")
            }
            if model.fumenPanelTab != .play {
                Button {
                    runAction()
                } label: {
                    Label(model.isRunning ? "Running..." : "Run Search", systemImage: "play.fill")
                }
                .buttonStyle(.borderedProminent)
                .disabled(model.isRunning)
                if model.isRunning {
                    Button {
                        model.cancelRun()
                    } label: {
                        Label("Cancel", systemImage: "stop.fill")
                    }
                    .keyboardShortcut(.cancelAction)
                }
            }
        }
    }

    private var fumenSideControls: some View {
        VStack(alignment: .leading, spacing: 14) {
            if model.fumenPanelTab == .preview {
                previewControls
            } else {
                GroupBox(isSetupMode ? "Setup Marks" : "Color") {
                    VStack(alignment: .leading, spacing: 8) {
                        HStack(spacing: 8) {
                            ForEach(activePalette, id: \.value) { item in
                                Button {
                                    model.selectedFumenColor = item.value
                                } label: {
                                    Circle()
                                        .fill(item.color)
                                        .frame(width: 24, height: 24)
                                        .overlay(Circle().stroke(model.selectedFumenColor == item.value ? Color.accentColor : Color(nsColor: .separatorColor), lineWidth: model.selectedFumenColor == item.value ? 3 : 1))
                                }
                                .buttonStyle(.plain)
                            }
                        }
                        Toggle("Fill row", isOn: $model.fumenRowFill)
                            .toggleStyle(.checkbox)
                        if isSetupMode {
                            Text("I = fill target, O = margin, gray = fixed block. Click the same mark again to erase.")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    .padding(.vertical, 4)
                }

                if !isSetupMode {
                    GroupBox("Mino") {
                        VStack(alignment: .leading, spacing: 8) {
                            HStack {
                                Toggle("Place", isOn: $model.fumenPlaceMino)
                                    .toggleStyle(.checkbox)
                                Toggle("Color", isOn: .constant(true))
                                    .toggleStyle(.checkbox)
                                    .disabled(true)
                            }
                            HStack(spacing: 7) {
                                ForEach(fumenPieces, id: \.type) { piece in
                                    Button {
                                        model.setFumenOperationType(piece.type)
                                        commitAction()
                                    } label: {
                                        piecePreview(piece)
                                            .frame(width: 32, height: 46)
                                            .overlay(RoundedRectangle(cornerRadius: 4).stroke(model.fumenOperation.type == piece.type ? Color.accentColor : Color.clear, lineWidth: 2))
                                    }
                                    .buttonStyle(.plain)
                                }
                            }
                            HStack(spacing: 8) {
                                Button {
                                    model.moveFumenOperation(dx: -1, dy: 0)
                                    commitAction()
                                } label: {
                                    Label("Left", systemImage: "arrow.left")
                                }
                                Button {
                                    model.moveFumenOperation(dx: 0, dy: 1)
                                    commitAction()
                                } label: {
                                    Label("Down", systemImage: "arrow.down")
                                }
                                Button {
                                    model.moveFumenOperation(dx: 0, dy: -1)
                                    commitAction()
                                } label: {
                                    Label("Up", systemImage: "arrow.up")
                                }
                                Button {
                                    model.moveFumenOperation(dx: 1, dy: 0)
                                    commitAction()
                                } label: {
                                    Label("Right", systemImage: "arrow.right")
                                }
                            }
                            HStack(spacing: 8) {
                                Button {
                                    model.rotateFumenOperation(1)
                                    commitAction()
                                } label: {
                                    Label("CW", systemImage: "rotate.right")
                                }
                                Button {
                                    model.rotateFumenOperation(-1)
                                    commitAction()
                                } label: {
                                    Label("CCW", systemImage: "rotate.left")
                                }
                                Button {
                                    model.clearFumenOperation()
                                    commitAction()
                                } label: {
                                    Label("Clear", systemImage: "xmark")
                                }
                            }
                        }
                        .padding(.vertical, 4)
                    }

                    GroupBox("Pages") {
                        VStack(alignment: .leading, spacing: 8) {
                            HStack(spacing: 8) {
                                Button {
                                    model.goToFumenPage(model.currentFumenPage - 1)
                                    commitAction()
                                } label: {
                                    Label("Previous", systemImage: "chevron.left")
                                }
                                .disabled(model.currentFumenPage == 0)

                                Text(model.fumenPageLabel)
                                    .font(.headline.monospacedDigit())
                                    .frame(minWidth: 52)

                                Button {
                                    model.goToFumenPage(model.currentFumenPage + 1)
                                    commitAction()
                                } label: {
                                    Label("Next", systemImage: "chevron.right")
                                }
                                .disabled(model.currentFumenPage >= model.fumenPages.count - 1)
                            }
                            VStack(alignment: .leading, spacing: 8) {
                                Button {
                                    model.addFumenPage()
                                    commitAction()
                                } label: {
                                    Label("Add", systemImage: "plus")
                                }
                                HStack(spacing: 8) {
                                    Button(role: .destructive) {
                                        model.deletePreviousFumenPages()
                                        commitAction()
                                    } label: {
                                        Label("Trim Before", systemImage: "scissors")
                                    }
                                    .disabled(model.currentFumenPage == 0)

                                    Button(role: .destructive) {
                                        model.deleteFollowingFumenPages()
                                        commitAction()
                                    } label: {
                                        Label("Trim After", systemImage: "scissors")
                                    }
                                    .disabled(model.currentFumenPage >= model.fumenPages.count - 1)
                                }
                            }
                            TextField("Comment", text: $model.fumenComment)
                                .textFieldStyle(.roundedBorder)
                                .onChange(of: model.fumenComment) { _ in
                                    model.saveCurrentFumenPage()
                                    commitAction()
                                }
                        }
                        .padding(.vertical, 4)
                    }

                }

                boardMoveControls

                Button(role: .destructive) {
                    model.clearFumenCells()
                    commitAction()
                } label: {
                    Label("Clear Board", systemImage: "trash")
                }
            }
        }
        .frame(minWidth: 260, idealWidth: 310, maxWidth: 340, alignment: .topLeading)
    }

    private var boardMoveControls: some View {
        GroupBox("Move") {
            VStack(spacing: 8) {
                HStack {
                    Spacer(minLength: 0)
                    Button {
                        model.shiftFumenCells(dx: 0, dy: -1)
                        commitAction()
                    } label: {
                        Label("Up", systemImage: "arrow.up")
                    }
                    Spacer(minLength: 0)
                }
                HStack(spacing: 8) {
                    Button {
                        model.shiftFumenCells(dx: -1, dy: 0)
                        commitAction()
                    } label: {
                        Label("Left", systemImage: "arrow.left")
                    }
                    Button {
                        model.shiftFumenCells(dx: 0, dy: 1)
                        commitAction()
                    } label: {
                        Label("Down", systemImage: "arrow.down")
                    }
                    Button {
                        model.shiftFumenCells(dx: 1, dy: 0)
                        commitAction()
                    } label: {
                        Label("Right", systemImage: "arrow.right")
                    }
                }
                Button {
                    if isSetupMode {
                        model.mirrorSetupCells()
                    } else {
                        model.mirrorFumenCells()
                    }
                    commitAction()
                } label: {
                    Label("Mirror", systemImage: "arrow.left.and.right")
                }
            }
            .padding(.vertical, 4)
        }
    }

    private var boardAndOpeningsColumn: some View {
        VStack(alignment: .leading, spacing: 12) {
            if model.fumenPanelTab == .preview {
                previewBoardView
            } else {
                ZStack(alignment: .topLeading) {
                    LazyVGrid(columns: columns, spacing: 2) {
                        ForEach(visibleCellIndices, id: \.self) { index in
                            Rectangle()
                                .fill(boardColor(for: index))
                                .overlay(Rectangle().stroke(Color(nsColor: .separatorColor), lineWidth: 0.5))
                                .frame(width: cellSize, height: cellSize)
                        }
                    }
                    .padding(8)
                    .background(Color.black)
                    .clipShape(RoundedRectangle(cornerRadius: 8))
                    .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))

                    GeometryReader { proxy in
                        Rectangle()
                            .fill(Color.clear)
                            .contentShape(Rectangle())
                            .gesture(
                                DragGesture(minimumDistance: 0)
                                    .onChanged { value in
                                        paintCell(at: value.location, in: proxy.size)
                                    }
                                    .onEnded { _ in
                                        lastPaintedIndex = nil
                                        strokePaintValue = nil
                                    }
                            )
                    }
                    .padding(8)
                    .frame(width: boardSize.width + 16, height: boardSize.height + 16)
                }
                .frame(width: boardSize.width + 16, height: boardSize.height + 16)
            }
        }
        .frame(width: boardSize.width + 16, alignment: .topLeading)
    }

    private var openingsSection: some View {
        GroupBox("Openings") {
            VStack(alignment: .leading, spacing: 8) {
                ViewThatFits(in: .horizontal) {
                    HStack(spacing: 10) {
                        openingPickers
                    }
                    VStack(alignment: .leading, spacing: 8) {
                        openingPickers
                    }
                }

                ViewThatFits(in: .horizontal) {
                    HStack(spacing: 8) {
                        openingButtons
                    }
                    VStack(alignment: .leading, spacing: 8) {
                        openingButtons
                    }
                }
            }
            .padding(.vertical, 4)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var openingPickers: some View {
        Group {
            Picker("Opener", selection: $model.selectedOpeningGroupID) {
                ForEach(fumenOpeningGroups) { group in
                    Text(group.name).tag(group.id)
                }
            }
            .onChange(of: model.selectedOpeningGroupID) { groupID in
                if let group = fumenOpeningGroups.first(where: { $0.id == groupID }),
                   let preset = group.presets.first {
                    model.selectedOpeningVariationID = preset.id
                    model.selectedOpeningPresetID = preset.id
                }
            }
            .frame(minWidth: 220)

            Picker("Variation", selection: $model.selectedOpeningVariationID) {
                let variations = fumenOpeningGroups.first(where: { $0.id == model.selectedOpeningGroupID })?.presets ?? []
                ForEach(variations) { preset in
                    Text(preset.variationName).tag(preset.id)
                }
            }
            .onChange(of: model.selectedOpeningVariationID) { presetID in
                model.selectedOpeningPresetID = presetID
            }
            .frame(minWidth: 180)
        }
    }

    private var openingButtons: some View {
        Group {
            Button {
                detectOpeningAction()
            } label: {
                Label("Opener Detector", systemImage: "magnifyingglass")
            }

            Button {
                if let preset = fumenOpeningPresets.first(where: { $0.id == model.selectedOpeningVariationID }) {
                    loadOpeningAction(preset)
                }
            } label: {
                Label("Load Opening", systemImage: "arrow.clockwise")
            }
            .disabled(fumenOpeningPresets.first(where: { $0.id == model.selectedOpeningVariationID }) == nil)
        }
    }

    private var coverFumensSection: some View {
        GroupBox("Cover Fumens") {
            VStack(alignment: .leading, spacing: 8) {
                ViewThatFits(in: .horizontal) {
                    HStack(spacing: 8) {
                        coverFumenControls
                    }
                    VStack(alignment: .leading, spacing: 8) {
                        coverFumenControls
                    }
                }

                TextEditor(text: selectedCoverFumenCode)
                    .font(.system(.caption, design: .monospaced))
                    .frame(height: 76)
                    .disabled(model.coverFumenCodes.isEmpty)
                    .overlay(RoundedRectangle(cornerRadius: 5).stroke(Color(nsColor: .separatorColor)))
            }
            .padding(.vertical, 4)
        }
    }

    private var coverFumenControls: some View {
        Group {
            Picker("Stored fumen", selection: $model.selectedCoverFumenIndex) {
                if model.coverFumenCodes.isEmpty {
                    Text("No stored fumens").tag(0)
                } else {
                    ForEach(model.coverFumenCodes.indices, id: \.self) { index in
                        Text("Fumen \(index + 1)").tag(index)
                    }
                }
            }
            .labelsHidden()
            .frame(width: 180)
            .disabled(model.coverFumenCodes.isEmpty)

            Text("\(model.coverFumenCodes.count) stored")
                .font(.caption.monospacedDigit())
                .foregroundStyle(.secondary)

            Button {
                addCoverFumenAction()
            } label: {
                Label("Add", systemImage: "plus")
            }
            Button {
                replaceCoverFumenAction()
            } label: {
                Label("Replace", systemImage: "arrow.triangle.2.circlepath")
            }
            .disabled(model.coverFumenCodes.isEmpty)
            Button {
                if model.coverFumenCodes.indices.contains(model.selectedCoverFumenIndex) {
                    loadCoverFumenAction(model.coverFumenCodes[model.selectedCoverFumenIndex])
                }
            } label: {
                Label("Load", systemImage: "square.and.arrow.down")
            }
            .disabled(model.coverFumenCodes.isEmpty)
            Button(role: .destructive) {
                model.removeSelectedCoverFumen()
            } label: {
                Label("Remove", systemImage: "trash")
            }
            .disabled(model.coverFumenCodes.isEmpty)
        }
    }

    private func paintCell(at location: CGPoint, in size: CGSize) {
        guard location.x >= 0, location.y >= 0, location.x <= size.width, location.y <= size.height else { return }
        let column = Int(location.x / (cellSize + cellSpacing))
        let visibleRow = Int(location.y / (cellSize + cellSpacing))
        guard (0..<10).contains(column), (0..<fumenVisibleRows).contains(visibleRow) else { return }

        let row = visibleRow + fumenVisibleTopRow
        let index = row * 10 + column
        guard index != lastPaintedIndex else { return }
        lastPaintedIndex = index
        if model.fumenPlaceMino && !isSetupMode {
            model.setFumenOperationPosition(index)
        } else {
            let selectedValue = isSetupMode && ![1, 3, 8].contains(model.selectedFumenColor) ? 1 : model.selectedFumenColor
            let value = strokePaintValue ?? (model.nativeFumenCells[index] == selectedValue ? 0 : selectedValue)
            strokePaintValue = value
            model.paintFumenCell(index, value: value)
        }
        commitAction()
    }

    private func boardColor(for index: Int) -> Color {
        if model.fumenPlaceMino,
           fumenOperationCells(model.fumenOperation).contains(index),
           let piece = fumenPieces.first(where: { $0.type == model.fumenOperation.type }) {
            return piece.color
        }
        return fumenCellColor(model.nativeFumenCells[index])
    }

    private func previewBoardColor(for index: Int) -> Color {
        let page = model.previewFumenPages.indices.contains(model.currentPreviewFumenPage)
            ? model.previewFumenPages[model.currentPreviewFumenPage]
            : Array(repeating: 0, count: 240)
        let operation = model.previewFumenOperations.indices.contains(model.currentPreviewFumenPage)
            ? model.previewFumenOperations[model.currentPreviewFumenPage]
            : FumenOperation()

        if operation.type > 0,
           fumenOperationCells(operation).contains(index),
           let piece = fumenPieces.first(where: { $0.type == operation.type }) {
            return piece.color
        }
        return fumenCellColor(page.indices.contains(index) ? page[index] : 0)
    }

    private var previewControls: some View {
        GroupBox("Preview") {
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 8) {
                    Button {
                        model.goToPreviewFumenPage(model.currentPreviewFumenPage - 1)
                    } label: {
                        Label("Previous", systemImage: "chevron.left")
                    }
                    .disabled(model.currentPreviewFumenPage == 0)

                    Text(model.previewFumenPageLabel)
                        .font(.headline.monospacedDigit())
                        .frame(minWidth: 52)

                    Button {
                        model.goToPreviewFumenPage(model.currentPreviewFumenPage + 1)
                    } label: {
                        Label("Next", systemImage: "chevron.right")
                    }
                    .disabled(model.currentPreviewFumenPage >= model.previewFumenPages.count - 1)
                }

                if model.previewFumenComments.indices.contains(model.currentPreviewFumenPage),
                   !model.previewFumenComments[model.currentPreviewFumenPage].isEmpty {
                    Text(model.previewFumenComments[model.currentPreviewFumenPage])
                        .font(.callout)
                        .foregroundStyle(.secondary)
                        .lineLimit(3)
                }

                if model.previewFumenCode.isEmpty {
                    Text("Click a fumen link in the Output tab to preview it here.")
                        .font(.callout)
                        .foregroundStyle(.secondary)
                } else {
                    Button {
                        model.sendPreviewToEditor()
                        commitAction()
                    } label: {
                        Label("Send to Editor", systemImage: "square.and.arrow.down")
                    }
                    Button {
                        NSPasteboard.general.clearContents()
                        NSPasteboard.general.setString(model.previewFumenCode, forType: .string)
                        model.status = "Preview fumen code copied"
                    } label: {
                        Label("Copy Fumen Code", systemImage: "doc.on.doc")
                    }
                }
            }
            .padding(.vertical, 4)
        }
    }

    private func piecePreview(_ piece: FumenPiece) -> some View {
        let previewColumns = Array(repeating: GridItem(.fixed(6), spacing: 1), count: 4)
        let cells = Set((fumenPieceOffsets[piece.type]?[0] ?? []).map { $0.y * 4 + $0.x })
        return LazyVGrid(columns: previewColumns, spacing: 1) {
            ForEach(0..<12, id: \.self) { index in
                Rectangle()
                    .fill(cells.contains(index) ? piece.color : Color.clear)
                    .frame(width: 6, height: 6)
            }
        }
    }
}

struct ContentView: View {
    @StateObject private var model = AppModel()
    @State private var webView = WKWebView()
    @State private var autoImportTask: Task<Void, Never>?
    @State private var lastAutoImportedFumenCode = ""
    @State private var fumenImportToken = 0
    @State private var fumenPreviewImportToken = 0

    var body: some View {
        VStack(spacing: 0) {
            HSplitView {
                LeftPane(model: model)
                    .frame(minWidth: 260, idealWidth: 315, maxWidth: 380)

                FumenEditorPane(
                    model: model,
                    screenshotAction: takeBoardScreenshot,
                    detectOpeningAction: detectOpeningFromScreenshot,
                    detectPlayableOpeningAction: detectPlayableOpening,
                    runAction: {
                        exportFumenCode {
                            model.inputSource = .fumen
                            model.run()
                        }
                    },
                    loadOpeningAction: loadOpeningPreset,
                    addCoverFumenAction: {
                        exportFumenCode {
                            model.addCoverFumen(model.fumenCode)
                        }
                    },
                    replaceCoverFumenAction: {
                        exportFumenCode {
                            model.replaceSelectedCoverFumen(model.fumenCode)
                        }
                    },
                    loadCoverFumenAction: { code in
                        model.fumenCode = code
                        importFumenCode(codeOverride: code, statusMessage: "Loaded cover fumen")
                    },
                    commitAction: commitNativeFumen,
                    previewFumenAction: previewFumenCode
                )
                .frame(minWidth: 320, idealWidth: 620, maxWidth: .infinity)

                RightPane(model: model)
                    .frame(minWidth: 260, idealWidth: 315, maxWidth: 380)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            Divider()
            FooterView(model: model)
        }
        .background(Color(nsColor: .windowBackgroundColor))
        .background(
            WebView(webView: webView, url: model.fumenURL)
                .frame(width: 1, height: 1)
                .opacity(0.01)
                .accessibilityHidden(true)
        )
        .onAppear {
            reloadFumen()
        }
        .onChange(of: model.fumenCode) { newValue in
            scheduleAutoImport(for: newValue)
        }
    }

    private func reloadFumen() {
        webView.loadFileURL(model.fumenURL, allowingReadAccessTo: model.fumenURL.deletingLastPathComponent())
        model.status = "Fumen editor loaded"
    }

    private func takeBoardScreenshot() {
        autoImportTask?.cancel()
        let destination = FileManager.default.temporaryDirectory
            .appendingPathComponent("solution-finder-board-\(UUID().uuidString).png")
        model.status = "Drag around the Tetris board to import it"
        NSApp.activate(ignoringOtherApps: true)

        DispatchQueue.global(qos: .userInitiated).async {
            let process = Process()
            process.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
            process.arguments = ["-i", "-x", destination.path]

            do {
                try process.run()
                process.waitUntilExit()
            } catch {
                DispatchQueue.main.async {
                    model.status = "Screenshot failed: \(error.localizedDescription)"
                }
                return
            }

            DispatchQueue.main.async {
                guard process.terminationStatus == 0,
                      FileManager.default.fileExists(atPath: destination.path) else {
                    model.status = "Screenshot canceled"
                    return
                }
                guard let image = NSImage(contentsOf: destination),
                      let cells = fumenCells(fromBoardImage: image, preservingColors: model.importScreenshotColors) else {
                    model.status = "Could not read screenshot as a board"
                    return
                }

                model.replaceCurrentFumenCells(cells)
                model.inputSource = .fumen
                model.status = "Imported board screenshot"
                commitNativeFumen()
            }
        }
    }

    private func fumenEditorSnapshot() -> FumenEditorSnapshot {
        model.saveCurrentFumenPage()
        return FumenEditorSnapshot(
            fumenCode: model.fumenCode,
            nativeFumenCells: model.nativeFumenCells,
            fumenPages: model.fumenPages,
            fumenComments: model.fumenComments,
            fumenOperations: model.fumenOperations,
            currentFumenPage: model.currentFumenPage,
            fumenComment: model.fumenComment,
            fumenOperation: model.fumenOperation,
            fumenPlaceMino: model.fumenPlaceMino,
            inputSource: model.inputSource,
            fumenPanelTab: model.fumenPanelTab,
            selectedOpeningPresetID: model.selectedOpeningPresetID,
            selectedOpeningGroupID: model.selectedOpeningGroupID,
            selectedOpeningVariationID: model.selectedOpeningVariationID
        )
    }

    private func restoreFumenEditorSnapshot(_ snapshot: FumenEditorSnapshot, status: String = "Restored previous fumen") {
        model.fumenCode = snapshot.fumenCode
        model.nativeFumenCells = snapshot.nativeFumenCells
        model.fumenPages = snapshot.fumenPages
        model.fumenComments = snapshot.fumenComments
        model.fumenOperations = snapshot.fumenOperations
        model.currentFumenPage = snapshot.currentFumenPage
        model.fumenComment = snapshot.fumenComment
        model.fumenOperation = snapshot.fumenOperation
        model.fumenPlaceMino = snapshot.fumenPlaceMino
        model.inputSource = snapshot.inputSource
        model.fumenPanelTab = snapshot.fumenPanelTab
        model.selectedOpeningPresetID = snapshot.selectedOpeningPresetID
        model.selectedOpeningGroupID = snapshot.selectedOpeningGroupID
        model.selectedOpeningVariationID = snapshot.selectedOpeningVariationID
        model.updateCommandHeightIfNeeded(force: true)
        lastAutoImportedFumenCode = snapshot.fumenCode
        model.status = status
        commitNativeFumen()
    }

    private func detectOpeningFromScreenshot() {
        autoImportTask?.cancel()
        let snapshot = fumenEditorSnapshot()
        let destination = FileManager.default.temporaryDirectory
            .appendingPathComponent("solution-finder-opener-\(UUID().uuidString).png")
        model.status = "Drag around the opener to identify it"
        NSApp.activate(ignoringOtherApps: true)

        DispatchQueue.global(qos: .userInitiated).async {
            let process = Process()
            process.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
            process.arguments = ["-i", "-x", destination.path]

            do {
                try process.run()
                process.waitUntilExit()
            } catch {
                DispatchQueue.main.async {
                    model.status = "Opener screenshot failed: \(error.localizedDescription)"
                }
                return
            }

            DispatchQueue.main.async {
                guard process.terminationStatus == 0,
                      FileManager.default.fileExists(atPath: destination.path) else {
                    model.status = "Opener detection canceled"
                    return
                }
                guard let image = NSImage(contentsOf: destination),
                      let cells = fumenCells(fromBoardImage: image, preservingColors: model.importScreenshotColors) else {
                    model.status = "Could not read screenshot as a board"
                    return
                }

                model.fumenPanelTab = .editor
                model.replaceCurrentFumenCells(cells)
                model.inputSource = .fumen
                commitNativeFumen()

                let editorCells = model.solverFumenCells()
                let targetMask = fumenOccupancyMask(editorCells)
                guard !targetMask.isEmpty else {
                    model.status = "No occupied cells found in screenshot"
                    return
                }

                let targetColors = fumenColorMask(editorCells)
                model.status = "Imported screenshot; detecting opener..."
                detectOpening(
                    in: fumenOpeningPresets.filter { $0.id != "empty" },
                    targetMask: targetMask,
                    targetColors: targetColors,
                    results: []
                ) { results in
                    logOpeningDetectionResults(source: "Screenshot opener detector", results: results)
                    guard let best = results.first else {
                        showNoOpeningMatchDialog(restoreSnapshot: snapshot)
                        return
                    }
                    let closeResults = Array(results.prefix(5)).filter { result in
                        result.overallScore >= 0.46 && best.overallScore - result.overallScore <= 0.10
                    }
                    if closeResults.count > 1 {
                        showOpeningChoiceDialog(closeResults, restoreSnapshot: snapshot)
                    } else {
                        showOpeningMatchDialog(best, restoreSnapshot: snapshot)
                    }
                }
            }
        }
    }

    private func detectOpening(
        in presets: [FumenOpeningPreset],
        targetMask: [[Bool]],
        targetColors: [[Int]],
        results: [OpeningDetectionResult],
        completion: @escaping ([OpeningDetectionResult]) -> Void
    ) {
        guard let preset = presets.first else {
            let rankedResults = results
                .filter { $0.score >= 0.42 || $0.overallScore >= 0.42 }
                .sorted { betterOpeningResult($0, than: $1) }

            guard !rankedResults.isEmpty else {
                let fallbackResults = results
                    .sorted { betterOpeningResult($0, than: $1) }
                completion(Array(fallbackResults.prefix(5)))
                return
            }

            completion(rankedResults)
            return
        }

        let remaining = Array(presets.dropFirst())
        let handlePages: ([[Int]]) -> Void = { pages in
            if let candidate = bestOpeningDetectionResult(
                preset: preset,
                pages: pages,
                targetMask: targetMask,
                targetColors: targetColors
            ) {
                detectOpening(in: remaining, targetMask: targetMask, targetColors: targetColors, results: results + [candidate], completion: completion)
            } else {
                detectOpening(in: remaining, targetMask: targetMask, targetColors: targetColors, results: results, completion: completion)
            }
        }

        if !preset.cells.isEmpty {
            handlePages([preset.cells])
        } else if let code = preset.code {
            decodeFumenCode(code) { pages in
                guard !pages.isEmpty else {
                    detectOpening(in: remaining, targetMask: targetMask, targetColors: targetColors, results: results, completion: completion)
                    return
                }
                handlePages(pages)
            }
        } else {
            detectOpening(in: remaining, targetMask: targetMask, targetColors: targetColors, results: results, completion: completion)
        }
    }

    private func detectPlayableOpening(cells: [Int], openerName: String?, completion: @escaping ([OpeningDetectionResult]) -> Void) {
        let targetMask = fumenOccupancyMask(cells)
        guard !targetMask.isEmpty else {
            completion([])
            return
        }
        let presets = fumenOpeningPresets.filter { preset in
            guard preset.id != "empty" else { return false }
            guard let openerName else { return true }
            return preset.openerName == openerName
        }
        detectOpening(
            in: presets,
            targetMask: targetMask,
            targetColors: fumenColorMask(cells),
            results: [],
            completion: { results in
                let strictResults = strictPlayableOpeningResults(results)
                logPlayableOpeningDetection(
                    rawResults: results,
                    strictResults: strictResults,
                    openerName: openerName
                )
                completion(strictResults)
            }
        )
    }

    private func strictPlayableOpeningResults(_ results: [OpeningDetectionResult]) -> [OpeningDetectionResult] {
        results
            .filter { result in
                guard result.occupancyScore >= 0.77,
                      result.overallScore >= 0.77
                else { return false }

                if result.comparableColorCells >= 6 {
                    return result.colorScore >= 0.70
                }

                return result.occupancyScore >= 0.85
            }
            .sorted { betterOpeningResult($0, than: $1) }
    }

    private func logPlayableOpeningDetection(rawResults: [OpeningDetectionResult], strictResults: [OpeningDetectionResult], openerName: String?) {
        let filterDescription = openerName.map { "filter=\($0)" } ?? "filter=all openers"
        let rawLines = Array(rawResults.sorted { betterOpeningResult($0, than: $1) }.prefix(10)).map { result in
            "\(playableDetectionVerdict(for: result)) \(playableDetectionSummary(result))"
        }
        let strictLines = strictResults.prefix(10).map(playableDetectionSummary)
        DetectionDebugLog.shared.appendBlock(
            title: "Play opener detection \(filterDescription): raw \(rawResults.count), passed \(strictResults.count)",
            lines: ["Raw candidates:"] + (rawLines.isEmpty ? ["  none"] : rawLines)
                + ["Passed candidates:"] + (strictLines.isEmpty ? ["  none"] : strictLines)
        )
    }

    private func logOpeningDetectionResults(source: String, results: [OpeningDetectionResult]) {
        let lines = Array(results.prefix(12)).map { result in
            "\(cleanOpeningName(for: result)) | \(playableDetectionSummary(result))"
        }
        DetectionDebugLog.shared.appendBlock(
            title: "\(source): \(results.count) candidate\(results.count == 1 ? "" : "s")",
            lines: lines.isEmpty ? ["none"] : lines
        )
    }

    private func playableDetectionVerdict(for result: OpeningDetectionResult) -> String {
        let shapeOK = result.occupancyScore >= 0.77
        let overallOK = result.overallScore >= 0.77
        let colorOK = result.comparableColorCells >= 6 ? result.colorScore >= 0.70 : result.occupancyScore >= 0.85
        let failed = [
            shapeOK ? nil : "shape",
            overallOK ? nil : "overall",
            colorOK ? nil : "color"
        ].compactMap { $0 }
        return failed.isEmpty ? "PASS" : "FAIL(\(failed.joined(separator: ",")))"
    }

    private func playableDetectionSummary(_ result: OpeningDetectionResult) -> String {
        let overall = String(format: "%.1f%%", result.overallScore * 100.0)
        let shape = String(format: "%.1f%%", result.occupancyScore * 100.0)
        let color = result.comparableColorCells >= 6 ? String(format: "%.1f%%", result.colorScore * 100.0) : "n/a"
        let mirror = result.mirrored ? " mirrored" : ""
        let page = result.pageIndex > 0 ? " page \(result.pageIndex + 1)" : ""
        let rowOffset = result.rowOffset == 0 ? "" : " rows \(result.rowOffset > 0 ? "+" : "")\(result.rowOffset)"
        return "\(result.preset.openerName) - \(result.preset.variationName)\(mirror)\(page)\(rowOffset) | overall \(overall) shape \(shape) color \(color) comparable \(result.comparableColorCells)"
    }

    private func showOpeningMatchDialog(_ result: OpeningDetectionResult, restoreSnapshot: FumenEditorSnapshot?) {
        let alert = NSAlert()
        alert.messageText = result.score >= 0.72 ? "Opener match found" : "Closest opener match"
        alert.informativeText = cleanOpeningName(for: result)
        alert.addButton(withTitle: "Add to Editor")
        alert.addButton(withTitle: "Close")
        let response = alert.runModal()
        if response == .alertFirstButtonReturn {
            loadDetectedOpening(result)
        } else {
            if let restoreSnapshot {
                restoreFumenEditorSnapshot(restoreSnapshot, status: "Kept previous fumen")
            } else {
                model.status = "Detected \(cleanOpeningName(for: result))"
            }
        }
    }

    private func showOpeningChoiceDialog(_ results: [OpeningDetectionResult], restoreSnapshot: FumenEditorSnapshot?) {
        let alert = NSAlert()
        let confident = results.first?.overallScore ?? 0 >= 0.58
        alert.messageText = confident ? "Choose opener match" : "Closest opener matches"
        alert.informativeText = ""

        let container = NSView(frame: NSRect(x: 0, y: 0, width: 560, height: 150))
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 10
        stack.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(stack)

        let promptText = confident
            ? "More than one opener looks plausible. Pick the one that matches your board."
            : "No confident match was found. These are the closest samples; pick one only if it looks right."
        let prompt = NSTextField(labelWithString: promptText)
        prompt.font = NSFont.systemFont(ofSize: 13)
        prompt.textColor = .labelColor
        prompt.lineBreakMode = .byWordWrapping
        prompt.maximumNumberOfLines = 2
        prompt.translatesAutoresizingMaskIntoConstraints = false

        let popup = NSPopUpButton(frame: NSRect(x: 0, y: 0, width: 540, height: 30), pullsDown: false)
        for (index, result) in results.enumerated() {
            let name = cleanOpeningName(for: result)
            let duplicateCount = results.prefix(index).filter { cleanOpeningName(for: $0) == name }.count
            popup.addItem(withTitle: duplicateCount == 0 ? name : "\(name) (\(duplicateCount + 1))")
        }
        popup.selectItem(at: 0)

        stack.addArrangedSubview(prompt)
        stack.addArrangedSubview(popup)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            stack.topAnchor.constraint(equalTo: container.topAnchor),
            stack.bottomAnchor.constraint(lessThanOrEqualTo: container.bottomAnchor),
            prompt.widthAnchor.constraint(equalToConstant: 540),
            popup.widthAnchor.constraint(equalToConstant: 540)
        ])

        alert.accessoryView = container
        alert.addButton(withTitle: "Add to Editor")
        alert.addButton(withTitle: "Close")
        let response = alert.runModal()
        if response == .alertFirstButtonReturn {
            let selectedIndex = max(0, popup.indexOfSelectedItem)
            loadDetectedOpening(results[min(selectedIndex, results.count - 1)])
        } else if let restoreSnapshot {
            restoreFumenEditorSnapshot(restoreSnapshot, status: "Kept previous fumen")
        } else if let first = results.first {
            model.status = "Detected possible matches; top was \(cleanOpeningName(for: first))"
        }
    }

    private func showNoOpeningMatchDialog(restoreSnapshot: FumenEditorSnapshot?) {
        let alert = NSAlert()
        alert.messageText = "No opener match found"
        alert.informativeText = "The screenshot did not match any opener samples."
        alert.addButton(withTitle: "Close")
        alert.runModal()
        if let restoreSnapshot {
            restoreFumenEditorSnapshot(restoreSnapshot, status: "No confident opener match; restored previous fumen")
        } else {
            model.status = "No confident opener match"
        }
    }

    private func loadDetectedOpening(_ result: OpeningDetectionResult) {
        let detectedName = cleanOpeningName(for: result)

        if let code = result.preset.code {
            lastAutoImportedFumenCode = code
            model.fumenCode = code
            importFumenCode(codeOverride: code, statusMessage: "Detected \(detectedName)") {
                if result.pageIndex > 0 {
                    model.goToFumenPage(min(result.pageIndex, model.fumenPages.count - 1))
                }
                if result.mirrored {
                    model.mirrorFumenCells()
                    commitNativeFumen()
                }
                model.selectOpeningPreset(result.preset)
                model.status = "Detected \(detectedName)"
            }
            return
        }

        model.replaceFumenPages(
            [result.preset.cells],
            comments: [result.preset.name],
            operations: [FumenOperation()],
            selectedPage: 0
        )
        if result.mirrored {
            model.mirrorFumenCells()
        }
        model.inputSource = .fumen
        model.selectOpeningPreset(result.preset)
        model.status = "Detected \(detectedName)"
        commitNativeFumen()
    }

    private func cleanOpeningName(for result: OpeningDetectionResult) -> String {
        var name = result.preset.variationName == "Base"
            ? result.preset.openerName
            : "\(result.preset.openerName) - \(result.preset.variationName)"
        if result.mirrored {
            name += " mirrored"
        }
        return name
    }

    private func loadOpeningPreset(_ preset: FumenOpeningPreset) {
        autoImportTask?.cancel()
        model.selectOpeningPreset(preset)
        if let code = preset.code {
            lastAutoImportedFumenCode = code
            model.fumenCode = code
            importFumenCode(codeOverride: code, statusMessage: "Loaded \(preset.name)")
            return
        }

        model.replaceFumenPages(
            [preset.cells],
            comments: [preset.name],
            operations: [FumenOperation()],
            selectedPage: 0
        )
        model.inputSource = .fumen
        model.status = "Loaded \(preset.name)"
        commitNativeFumen()
    }

    private func importFumenCode(codeOverride: String? = nil, statusMessage: String = "Imported fumen code", completion: (() -> Void)? = nil) {
        let code = (codeOverride ?? model.fumenCode).trimmingCharacters(in: .whitespacesAndNewlines)
        guard !code.isEmpty else {
            model.status = "No fumen code to import"
            return
        }
        fumenImportToken += 1
        let importToken = fumenImportToken

        let js = """
        (function() {
          var tx = document.getElementById('tx');
          if (!tx) return 'Fumen input field was not found';
          if (typeof updateflag !== 'undefined') updateflag = 0;
          if (typeof newdata === 'function') newdata(0);
          if (typeof updateflag !== 'undefined') updateflag = 0;
          tx.value = \(javascriptStringLiteral(code));
          if (typeof versioncheck === 'function') versioncheck(0);
          var pages = [];
          var operations = [];
          for (var e = 0; e <= framemax; e++) {
            pages.push(Array.prototype.slice.call(af, e * fldblks, (e + 1) * fldblks));
            operations.push({ type: ap[e * 3 + 0] || 0, rotation: ap[e * 3 + 1] || 0, position: ap[e * 3 + 2] || 0 });
          }
          return {
            message: 'Imported fumen code',
            pages: pages,
            operations: operations,
            comments: Array.prototype.slice.call(ac, 0, framemax + 1),
            frame: frame
          };
        })();
        """

        webView.evaluateJavaScript(js) { value, error in
            DispatchQueue.main.async {
                guard importToken == fumenImportToken else { return }

                if let error {
                    model.status = "Import failed: \(error.localizedDescription)"
                } else {
                    if let result = value as? [String: Any],
                       let rawPages = result["pages"] as? [Any] {
                        let pages = rawPages.compactMap { page -> [Int]? in
                            guard let rawCells = page as? [Any] else { return nil }
                            return rawCells.compactMap { cell -> Int? in
                                if let number = cell as? NSNumber { return number.intValue }
                                return cell as? Int
                            }
                        }
                        let comments = (result["comments"] as? [Any])?.map { "\($0)" } ?? []
                        let operations = ((result["operations"] as? [Any]) ?? []).compactMap { item -> FumenOperation? in
                            guard let raw = item as? [String: Any] else { return nil }
                            let type = (raw["type"] as? NSNumber)?.intValue ?? 0
                            let rotation = (raw["rotation"] as? NSNumber)?.intValue ?? 0
                            let position = (raw["position"] as? NSNumber)?.intValue ?? 0
                            return FumenOperation(type: type, rotation: rotation, position: position)
                        }
                        let selectedPage = (result["frame"] as? NSNumber)?.intValue ?? 0
                        if pages.isEmpty {
                            model.replaceFumenPages([], comments: [], operations: [], selectedPage: 0)
                        } else {
                            model.replaceFumenPages(pages, comments: comments, operations: operations, selectedPage: selectedPage)
                        }
                    } else {
                        model.replaceFumenPages([], comments: [], operations: [], selectedPage: 0)
                    }
                    self.lastAutoImportedFumenCode = code
                    model.inputSource = .fumen
                    model.status = statusMessage
                    completion?()
                }
            }
        }
    }

    private func decodeFumenCode(_ code: String, completion: @escaping ([[Int]]) -> Void) {
        let trimmedCode = code.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedCode.isEmpty else {
            completion([])
            return
        }

        let js = """
        (function() {
          var tx = document.getElementById('tx');
          if (!tx) return [];
          if (typeof updateflag !== 'undefined') updateflag = 0;
          if (typeof newdata === 'function') newdata(0);
          if (typeof updateflag !== 'undefined') updateflag = 0;
          tx.value = \(javascriptStringLiteral(trimmedCode));
          if (typeof versioncheck === 'function') versioncheck(0);
          var pages = [];
          for (var e = 0; e <= framemax; e++) {
            pages.push(Array.prototype.slice.call(af, e * fldblks, (e + 1) * fldblks));
          }
          return pages;
        })();
        """

        webView.evaluateJavaScript(js) { value, _ in
            DispatchQueue.main.async {
                let pages = (value as? [Any])?.compactMap { page -> [Int]? in
                    guard let rawCells = page as? [Any] else { return nil }
                    return rawCells.compactMap { cell -> Int? in
                        if let number = cell as? NSNumber { return number.intValue }
                        return cell as? Int
                    }
                } ?? []
                completion(pages)
            }
        }
    }

    private func previewFumenCode(_ code: String) {
        let trimmedCode = code.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmedCode.hasPrefix("v115@") else {
            model.status = "No fumen code found in link"
            return
        }
        fumenPreviewImportToken += 1
        let previewToken = fumenPreviewImportToken
        model.status = "Loading fumen preview..."

        let js = """
        (function() {
          var tx = document.getElementById('tx');
          if (!tx) return 'Fumen input field was not found';
          if (typeof updateflag !== 'undefined') updateflag = 0;
          if (typeof newdata === 'function') newdata(0);
          if (typeof updateflag !== 'undefined') updateflag = 0;
          tx.value = \(javascriptStringLiteral(trimmedCode));
          if (typeof versioncheck === 'function') versioncheck(0);
          var pages = [];
          var operations = [];
          for (var e = 0; e <= framemax; e++) {
            pages.push(Array.prototype.slice.call(af, e * fldblks, (e + 1) * fldblks));
            operations.push({ type: ap[e * 3 + 0] || 0, rotation: ap[e * 3 + 1] || 0, position: ap[e * 3 + 2] || 0 });
          }
          return {
            pages: pages,
            operations: operations,
            comments: Array.prototype.slice.call(ac, 0, framemax + 1),
            frame: frame
          };
        })();
        """

        webView.evaluateJavaScript(js) { value, error in
            DispatchQueue.main.async {
                guard previewToken == fumenPreviewImportToken else { return }

                if let error {
                    model.status = "Preview failed: \(error.localizedDescription)"
                    return
                }

                guard let result = value as? [String: Any],
                      let rawPages = result["pages"] as? [Any] else {
                    model.status = "Preview failed: fumen could not be decoded"
                    return
                }

                let pages = rawPages.compactMap { page -> [Int]? in
                    guard let rawCells = page as? [Any] else { return nil }
                    return rawCells.compactMap { cell -> Int? in
                        if let number = cell as? NSNumber { return number.intValue }
                        return cell as? Int
                    }
                }
                let comments = (result["comments"] as? [Any])?.map { "\($0)" } ?? []
                let operations = ((result["operations"] as? [Any]) ?? []).compactMap { item -> FumenOperation? in
                    guard let raw = item as? [String: Any] else { return nil }
                    let type = (raw["type"] as? NSNumber)?.intValue ?? 0
                    let rotation = (raw["rotation"] as? NSNumber)?.intValue ?? 0
                    let position = (raw["position"] as? NSNumber)?.intValue ?? 0
                    return FumenOperation(type: type, rotation: rotation, position: position)
                }
                let selectedPage = (result["frame"] as? NSNumber)?.intValue ?? 0
                model.previewFumenCode = trimmedCode
                model.replacePreviewFumenPages(pages, comments: comments, operations: operations, selectedPage: selectedPage)
                model.fumenPanelTab = .preview
                model.status = "Previewing fumen link"
            }
        }
    }

    private func exportFumenCode(completion: (() -> Void)? = nil) {
        let js = nativeFumenCodecScript()

        webView.evaluateJavaScript(js) { value, error in
            DispatchQueue.main.async {
                if let error {
                    model.status = "Export failed: \(error.localizedDescription)"
                    return
                }

                model.fumenCode = value as? String ?? ""
                self.lastAutoImportedFumenCode = self.model.fumenCode
                model.inputSource = .fumen
                model.status = model.fumenCode.isEmpty ? "No fumen code exported" : "Fumen code exported"
                if !model.fumenCode.isEmpty {
                    completion?()
                }
            }
        }
    }

    private func commitNativeFumen() {
        guard webView.url != nil else { return }

        let js = nativeFumenCodecScript()

        webView.evaluateJavaScript(js) { value, error in
            guard error == nil, let code = value as? String, !code.isEmpty else { return }

            DispatchQueue.main.async {
                if model.fumenCode != code {
                    lastAutoImportedFumenCode = code
                    model.fumenCode = code
                    model.inputSource = .fumen
                }
            }
        }
    }

    private func scheduleAutoImport(for value: String) {
        let code = value.trimmingCharacters(in: .whitespacesAndNewlines)
        autoImportTask?.cancel()

        guard code.hasPrefix("v115@"), code != lastAutoImportedFumenCode else { return }

        autoImportTask = Task {
            try? await Task.sleep(nanoseconds: 650_000_000)
            guard !Task.isCancelled else { return }

            await MainActor.run {
                let currentCode = model.fumenCode.trimmingCharacters(in: .whitespacesAndNewlines)
                guard currentCode == code, currentCode != lastAutoImportedFumenCode else { return }
                importFumenCode(statusMessage: "Loaded fumen code")
            }
        }
    }

    private func nativeFumenCodecScript() -> String {
        let pages = "[" + model.solverFumenPages()
            .map { "[" + $0.map(String.init).joined(separator: ",") + "]" }
            .joined(separator: ",") + "]"
        let comments = "[" + model.fumenComments.map(javascriptStringLiteral).joined(separator: ",") + "]"
        model.saveCurrentFumenPage()
        let operations = "[" + model.fumenOperations.map { operation in
            "[\(operation.type),\(operation.rotation),\(operation.position)]"
        }.joined(separator: ",") + "]"
        let frame = model.currentFumenPage

        let js = """
        (function() {
          var tx = document.getElementById('tx');
          if (!tx) return '';
          var pages = \(pages);
          var comments = \(comments);
          var operations = \(operations);
          frame = Math.min(\(frame), Math.max(0, pages.length - 1));
          framemax = Math.max(0, pages.length - 1);
          for (var e = 0; e < pages.length; e++) {
            for (var i = 0; i < fldblks; i++) af[e * fldblks + i] = pages[e][i] || 0;
            ap[e * 3 + 0] = operations[e] ? operations[e][0] || 0 : 0;
            ap[e * 3 + 1] = operations[e] ? operations[e][1] || 0 : 0;
            ap[e * 3 + 2] = operations[e] ? operations[e][2] || 0 : 0;
            au[e] = 0;
            am[e] = 0;
            ac[e] = comments[e] || '';
            ad[e] = 0;
          }
          for (var i = 0; i < fldblks; i++) f[i] = pages[frame][i] || 0;
          if (typeof p !== 'undefined') p = operations[frame] ? [operations[frame][0] || 0, operations[frame][1] || 0, operations[frame][2] || 0] : [0, 0, 0];
          var up = document.getElementById('up'); if (up) up.checked = false;
          var mr = document.getElementById('mr'); if (mr) mr.checked = false;
          var dc = document.getElementById('dc'); if (dc) dc.checked = true;
          var cm = document.getElementById('cm'); if (cm) cm.value = comments[frame] || '';
          if (typeof refresh === 'function') refresh();
          if (typeof updated === 'function') updated();
          else if (typeof encode === 'function') encode(0);
          return tx.value || '';
        })();
        """
        return js
    }
}

struct LeftPane: View {
    @ObservedObject var model: AppModel
    @State private var commandGuideExpanded = false

    var body: some View {
        ScrollView {
            VStack(spacing: 12) {
                if model.fumenPanelTab == .play {
                    PlaySettingsView(model: model)
                } else {
                    SettingsView(model: model)
                    HStack(alignment: .top, spacing: 6) {
                        Image(systemName: "lightbulb")
                            .foregroundStyle(.secondary)
                        Text(model.command.tip)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                        Spacer(minLength: 0)
                    }
                    .padding(8)
                    .background(Color(nsColor: .controlBackgroundColor))
                    .clipShape(RoundedRectangle(cornerRadius: 6))
                    .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color(nsColor: .separatorColor)))

                    GroupBox("Patterns") {
                        VStack(alignment: .leading, spacing: 5) {
                            TextField("Pattern sequence", text: $model.patterns)
                                .font(.system(.body, design: .monospaced))
                                .textFieldStyle(.roundedBorder)
                            Text("Use commas for fixed steps, *pN for any N pieces, and brackets for a piece pool. Examples: T,*p5 or [IJLOS]p5.")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                    }
                }
                GroupBox("Fumen Code") {
                    VStack(spacing: 8) {
                        TextEditor(text: $model.fumenCode)
                            .font(.system(.body, design: .monospaced))
                            .frame(height: 64)
                        HStack {
                            Spacer()
                            Button {
                                NSPasteboard.general.clearContents()
                                NSPasteboard.general.setString(model.fumenCode, forType: .string)
                                model.status = "Fumen code copied"
                            } label: {
                                Label("Copy", systemImage: "doc.on.doc")
                            }
                        }
                    }
                }
                DisclosureGroup("Command Guide", isExpanded: $commandGuideExpanded) {
                    GroupBox {
                        VStack(alignment: .leading, spacing: 8) {
                            Text(model.command.syntax)
                                .font(.system(.caption, design: .monospaced))
                                .foregroundStyle(Color(white: 0.9))
                                .textSelection(.enabled)
                                .fixedSize(horizontal: false, vertical: true)

                            Divider()

                            ForEach(model.command.helpLines, id: \.self) { line in
                                HStack(alignment: .top, spacing: 6) {
                                    Text("-")
                                        .foregroundStyle(.secondary)
                                    Text(line)
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                        .fixedSize(horizontal: false, vertical: true)
                                }
                            }
                        }
                        .padding(.vertical, 4)
                    }
                }
                .font(.headline)
                Spacer(minLength: 0)
            }
            .padding(14)
            .frame(maxWidth: .infinity, alignment: .topLeading)
        }
    }
}

struct PlaySettingsView: View {
    @ObservedObject var model: AppModel
    @AppStorage("play.control.left") private var controlLeft = "a"
    @AppStorage("play.control.right") private var controlRight = "d"
    @AppStorage("play.control.softDrop") private var controlSoftDrop = "s"
    @AppStorage("play.control.hardDrop") private var controlHardDrop = "space"
    @AppStorage("play.control.rotateCW") private var controlRotateCW = "w"
    @AppStorage("play.control.rotateCCW") private var controlRotateCCW = "q"
    @AppStorage("play.control.rotate180") private var controlRotate180 = "e"
    @AppStorage("play.control.hold") private var controlHold = "c"
    @AppStorage("play.control.undo") private var controlUndo = "z"
    @AppStorage("play.control.reset") private var controlReset = "r"
    @AppStorage("play.tuning.das") private var tuningAutoShiftMs = 130.0
    @AppStorage("play.tuning.arr") private var tuningRepeatMs = 28.0
    @AppStorage("play.tuning.softDrop") private var tuningSoftDropMs = 75.0
    @AppStorage("play.tuning.gravity") private var tuningGravityMs = 1000.0
    @AppStorage("play.tuning.lockDelay") private var tuningLockDelayMs = 500.0
    @AppStorage("play.tuning.gravityEnabled") private var gravityEnabled = true
    @AppStorage("play.tuning.infiniteLockDelay") private var infiniteLockDelay = false
    @AppStorage("play.tuning.infiniteHold") private var infiniteHold = false
    @AppStorage("play.previewCellSize") private var previewCellSize = 14.0
    @AppStorage("play.customQueue") private var customQueue = ""
    @AppStorage("play.customHold") private var customHold = "-"
    @AppStorage("play.exportIncludeActive") private var exportIncludeActive = false

    var body: some View {
        VStack(spacing: 12) {
            GroupBox("Game Settings") {
                VStack(alignment: .leading, spacing: 8) {
                    TextField("Queue, e.g. TILJSZO", text: $customQueue)
                        .textFieldStyle(.roundedBorder)
                    HStack {
                        Picker("Hold", selection: $customHold) {
                            Text("-").tag("-")
                            ForEach(["I", "L", "O", "Z", "T", "J", "S"], id: \.self) { piece in
                                Text(piece).tag(piece)
                            }
                        }
                        Button("Apply") {
                            model.playSettingsApplyToken += 1
                        }
                    }
                    Button {
                        customQueue = ""
                        customHold = "-"
                        model.playSettingsApplyToken += 1
                    } label: {
                        Label("Random 7-bag", systemImage: "shuffle")
                    }
                    Text("Queue accepts piece letters; spaces and commas are ignored.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .padding(.vertical, 4)
            }

            GroupBox("Controls") {
                Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 8) {
                    playControlRow("Left", $controlLeft)
                    playControlRow("Right", $controlRight)
                    playControlRow("Soft", $controlSoftDrop)
                    playControlRow("Hard", $controlHardDrop)
                    playControlRow("CW", $controlRotateCW)
                    playControlRow("CCW", $controlRotateCCW)
                    playControlRow("180", $controlRotate180)
                    playControlRow("Hold", $controlHold)
                    playControlRow("Undo", $controlUndo)
                    playControlRow("Reset", $controlReset)
                }
                .padding(.vertical, 4)
            }

            GroupBox("Tuning") {
                Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 8) {
                    playTuningRow("DAS", value: $tuningAutoShiftMs, range: 0...300, suffix: "ms")
                    playTuningRow("ARR", value: $tuningRepeatMs, range: 0...120, suffix: "ms")
                    playTuningRow("Soft", value: $tuningSoftDropMs, range: 0...250, suffix: "ms")
                    playTuningRow("Gravity", value: $tuningGravityMs, range: 50...1500, suffix: "ms")
                    playTuningRow("Lock", value: $tuningLockDelayMs, range: 0...1000, suffix: "ms")
                    playTuningRow("Preview", value: $previewCellSize, range: 8...20, suffix: "px")
                    GridRow {
                        Text("Rules")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        VStack(alignment: .leading) {
                            Toggle("Gravity", isOn: $gravityEnabled)
                            Toggle("Infinite lock delay", isOn: $infiniteLockDelay)
                            Toggle("Infinite hold", isOn: $infiniteHold)
                            Toggle("Export active piece", isOn: $exportIncludeActive)
                        }
                        .font(.caption)
                    }
                }
                .padding(.vertical, 4)
            }
        }
    }

    private func playControlRow(_ label: String, _ value: Binding<String>) -> some View {
        GridRow {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
            TextField(label, text: value)
                .textFieldStyle(.roundedBorder)
        }
    }

    private func playTuningRow(_ label: String, value: Binding<Double>, range: ClosedRange<Double>, suffix: String) -> some View {
        GridRow {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
            HStack {
                Slider(value: value, in: range)
                Text("\(Int(value.wrappedValue.rounded())) \(suffix)")
                    .font(.caption.monospacedDigit())
                    .frame(width: 58, alignment: .trailing)
            }
        }
    }
}

struct SettingsView: View {
    @ObservedObject var model: AppModel
    @State private var advancedExpanded = false

    let kickOptions = ["srs", "nokicks", "nullpomino180", "jstris180"]
    let formatOptions = ["html", "csv", "link", ""]
    let spinFormatOptions = ["html", "csv"]
    let coverModeOptions = ["normal", "tetris", "tetris-end", "b2b", "any-tspin", "tss", "tsd", "tst", "1l", "2l", "3l", "4l", "1l-or-pc", "2l-or-pc", "3l-or-pc", "4l-or-pc"]
    let coverSortOptions = ["input", "success-desc", "success-asc", "success"]
    let spinFilterOptions = ["none", "strict", "ignore-t"]

    var body: some View {
        GroupBox("Search Settings") {
            VStack(alignment: .leading, spacing: 8) {
                Grid(alignment: .leading, horizontalSpacing: 10, verticalSpacing: 8) {
                    GridRow {
                        PickerField("Command", selection: $model.command) {
                            ForEach(FinderCommand.allCases) { command in
                                Text(command.rawValue).tag(command)
                            }
                        }
                        .onChange(of: model.command) { _ in model.updateForCommand() }

                        if model.command.supportsHoldDropKicks {
                            PickerField("Hold", selection: $model.hold) {
                                ForEach(HoldMode.allCases) { hold in
                                    Text(hold.rawValue).tag(hold)
                                }
                            }

                            PickerField("Drop", selection: $model.drop) {
                                ForEach(DropMode.allCases) { drop in
                                    Text(drop.rawValue).tag(drop)
                                }
                            }
                        } else {
                            EmptyCell()
                            EmptyCell()
                        }
                    }

                    GridRow {
                        if model.command == .setup {
                            ReadOnlyField(title: "Auto height", value: model.lines)
                        } else if model.command == .spin {
                            TextInputField("T lines", text: $model.lines)
                        } else if model.command.supportsLines {
                            TextInputField("Lines", text: $model.lines)
                        } else {
                            EmptyCell()
                        }

                        if model.command == .spin {
                            ReadOnlyField(title: "Board height", value: model.spinAutoHeight)
                        } else {
                            EmptyCell()
                        }
                        EmptyCell()
                    }
                }

                if model.command.supportsCoverSettings {
                    Divider()
                    Grid(alignment: .leading, horizontalSpacing: 10, verticalSpacing: 8) {
                        GridRow {
                            ComboField(title: "Mode", text: $model.coverMode, options: coverModeOptions)
                            Toggle("Mirror", isOn: $model.coverMirror).toggleStyle(.checkbox)
                            EmptyCell()
                        }
                    }
                }

                if model.command.supportsSpinSettings {
                    Divider()
                    Grid(alignment: .leading, horizontalSpacing: 10, verticalSpacing: 8) {
                        GridRow {
                            ComboField(title: "Filter", text: $model.spinFilter, options: spinFilterOptions)
                            EmptyCell()
                            EmptyCell()
                        }
                    }
                }

                DisclosureGroup("Advanced", isExpanded: $advancedExpanded) {
                    VStack(alignment: .leading, spacing: 8) {
                        Grid(alignment: .leading, horizontalSpacing: 10, verticalSpacing: 8) {
                            GridRow {
                                if model.command.supportsFormat {
                                    ComboField(title: "Format", text: $model.format, options: model.command == .spin ? spinFormatOptions : formatOptions)
                                } else {
                                    EmptyCell()
                                }

                                if model.command.supportsOutputBase {
                                    TextInputField("Output base", text: $model.outputBase)
                                } else {
                                    EmptyCell()
                                }

                                EmptyCell()
                            }

                            GridRow {
                                if model.command.supportsHoldDropKicks {
                                    ComboField(title: "Kicks", text: $model.kicks, options: kickOptions)
                                } else {
                                    EmptyCell()
                                }

                                if model.command.supportsThreads {
                                    TextInputField("Threads", text: $model.threads, placeholder: "auto")
                                } else {
                                    EmptyCell()
                                }

                                TextInputField("Extra CLI args", text: $model.extraArgs, placeholder: model.command.extraArgsPlaceholder)
                            }

                            if model.command.supportsCoverSettings {
                                GridRow {
                                    ComboField(title: "Sort", text: $model.coverSort, options: coverSortOptions)
                                    Toggle("Accum", isOn: $model.coverAccum).toggleStyle(.checkbox)
                                    Toggle("Priority", isOn: $model.coverPriority).toggleStyle(.checkbox)
                                }
                            }

                            if model.command.supportsSpinSettings {
                                GridRow {
                                    Toggle("Roof", isOn: $model.spinRoof).toggleStyle(.checkbox)
                                    Toggle("Split", isOn: $model.spinSplit).toggleStyle(.checkbox)
                                    EmptyCell()
                                }
                            }
                        }
                    }
                    .padding(.top, 6)
                }
            }
            .padding(.top, 4)
        }
    }
}

struct RightPane: View {
    @ObservedObject var model: AppModel

    var body: some View {
        VStack(spacing: 12) {
            HStack {
                Text("Command Output")
                    .font(.headline)
                Spacer()
                Toggle("Verbose", isOn: $model.verboseOutput)
                    .toggleStyle(.checkbox)
                Button {
                    model.clearOutput()
                } label: {
                    Label("Clear", systemImage: "xmark.circle")
                }
            }

            ScrollViewReader { proxy in
                ScrollView {
                    Text(model.log.isEmpty ? "Run a search to see output here." : model.log)
                        .font(.system(size: 13.5, weight: .regular, design: .monospaced))
                        .foregroundStyle(model.log.isEmpty ? Color(nsColor: .secondaryLabelColor) : Color(white: 0.9))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .textSelection(.enabled)
                        .padding(12)
                        .id("log-bottom")
                }
                .background(Color(red: 0.12, green: 0.12, blue: 0.12))
                .clipShape(RoundedRectangle(cornerRadius: 8))
                .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(nsColor: .separatorColor)))
                .onChange(of: model.log) { _ in
                    proxy.scrollTo("log-bottom", anchor: .bottom)
                }
            }

            OutputFilesView(model: model)
                .frame(height: 185)
        }
        .padding(14)
    }
}

struct OutputFilesView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GroupBox {
            VStack(spacing: 8) {
                HStack {
                    Text("Generated Files")
                        .font(.headline)
                    Spacer()
                    Button {
                        model.refreshFiles()
                    } label: {
                        Label("Refresh", systemImage: "arrow.clockwise")
                    }
                }

                List(model.outputFiles, selection: Binding(
                    get: { model.selectedOutputFile?.id },
                    set: { id in model.selectedOutputFile = model.outputFiles.first { $0.id == id } }
                )) { file in
                    ViewThatFits(in: .horizontal) {
                        HStack {
                            outputFileLabel(file)
                            Spacer()
                            Button("Open") { model.open(file) }
                        }
                        VStack(alignment: .leading, spacing: 6) {
                            outputFileLabel(file)
                            HStack {
                                Spacer()
                                Button("Open") { model.open(file) }
                            }
                        }
                    }
                    .padding(.vertical, 2)
                }
            }
        }
    }

    private func outputFileLabel(_ file: OutputFile) -> some View {
        VStack(alignment: .leading) {
            Text(file.name).lineLimit(1)
            Text(file.sizeLabel)
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }
}

struct FooterView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        HStack {
            Text(model.status)
                .foregroundStyle(.secondary)
                .lineLimit(1)
            Spacer()
            Button {
                AppDelegate.restartApp()
            } label: {
                Label("Restart", systemImage: "arrow.triangle.2.circlepath")
            }
            Button {
                model.openOutputFolder()
            } label: {
                Label("Open Output Folder", systemImage: "folder")
            }
        }
        .padding(.horizontal, 18)
        .padding(.vertical, 10)
    }
}

struct PickerField<Selection: Hashable, Content: View>: View {
    let title: String
    @Binding var selection: Selection
    @ViewBuilder var content: () -> Content

    init(_ title: String, selection: Binding<Selection>, @ViewBuilder content: @escaping () -> Content) {
        self.title = title
        self._selection = selection
        self.content = content
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            Picker(title, selection: $selection, content: content)
                .labelsHidden()
                .frame(maxWidth: .infinity)
        }
    }
}

struct EmptyCell: View {
    var body: some View {
        Color.clear
            .frame(height: 1)
            .frame(maxWidth: .infinity)
    }
}

struct ComboField: View {
    let title: String
    @Binding var text: String
    let options: [String]

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            Picker(title, selection: $text) {
                ForEach(options, id: \.self) { option in
                    Text(option.isEmpty ? "none" : option).tag(option)
                }
            }
            .labelsHidden()
        }
    }
}

struct TextInputField: View {
    let title: String
    @Binding var text: String
    var placeholder = ""

    init(_ title: String, text: Binding<String>, placeholder: String = "") {
        self.title = title
        self._text = text
        self.placeholder = placeholder
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            TextField(placeholder, text: $text)
                .textFieldStyle(.roundedBorder)
        }
    }
}

struct ReadOnlyField: View {
    let title: String
    let value: String

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            Text(value)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 8)
                .padding(.vertical, 5)
                .background(Color(nsColor: .textBackgroundColor).opacity(0.45))
                .clipShape(RoundedRectangle(cornerRadius: 5))
                .overlay(RoundedRectangle(cornerRadius: 5).stroke(Color(nsColor: .separatorColor)))
        }
    }
}

func shellLine(_ args: [String]) -> String {
    args.map { arg in
        if arg.rangeOfCharacter(from: CharacterSet.whitespacesAndNewlines.union(CharacterSet(charactersIn: "'\""))) == nil {
            return arg
        }
        return "'" + arg.replacingOccurrences(of: "'", with: "'\\''") + "'"
    }
    .joined(separator: " ")
}

func splitArguments(_ text: String) -> [String] {
    var result: [String] = []
    var current = ""
    var quote: Character?
    var escaping = false

    for char in text {
        if escaping {
            current.append(char)
            escaping = false
        } else if char == "\\" {
            escaping = true
        } else if let activeQuote = quote {
            if char == activeQuote {
                quote = nil
            } else {
                current.append(char)
            }
        } else if char == "'" || char == "\"" {
            quote = char
        } else if char.isWhitespace {
            if !current.isEmpty {
                result.append(current)
                current = ""
            }
        } else {
            current.append(char)
        }
    }

    if !current.isEmpty {
        result.append(current)
    }
    return result
}

func javascriptStringLiteral(_ value: String) -> String {
    if let data = try? JSONSerialization.data(withJSONObject: [value], options: []),
       let json = String(data: data, encoding: .utf8),
       json.count >= 2 {
        return String(json.dropFirst().dropLast())
    }
    return "''"
}

func fumenCode(from url: URL) -> String? {
    let candidates = [
        url.absoluteString,
        url.absoluteString.removingPercentEncoding ?? "",
        url.query ?? "",
        url.query?.removingPercentEncoding ?? ""
    ]

    for candidate in candidates {
        guard let start = candidate.range(of: "v115@") else { continue }
        var code = String(candidate[start.lowerBound...])
        if let end = code.rangeOfCharacter(from: CharacterSet.whitespacesAndNewlines.union(CharacterSet(charactersIn: "\"'<>#&"))) {
            code = String(code[..<end.lowerBound])
        }
        code = code.removingPercentEncoding ?? code
        code = code.trimmingCharacters(in: CharacterSet(charactersIn: ".,);]"))
        if code.hasPrefix("v115@"), code.count > 6 {
            return code
        }
    }

    return nil
}
