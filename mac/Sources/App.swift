import Foundation

enum App {
    static let name = "TwentyTwentyTwenty"
    static let title = "20-20-20"
    static let bundleId = "com.sadiq.twentytwentytwenty"
}

extension String {
    var trimmed: String { trimmingCharacters(in: .whitespaces) }
    var trimmedEnd: String {
        var s = Substring(self)
        while let last = s.last, last.isWhitespace { s.removeLast() }
        return String(s)
    }
}
