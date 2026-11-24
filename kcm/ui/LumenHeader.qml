import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Lines up with LumenCard.
FormCard.FormHeader {
    maximumWidth: Math.min(1000, width - 2 * Kirigami.Units.gridUnit)
}
