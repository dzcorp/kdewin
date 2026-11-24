#pragma once

#include "lumencolor.h"

#include <KConfigWatcher>
#include <QCommonStyle>
#include <QStyleOption>
#include <QWidget>

namespace Lumen
{
class Animations;
class ShadowHelper;
class WindowDrag;
class WindowMaterial;

class Style : public QCommonStyle
{
    Q_OBJECT
    // KStyleExtensions: KCapacityBar asks for CE_CapacityBar through styleHint(SH_KCustomElement) with the name as objectName.
    Q_CLASSINFO("X-KDE-CustomElements", "true")
public:
    static constexpr auto SH_KCustomElement = StyleHint(0xff000001);
    static constexpr auto CE_CapacityBar = ControlElement(CE_CustomBase + 1);
    /// Tab bars with this property draw Windows title-row tabs (client-drawn title rows, e.g. Lumen Dolphin).
    static constexpr const char *TitleTabs = "_lumen_titletabs";
    static constexpr int TitleTabFlare = 6;
    static bool isTitleTabs(const QWidget *w)
    {
        return w && w->property(TitleTabs).toBool();
    }
    /// Top-level group boxes in settings apps are drawn as Windows Settings cards.
    static bool isSettingsCard(const QWidget *w);
    static constexpr int CardPad = 16;

    Style();
    ~Style() override;

    using QCommonStyle::polish;
    using QCommonStyle::unpolish;
    void polish(QWidget *w) override;
    void unpolish(QWidget *w) override;
    bool eventFilter(QObject *o, QEvent *e) override;

    int pixelMetric(PixelMetric m, const QStyleOption *opt = nullptr, const QWidget *w = nullptr) const override;
    int styleHint(StyleHint h, const QStyleOption *opt = nullptr, const QWidget *w = nullptr, QStyleHintReturn *ret = nullptr) const override;
    QSize sizeFromContents(ContentsType ct, const QStyleOption *opt, const QSize &size, const QWidget *w = nullptr) const override;
    QRect subElementRect(SubElement se, const QStyleOption *opt, const QWidget *w = nullptr) const override;
    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *w = nullptr) const override;

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p, const QWidget *w = nullptr) const override;
    void drawControl(ControlElement ce, const QStyleOption *opt, QPainter *p, const QWidget *w = nullptr) const override;
    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt, QPainter *p, const QWidget *w = nullptr) const override;

private:
    /// Animated hover/press progress; widget or QML styleObject is the repaint target.
    qreal anim(const QStyleOption *opt, const QWidget *w, quintptr key, bool on, int ms = int(T::Motion::FasterMs)) const;

    void drawButtonPanel(const QStyleOption *opt, QPainter *p, const QWidget *w, bool accent, bool flat) const;
    void drawInputPanel(const QStyleOption *opt, QPainter *p, const QWidget *w, bool focus) const;
    void drawCheck(const QStyleOption *opt, QPainter *p, const QWidget *w, bool radio) const;
    void drawMenuItem(const QStyleOption *opt, QPainter *p, const QWidget *w) const;
    void drawTab(const QStyleOption *opt, QPainter *p, const QWidget *w) const;
    void drawScrollBar(const QStyleOptionComplex *opt, QPainter *p, const QWidget *w) const;
    void drawSlider(const QStyleOptionComplex *opt, QPainter *p, const QWidget *w) const;
    void drawProgress(const QStyleOption *opt, QPainter *p, const QWidget *w) const;
    void drawCapacity(const QStyleOption *opt, QPainter *p, const QWidget *w) const;
    void drawItemViewItem(const QStyleOption *opt, QPainter *p, const QWidget *w) const;
    /// Single-column lists and header-less trees show the Fluent selection pill; detail views don't.
    static bool isNavigationList(const QStyleOptionViewItem *vi, const QWidget *w);
    bool hidesButtonIcon(const QStyleOptionButton *b) const;
    static bool isPageTabs(const QStyleOptionTab *tab, const QWidget *w);
    static QFont groupTitleFont(const QWidget *w);

    Animations *m_anim;
    ShadowHelper *m_shadows;
    WindowDrag *m_drag;
    WindowMaterial *m_material;
    KConfigWatcher::Ptr m_watcher;
};
}
