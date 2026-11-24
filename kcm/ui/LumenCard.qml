import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Spans the page like Windows Settings cards (up to its 1000 px content width), never edge to edge.
FormCard.FormCard {
    maximumWidth: Math.min(1000, width - 2 * Kirigami.Units.gridUnit)
}
