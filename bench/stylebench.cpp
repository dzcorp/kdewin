// Paint cost of a representative widget set per style: stylebench <style> [iterations]
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QProxyStyle>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

// Accumulates time per (kind, element) to find the expensive draws.
class Timing : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;
    mutable std::map<std::pair<char, int>, qint64> ns;

    void drawPrimitive(PrimitiveElement e, const QStyleOption *o, QPainter *p, const QWidget *w) const override
    {
        QElapsedTimer t;
        t.start();
        QProxyStyle::drawPrimitive(e, o, p, w);
        ns[{'P', int(e)}] += t.nsecsElapsed();
    }
    void drawControl(ControlElement e, const QStyleOption *o, QPainter *p, const QWidget *w) const override
    {
        QElapsedTimer t;
        t.start();
        QProxyStyle::drawControl(e, o, p, w);
        ns[{'C', int(e)}] += t.nsecsElapsed();
    }
    void drawComplexControl(ComplexControl e, const QStyleOptionComplex *o, QPainter *p, const QWidget *w) const override
    {
        QElapsedTimer t;
        t.start();
        QProxyStyle::drawComplexControl(e, o, p, w);
        ns[{'X', int(e)}] += t.nsecsElapsed();
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: stylebench <style> [iterations]\n");
        return 2;
    }
    QStyle *style = QStyleFactory::create(QString::fromLatin1(argv[1]));
    if (!style) {
        std::fprintf(stderr, "style %s not found\n", argv[1]);
        return 1;
    }
    const bool breakdown = argc > 3;
    Timing *timing = breakdown ? new Timing(style) : nullptr;
    QApplication::setStyle(timing ? static_cast<QStyle *>(timing) : style);
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 300;

    QWidget root;
    auto *layout = new QVBoxLayout(&root);
    auto *tabs = new QTabWidget;
    auto *page = new QWidget;
    auto *pl = new QVBoxLayout(page);
    for (int i = 0; i < 4; ++i) {
        pl->addWidget(new QPushButton(QStringLiteral("Button %1").arg(i)));
    }
    auto *def = new QPushButton(QStringLiteral("Default"));
    def->setDefault(true);
    pl->addWidget(def);
    pl->addWidget(new QCheckBox(QStringLiteral("Check")));
    pl->addWidget(new QRadioButton(QStringLiteral("Radio")));
    auto *combo = new QComboBox;
    combo->addItems({QStringLiteral("One"), QStringLiteral("Two")});
    pl->addWidget(combo);
    pl->addWidget(new QLineEdit(QStringLiteral("Text")));
    pl->addWidget(new QSpinBox);
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setValue(40);
    pl->addWidget(slider);
    auto *progress = new QProgressBar;
    progress->setValue(60);
    pl->addWidget(progress);
    auto *list = new QListWidget;
    for (int i = 0; i < 40; ++i) {
        list->addItem(QStringLiteral("Item %1").arg(i));
    }
    list->setCurrentRow(3);
    pl->addWidget(list);
    tabs->addTab(page, QStringLiteral("Controls"));
    auto *tree = new QTreeWidget;
    for (int i = 0; i < 30; ++i) {
        auto *it = new QTreeWidgetItem(tree, {QStringLiteral("Node %1").arg(i)});
        new QTreeWidgetItem(it, {QStringLiteral("Child")});
    }
    tabs->addTab(tree, QStringLiteral("Tree"));
    layout->addWidget(tabs);
    auto *group = new QGroupBox(QStringLiteral("Group"));
    (new QVBoxLayout(group))->addWidget(new QScrollBar(Qt::Horizontal));
    layout->addWidget(group);
    root.resize(480, 720);
    root.ensurePolished();

    QImage target(root.size() * root.devicePixelRatio(), QImage::Format_ARGB32_Premultiplied);
    for (int i = 0; i < 20; ++i) {
        root.render(&target);
    }
    std::vector<qint64> samples;
    samples.reserve(size_t(iterations));
    QElapsedTimer t;
    for (int i = 0; i < iterations; ++i) {
        t.start();
        root.render(&target);
        samples.push_back(t.nsecsElapsed());
    }
    if (timing) {
        std::vector<std::pair<qint64, std::pair<char, int>>> rows;
        for (const auto &[k, v] : timing->ns) {
            rows.push_back({v, k});
        }
        std::sort(rows.rbegin(), rows.rend());
        for (size_t i = 0; i < std::min<size_t>(12, rows.size()); ++i) {
            std::printf("  %c%-4d %8.1f us/frame\n", rows[i].second.first, rows[i].second.second, rows[i].first / 1000.0 / (iterations + 20));
        }
    }
    if (const QByteArray out = qgetenv("STYLEBENCH_PNG"); !out.isEmpty()) {
        target.save(QString::fromLocal8Bit(out));
    }
    std::sort(samples.begin(), samples.end());
    const auto pct = [&](double p) { return samples[size_t(p * double(samples.size() - 1))] / 1000.0; };
    std::printf("%-8s median %7.1f us  p95 %7.1f us  min %7.1f us  (%d frames, 480x720)\n", argv[1], pct(0.5), pct(0.95), pct(0.0), iterations);
    return 0;
}
