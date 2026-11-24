/*
    Lumen materials: KWin 6.8 blur effect extended with a Fluent Mica material.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
    SPDX-FileCopyrightText: 2026 Lumen

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lumenmaterials.h"
#include "lumentokens.h"

#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glplatform.h"
#include "scene/backgroundeffectitem.h"
#include "scene/decorationitem.h"
#include "scene/scene.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "wayland/backgroundeffect_v1.h"
#include "wayland/display.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"

#if KWIN_BUILD_X11
#include "utils/xcbutils.h"
#include "x11window.h"
#endif

#include <KConfigGroup>
#include <KDecoration3/Decoration>
#include <KSharedConfig>

#include <QGuiApplication>
#include <QMatrix4x4>
#include <QScreen>
#include <QScopeGuard>
#include <QTime>
#include <QWindow>

#include <array>
#include <cmath>
#include <cstdlib>

Q_LOGGING_CATEGORY(LUMEN_MATERIALS, "kwin_effect_lumen_materials", QtWarningMsg)

static void ensureResources()
{
    Q_INIT_RESOURCE(lumen_materials);
}

namespace KWin
{

static const QByteArray s_blurAtomName = QByteArrayLiteral("_KDE_NET_WM_BLUR_BEHIND_REGION");

static auto disableScissor()
{
    const bool enabled = glIsEnabled(GL_SCISSOR_TEST);
    std::array<GLint, 4> box{};
    glGetIntegerv(GL_SCISSOR_BOX, box.data());
    glDisable(GL_SCISSOR_TEST);
    return qScopeGuard([enabled, box] {
        glScissor(box[0], box[1], box[2], box[3]);
        if (enabled) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
    });
}

static QMatrix4x4 saturationMatrix(qreal saturation)
{
    QMatrix4x4 m;
    if (!qFuzzyCompare(saturation, 1.0)) {
        const qreal rval = (1.0 - saturation) * 0.2126;
        const qreal gval = (1.0 - saturation) * 0.7152;
        const qreal bval = (1.0 - saturation) * 0.0722;
        m = QMatrix4x4(rval + saturation, rval, rval, 0.0,
                       gval, gval + saturation, gval, 0.0,
                       bval, bval, bval + saturation, 0.0,
                       0.0, 0.0, 0.0, 1.0);
    }
    return m;
}

static MaterialRecipe recipe(const Lumen::T::Material &m)
{
    const float r = ((m.tint >> 16) & 0xff) / 255.f, g = ((m.tint >> 8) & 0xff) / 255.f, b = (m.tint & 0xff) / 255.f;
    const float a = float(m.tintOpacity);
    return {QVector4D(r * a, g * a, b * a, a), QVector2D(0.3f * r + 0.59f * g + 0.11f * b, float(m.luminosityOpacity))};
}

using ToneClock = std::chrono::steady_clock;
constexpr auto ToneFade = std::chrono::milliseconds(600);
constexpr auto ToneSettle = std::chrono::milliseconds(300);

static float luma(const QVector3D &c)
{
    return 0.3f * c.x() + 0.59f * c.y() + 0.11f * c.z();
}

/// W3C SetLum with ClipColor, as in onscreen.frag.
static QVector3D setLum(QVector3D c, float l)
{
    c += QVector3D(1, 1, 1) * (l - luma(c));
    const float lum = luma(c);
    const float lo = std::min({c.x(), c.y(), c.z()});
    const float hi = std::max({c.x(), c.y(), c.z()});
    const QVector3D grey(lum, lum, lum);
    if (lo < 0) {
        c = grey + (c - grey) * (lum / std::max(lum - lo, 1e-5f));
    }
    if (hi > 1) {
        c = grey + (c - grey) * ((1 - lum) / std::max(hi - lum, 1e-5f));
    }
    return c;
}

static bool toneFading(const MicaCache &cache)
{
    return cache.toneValid && ToneClock::now() - cache.toneStart < ToneFade;
}

static QVector3D currentTone(const MicaCache &cache)
{
    const float t = std::clamp(std::chrono::duration<float>(ToneClock::now() - cache.toneStart) / std::chrono::duration<float>(ToneFade), 0.f, 1.f);
    return cache.toneFrom + (cache.toneTo - cache.toneFrom) * (t * t * (3 - 2 * t));
}

static std::unique_ptr<GLShader> loadShader(const char *vert, const char *frag)
{
    return ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                             QStringLiteral(":/effects/lumen_materials/shaders/") + QLatin1String(vert),
                                                             QStringLiteral(":/effects/lumen_materials/shaders/") + QLatin1String(frag));
}

LumenMaterialsEffect::LumenMaterialsEffect()
{
    ensureResources();
    m_toneTimer.setSingleShot(true);
    connect(&m_toneTimer, &QTimer::timeout, this, [this] {
        repaintMaterials();
    });

    m_onscreenPass.shader = loadShader("vertex.vert", "onscreen.frag");
    m_roundedOnscreenPass.shader = loadShader("onscreen_rounded.vert", "onscreen_rounded.frag");
    m_downsamplePass.shader = loadShader("vertex.vert", "downsample.frag");
    m_upsamplePass.shader = loadShader("vertex.vert", "upsample.frag");
    m_noisePass.shader = loadShader("vertex.vert", "noise.frag");
    m_popupPass.shader = loadShader("popup.vert", "popup.frag");
    if (!m_onscreenPass.shader || !m_roundedOnscreenPass.shader || !m_downsamplePass.shader || !m_upsamplePass.shader || !m_noisePass.shader || !m_popupPass.shader) {
        qCWarning(LUMEN_MATERIALS) << "Failed to load shaders";
        return;
    }

    auto *s = m_onscreenPass.shader.get();
    m_onscreenPass.mvpMatrixLocation = s->uniformLocation("modelViewProjectionMatrix");
    m_onscreenPass.colorMatrixLocation = s->uniformLocation("colorMatrix");
    m_onscreenPass.offsetLocation = s->uniformLocation("offset");
    m_onscreenPass.halfpixelLocation = s->uniformLocation("halfpixel");
    m_onscreenPass.tintLocation = s->uniformLocation("tint");
    m_onscreenPass.luminosityLocation = s->uniformLocation("luminosity");

    s = m_roundedOnscreenPass.shader.get();
    m_roundedOnscreenPass.mvpMatrixLocation = s->uniformLocation("modelViewProjectionMatrix");
    m_roundedOnscreenPass.colorMatrixLocation = s->uniformLocation("colorMatrix");
    m_roundedOnscreenPass.offsetLocation = s->uniformLocation("offset");
    m_roundedOnscreenPass.halfpixelLocation = s->uniformLocation("halfpixel");
    m_roundedOnscreenPass.boxLocation = s->uniformLocation("box");
    m_roundedOnscreenPass.cornerRadiusLocation = s->uniformLocation("cornerRadius");
    m_roundedOnscreenPass.opacityLocation = s->uniformLocation("opacity");
    m_roundedOnscreenPass.tintLocation = s->uniformLocation("tint");
    m_roundedOnscreenPass.luminosityLocation = s->uniformLocation("luminosity");

    s = m_popupPass.shader.get();
    m_popupPass.mvpMatrixLocation = s->uniformLocation("modelViewProjectionMatrix");
    m_popupPass.colorMatrixLocation = s->uniformLocation("colorMatrix");
    m_popupPass.offsetLocation = s->uniformLocation("offset");
    m_popupPass.halfpixelLocation = s->uniformLocation("halfpixel");
    m_popupPass.tintLocation[0] = s->uniformLocation("tintDark");
    m_popupPass.tintLocation[1] = s->uniformLocation("tintLight");
    m_popupPass.luminosityLocation[0] = s->uniformLocation("luminosityDark");
    m_popupPass.luminosityLocation[1] = s->uniformLocation("luminosityLight");
    m_popupPass.opacityLocation = s->uniformLocation("opacity");
    m_popupPass.noiseSizeLocation = s->uniformLocation("noiseSize");
    m_popupPass.keyOpaqueLocation = s->uniformLocation("keyOpaque");
    ShaderManager::instance()->pushShader(s);
    s->setUniform("popupUnit", 1);
    s->setUniform("noiseUnit", 2);
    ShaderManager::instance()->popShader();

    s = m_downsamplePass.shader.get();
    m_downsamplePass.mvpMatrixLocation = s->uniformLocation("modelViewProjectionMatrix");
    m_downsamplePass.offsetLocation = s->uniformLocation("offset");
    m_downsamplePass.halfpixelLocation = s->uniformLocation("halfpixel");

    s = m_upsamplePass.shader.get();
    m_upsamplePass.mvpMatrixLocation = s->uniformLocation("modelViewProjectionMatrix");
    m_upsamplePass.offsetLocation = s->uniformLocation("offset");
    m_upsamplePass.halfpixelLocation = s->uniformLocation("halfpixel");

    s = m_noisePass.shader.get();
    m_noisePass.mvpMatrixLocation = s->uniformLocation("modelViewProjectionMatrix");
    m_noisePass.noiseTextureSizeLocation = s->uniformLocation("noiseTextureSize");

    initBlurStrengthValues();
    readColorScheme();
    reconfigure(ReconfigureAll);

    m_kdeglobals = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
    connect(m_kdeglobals.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String("Colors:Window") || group.name() == QLatin1String("General")) {
            readColorScheme();
            repaintMaterials();
        }
    });

#if KWIN_BUILD_X11
    if (effects->xcbConnection()) {
        net_wm_blur_region = effects->announceSupportProperty(s_blurAtomName, this);
    }
#endif

    waylandServer()->backgroundEffectManager()->addBlurCapability();

    connect(effects, &EffectsHandler::windowAdded, this, &LumenMaterialsEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &LumenMaterialsEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::viewRemoved, this, &LumenMaterialsEffect::slotViewRemoved);
#if KWIN_BUILD_X11
    connect(effects, &EffectsHandler::propertyNotify, this, &LumenMaterialsEffect::slotPropertyNotify);
    connect(effects, &EffectsHandler::xcbConnectionChanged, this, [this]() {
        net_wm_blur_region = effects->announceSupportProperty(s_blurAtomName, this);
    });
#endif

    const auto stackingOrder = effects->stackingOrder();
    for (EffectWindow *window : stackingOrder) {
        slotWindowAdded(window);
    }

    m_valid = true;
}

LumenMaterialsEffect::~LumenMaterialsEffect()
{
    waylandServer()->backgroundEffectManager()->removeBlurCapability();
}

void LumenMaterialsEffect::initBlurStrengthValues()
{
    // Evenly distribute 15 strength steps over the usable (iteration, offset) pairs; outside
    // [minOffset, maxOffset] the dual Kawase passes produce blocky or diagonal artifacts.
    const int numOfBlurSteps = 15;
    int remainingSteps = numOfBlurSteps;

    blurOffsets.append({1.0, 2.0, 10});
    blurOffsets.append({2.0, 3.0, 20});
    blurOffsets.append({2.0, 5.0, 50});
    blurOffsets.append({3.0, 8.0, 150});

    float offsetSum = 0;
    for (const auto &o : std::as_const(blurOffsets)) {
        offsetSum += o.maxOffset - o.minOffset;
    }
    for (int i = 0; i < blurOffsets.size(); i++) {
        int iterationNumber = std::ceil((blurOffsets[i].maxOffset - blurOffsets[i].minOffset) / offsetSum * numOfBlurSteps);
        remainingSteps -= iterationNumber;
        if (remainingSteps < 0) {
            iterationNumber += remainingSteps;
        }
        const float offsetDifference = blurOffsets[i].maxOffset - blurOffsets[i].minOffset;
        for (int j = 1; j <= iterationNumber; j++) {
            blurStrengthValues.append({i + 1, blurOffsets[i].minOffset + (offsetDifference / iterationNumber) * j});
        }
    }
}

void LumenMaterialsEffect::readColorScheme()
{
    const KConfigGroup g(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), QStringLiteral("Colors:Window"));
    const QStringList rgb = g.readEntry("BackgroundNormal", QStringLiteral("32,32,32")).split(QLatin1Char(','));
    if (rgb.size() >= 3) {
        const double luma = 0.2126 * rgb[0].toInt() + 0.7152 * rgb[1].toInt() + 0.0722 * rgb[2].toInt();
        m_dark = luma < 128;
    }
    m_micaRecipe = recipe(m_dark ? Lumen::T::Materials::MicaDark : Lumen::T::Materials::MicaLight);
    m_acrylicRecipe = recipe(m_dark ? Lumen::T::Materials::AcrylicDark : Lumen::T::Materials::AcrylicLight);
}

void LumenMaterialsEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    const KConfigGroup g(effects->config(), QStringLiteral("Effect-lumen_materials"));
    auto strength = [this](int v) {
        return std::clamp(v, 1, int(blurStrengthValues.size())) - 1;
    };

    const int acrylic = strength(g.readEntry("AcrylicStrength", int(Lumen::T::Materials::AcrylicStrength)));
    m_iterationCount = blurStrengthValues[acrylic].iteration;
    m_offset = blurStrengthValues[acrylic].offset;
    m_expandSize = blurOffsets[m_iterationCount - 1].expandSize;
    m_noiseStrength = g.readEntry("AcrylicNoise", int(Lumen::T::Materials::AcrylicNoise));
    m_colorMatrix = saturationMatrix(g.readEntry("AcrylicSaturation", double(Lumen::T::Materials::AcrylicSaturation)) / 100.0);

    const int mica = strength(g.readEntry("MicaStrength", int(Lumen::T::Materials::MicaStrength)));
    m_micaIterations = blurStrengthValues[mica].iteration;
    m_micaOffset = blurStrengthValues[mica].offset;
    m_micaColorMatrix = saturationMatrix(g.readEntry("MicaSaturation", double(Lumen::T::Materials::MicaSaturation)) / 100.0);
    m_micaForPanels = g.readEntry("MicaForPanels", true);
    m_toneOpacity = std::clamp(g.readEntry("WallpaperTone", int(Lumen::T::Materials::MicaWallpaperTone)), 0, 100) / 100.f;

    for (auto &[window, data] : m_windows) {
        data.blurItem->setPixelsToExpandRepaintsBelowOpaqueRegions(m_expandSize);
    }
    for (auto &[view, cache] : m_mica) {
        cache.needsBlur = true;
        cache.wallpaperDamaged = true;
        cache.toneDirty = true;
    }
    repaintMaterials();
}

void LumenMaterialsEffect::repaintMaterials()
{
    for (auto &[window, data] : m_windows) {
        data.blurItem->scheduleRepaint(data.blurItem->boundingRect());
    }
    effects->addRepaintFull();
}

void LumenMaterialsEffect::updateBlurRegion(EffectWindow *w)
{
    std::optional<RegionF> content;
    std::optional<RegionF> frame;

#if KWIN_BUILD_X11
    if (net_wm_blur_region != XCB_ATOM_NONE) {
        if (const auto x11Window = qobject_cast<X11Window *>(w->window())) {
            Xcb::Property wmBlurRegionProperty(false, x11Window->window(), net_wm_blur_region, XCB_ATOM_CARDINAL, 0, 32768);
            if (const auto cardinals = wmBlurRegionProperty.array<uint32_t>()) {
                if (cardinals->size() == 0 || cardinals->size() == 1) {
                    content = RegionF();
                } else if (cardinals->size() % 4 == 0) {
                    RegionF region;
                    for (uint i = 0; i < cardinals->size();) {
                        const int x = (*cardinals)[i++];
                        const int y = (*cardinals)[i++];
                        const int w = (*cardinals)[i++];
                        const int h = (*cardinals)[i++];
                        region += Xcb::fromXNative(Rect(x, y, w, h));
                    }
                    content = region;
                }
            }
        }
    }
#endif

    if (SurfaceInterface *surface = w->surface()) {
        if (!surface->blurRegion().isEmpty()) {
            content = surface->blurRegion();
        }
    }
    if (auto internal = w->internalWindow()) {
        const auto property = internal->property("kwin_blur");
        if (property.isValid()) {
            content = property.value<RegionF>();
        }
    }
    if (w->decorationHasAlpha() && decorationSupportsBlurBehind(w)) {
        frame = decorationBlurRegion(w);
    }
    const bool popup = !content && !frame && isPopup(w);
    if (popup) {
        content = RegionF();
    }

    if (content.has_value() || frame.has_value()) {
        MaterialWindowData &data = m_windows[w];
        data.content = content;
        data.frame = frame;
        data.popup = popup;
        data.keyOpaque = !popup || !isGecko(w);
        if (!popup && !data.capture.empty()) {
            effects->makeOpenGLContextCurrent();
            data.capture.clear();
        }
        if (!data.blurItem) {
            data.blurItem = std::make_unique<BackgroundEffectItem>(w->windowItem());
        }
        data.blurItem->setPixelsToExpandRepaintsBelowOpaqueRegions(m_expandSize);
        data.blurItem->setEffectBoundingRect(blurRegion(w).boundingRect());
    } else if (auto it = m_windows.find(w); it != m_windows.end()) {
        effects->makeOpenGLContextCurrent();
        m_windows.erase(it);
    }
}

void LumenMaterialsEffect::slotWindowAdded(EffectWindow *w)
{
    if (SurfaceInterface *surf = w->surface()) {
        windowBlurChangedConnections[w] = connect(surf, &SurfaceInterface::blurChanged, this, [this, w]() {
            if (w) {
                updateBlurRegion(w);
            }
        });
    }
    if (auto internal = w->internalWindow()) {
        internal->installEventFilter(this);
    }
    if (w->isDesktop()) {
        // Exposed wallpaper repaints with unchanged pixels; only real wallpaper damage needs a new Mica blur.
        connect(w, &EffectWindow::windowDamaged, this, [this]() {
            for (auto &[view, cache] : m_mica) {
                cache.wallpaperDamaged = true;
            }
        });
    }
    setupDecorationConnections(w);
    connect(w, &EffectWindow::windowDecorationChanged, this, [this, w]() {
        setupDecorationConnections(w);
        updateBlurRegion(w);
    });
    updateBlurRegion(w);
}

void LumenMaterialsEffect::slotWindowDeleted(EffectWindow *w)
{
    for (auto &[view, cache] : m_mica) {
        if (cache.desktop == w) {
            cache.desktop = nullptr;
            cache.captured = false;
            cache.wallpaperDamaged = true;
        }
    }
    if (auto it = m_windows.find(w); it != m_windows.end()) {
        effects->makeOpenGLContextCurrent();
        m_windows.erase(it);
    }
    if (auto it = windowBlurChangedConnections.find(w); it != windowBlurChangedConnections.end()) {
        disconnect(*it);
        windowBlurChangedConnections.erase(it);
    }
}

void LumenMaterialsEffect::slotViewRemoved(KWin::RenderView *view)
{
    effects->makeOpenGLContextCurrent();
    for (auto &[window, data] : m_windows) {
        data.render.erase(view);
        data.capture.erase(view);
    }
    m_mica.erase(view);
}

#if KWIN_BUILD_X11
void LumenMaterialsEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == net_wm_blur_region && net_wm_blur_region != XCB_ATOM_NONE) {
        updateBlurRegion(w);
    }
}
#endif

void LumenMaterialsEffect::setupDecorationConnections(EffectWindow *w)
{
    if (!w->decoration()) {
        return;
    }
    connect(w->decoration(), &KDecoration3::Decoration::blurRegionChanged, this, [this, w]() {
        updateBlurRegion(w);
    });
}

bool LumenMaterialsEffect::eventFilter(QObject *watched, QEvent *event)
{
    auto internal = qobject_cast<QWindow *>(watched);
    if (internal && event->type() == QEvent::DynamicPropertyChange) {
        auto *pe = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (pe->propertyName() == "kwin_blur") {
            if (auto w = effects->findWindow(internal)) {
                updateBlurRegion(w);
            }
        }
    }
    return false;
}

bool LumenMaterialsEffect::enabledByDefault()
{
    return false;
}

bool LumenMaterialsEffect::supported()
{
    return effects->isOpenGLCompositing();
}

bool LumenMaterialsEffect::isPopup(const EffectWindow *w)
{
    return !w->internalWindow() && (w->isPopupWindow() || w->isPopupMenu() || w->isDropdownMenu() || w->isMenu() || w->isComboBox() || w->isTooltip());
}

bool LumenMaterialsEffect::isGecko(EffectWindow *w)
{
    while (EffectWindow *parent = w->transientFor()) {
        w = parent;
    }
    const QString app = w->windowClass().section(QLatin1Char(' '), -1).toLower();
    using namespace Qt::StringLiterals;
    static constexpr QLatin1StringView names[]{"firefox"_L1, "mozilla"_L1, "librewolf"_L1, "waterfox"_L1, "floorp"_L1, "thunderbird"_L1, "zen"_L1, "mullvad"_L1, "tor-browser"_L1};
    return std::ranges::any_of(names, [&app](QLatin1StringView n) {
        return app.contains(n);
    });
}

bool LumenMaterialsEffect::decorationSupportsBlurBehind(const EffectWindow *w) const
{
    return w->decoration() && !w->decoration()->blurRegion().isNull();
}

RegionF LumenMaterialsEffect::decorationBlurRegion(const EffectWindow *w) const
{
    if (!decorationSupportsBlurBehind(w)) {
        return RegionF();
    }
    const RegionF decorationRegion = RegionF(w->decoration()->rect()) - w->contentsRect();
    return decorationRegion.intersected(RegionF(w->decoration()->blurRegion()));
}

RegionF LumenMaterialsEffect::blurRegion(EffectWindow *w) const
{
    RegionF region;
    if (auto it = m_windows.find(w); it != m_windows.end()) {
        if (it->second.popup) {
            return w->expandedGeometry().translated(-w->pos());
        }
        const std::optional<RegionF> &content = it->second.content;
        const std::optional<RegionF> &frame = it->second.frame;
        if (content.has_value()) {
            if (content->isEmpty()) {
                region = w->contentsRect();
            } else if (w->decoration()) {
                region = content->translated(w->contentsRect().topLeft()) & w->contentsRect();
            } else {
                region = *content & w->contentsRect();
            }
            if (frame.has_value()) {
                region += frame.value();
            }
        } else if (frame.has_value()) {
            region = frame.value();
        }
    }
    return region;
}

LumenMaterialsEffect::Material LumenMaterialsEffect::materialFor(const EffectWindow *w, const MaterialWindowData &d) const
{
    // Static wallpaper material for long-lived surfaces; live blur for transient ones.
    if ((w->isDock() && m_micaForPanels) || w->isNormalWindow() || w->isDialog() || (d.frame.has_value() && !d.content.has_value())) {
        return Material::Mica;
    }
    return Material::Acrylic;
}

void LumenMaterialsEffect::prePaintScreen(ScreenPrePaintData &data)
{
    m_currentView = data.view;
    effects->prePaintScreen(data);
}

bool LumenMaterialsEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    if (!effects->activeFullScreenEffect() && !(mask & PAINT_SCREEN_TRANSFORMED)) {
        for (EffectWindow *w : effects->stackingOrder()) {
            if (w->isDesktop() && !w->isDeleted() && w->isVisible() && w->isOnCurrentDesktop() && w->isOnCurrentActivity()
                && (w->screen() == screen || (!screen && w->frameGeometry().intersects(viewport.renderRect())))) {
                MicaCache &cache = m_mica[m_currentView];
                cache.output = screen;
                if (cache.desktop != w) {
                    cache.desktop = w;
                    cache.captured = false;
                    cache.wallpaperDamaged = true;
                }
                // Refresh even when opaque windows hide the desktop from the normal draw pass.
                captureWallpaper(w, renderTarget, viewport);
                maybeSampleTone(cache);
                break;
            }
        }
    }
    return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
}

void LumenMaterialsEffect::postPaintScreen()
{
    if (m_toneOpacity > 0 && std::ranges::any_of(m_mica, [](const auto &entry) {
            return toneFading(entry.second);
        })) {
        repaintMaterials();
    }
    effects->postPaintScreen();
}

bool LumenMaterialsEffect::shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }
    if (w->isDesktop()) {
        return false;
    }
    const bool scaled = !qFuzzyCompare(data.xScale(), 1.0) && !qFuzzyCompare(data.yScale(), 1.0);
    const bool translated = data.xTranslation() || data.yTranslation();
    if ((scaled || translated || (mask & PAINT_WINDOW_TRANSFORMED)) && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }
    return true;
}

void LumenMaterialsEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    if (auto it = m_windows.find(w); it != m_windows.end() && it->second.popup) {
        data.setTranslucent(); // its opaque region must not hide the backdrop the acrylic is made from
    }
    effects->prePaintWindow(view, w, data);
}

bool LumenMaterialsEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    if (blur(renderTarget, viewport, w, mask, deviceRegion, data)) {
        return true;
    }
    return effects->drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

bool LumenMaterialsEffect::ensureChain(BlurChain &chain, const QSize &size, GLenum format, size_t iterations, bool *reallocated) const
{
    if (chain.framebuffers.size() == iterations + 1 && chain.textures[0]->size() == size && chain.textures[0]->internalFormat() == format) {
        return true;
    }
    if (reallocated) {
        *reallocated = true;
    }
    chain.framebuffers.clear();
    chain.textures.clear();
    glClearColor(0, 0, 0, 0);
    for (size_t i = 0; i <= iterations; ++i) {
        auto texture = GLTexture::allocate(format, size / (1 << i));
        if (!texture) {
            qCWarning(LUMEN_MATERIALS) << "Failed to allocate an offscreen texture";
            return false;
        }
        texture->setFilter(GL_LINEAR);
        texture->setWrapMode(GL_CLAMP_TO_EDGE);
        auto framebuffer = std::make_unique<GLFramebuffer>(texture.get());
        if (!framebuffer->valid()) {
            qCWarning(LUMEN_MATERIALS) << "Failed to create an offscreen framebuffer";
            return false;
        }
        EglContext::currentContext()->pushFramebuffer(framebuffer.get());
        glClear(GL_COLOR_BUFFER_BIT);
        EglContext::currentContext()->popFramebuffer();
        chain.textures.push_back(std::move(texture));
        chain.framebuffers.push_back(std::move(framebuffer));
    }
    return true;
}

void LumenMaterialsEffect::kawase(BlurChain &chain, const QSize &size, float offset, GLVertexBuffer *vbo)
{
    const auto scissor = disableScissor();
    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(QRectF(0.0, 0.0, size.width(), size.height()));

    ShaderManager::instance()->pushShader(m_downsamplePass.shader.get());
    m_downsamplePass.shader->setUniform(m_downsamplePass.mvpMatrixLocation, projectionMatrix);
    m_downsamplePass.shader->setUniform(m_downsamplePass.offsetLocation, offset);
    for (size_t i = 1; i < chain.framebuffers.size(); ++i) {
        const auto &read = chain.framebuffers[i - 1];
        const QVector2D halfpixel(0.5 / read->colorAttachment()->width(), 0.5 / read->colorAttachment()->height());
        m_downsamplePass.shader->setUniform(m_downsamplePass.halfpixelLocation, halfpixel);
        read->colorAttachment()->bind();
        GLFramebuffer::pushFramebuffer(chain.framebuffers[i].get());
        vbo->draw(GL_TRIANGLES, 0, 6);
    }
    ShaderManager::instance()->popShader();

    ShaderManager::instance()->pushShader(m_upsamplePass.shader.get());
    m_upsamplePass.shader->setUniform(m_upsamplePass.mvpMatrixLocation, projectionMatrix);
    m_upsamplePass.shader->setUniform(m_upsamplePass.offsetLocation, offset);
    for (size_t i = chain.framebuffers.size() - 1; i > 1; --i) {
        GLFramebuffer::popFramebuffer();
        const auto &read = chain.framebuffers[i];
        const QVector2D halfpixel(0.5 / read->colorAttachment()->width(), 0.5 / read->colorAttachment()->height());
        m_upsamplePass.shader->setUniform(m_upsamplePass.halfpixelLocation, halfpixel);
        read->colorAttachment()->bind();
        vbo->draw(GL_TRIANGLES, 0, 6);
    }
    ShaderManager::instance()->popShader();
}

void LumenMaterialsEffect::captureWallpaper(EffectWindow *desktop, const RenderTarget &renderTarget, const RenderViewport &viewport)
{
    MicaCache &cache = m_mica[m_currentView];
    const Rect viewRect = viewport.renderRect().rounded();
    GLenum format = GL_RGBA8;
    if (renderTarget.texture()) {
        format = renderTarget.texture()->internalFormat();
    }
    bool reallocated = cache.rect != viewRect;
    if (!ensureChain(cache.chain, viewRect.size(), format, m_micaIterations, &reallocated)) {
        return;
    }
    if (reallocated) {
        cache.rect = viewRect;
        cache.captured = false;
        cache.wallpaperDamaged = true;
    }
    if (!cache.wallpaperDamaged) {
        return;
    }
    const auto started = ToneClock::now();
    // Capture the full desktop independently of on-screen occlusion.
    GLFramebuffer *fbo = cache.chain.framebuffers[0].get();
    const RenderTarget target(fbo, renderTarget.colorDescription());
    const RenderViewport offscreen(viewport.renderRect(), 1.0, target, QPoint());
    const auto scissor = disableScissor();
    GLFramebuffer::pushFramebuffer(fbo);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    WindowPaintData paintData;
    const bool drawn = effects->drawWindow(target, offscreen, desktop, PAINT_WINDOW_OPAQUE, Region::infinite(), paintData);
    // Mica is opaque even where the desktop surface is not.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    GLFramebuffer::popFramebuffer();
    if (!drawn) {
        return;
    }
    cache.wallpaperDamaged = false;
    cache.captured = true;
    cache.needsBlur = true;
    cache.toneDirty = true;
    cache.lastCapture = ToneClock::now();
    ++cache.captures;
    cache.captureTime += cache.lastCapture - started;
}

void LumenMaterialsEffect::drawOnscreen(const RenderViewport &viewport, EffectWindow *w, const WindowPaintData &data, GLTexture *source, const Rect &scaledBackgroundRect,
                                        const QMatrix4x4 &colorMatrix, float offset, const MaterialRecipe &recipe, float modulation, int vertexCount)
{
    QMatrix4x4 projectionMatrix = viewport.projectionMatrix();
    projectionMatrix.translate(scaledBackgroundRect.x(), scaledBackgroundRect.y());
    const QVector2D halfpixel(0.5 / source->width(), 0.5 / source->height());

    if (const BorderRadius cornerRadius = w->window()->borderRadius(); !cornerRadius.isNull()) {
        auto *s = m_roundedOnscreenPass.shader.get();
        ShaderManager::instance()->pushShader(s);
        const RectF transformedRect = RectF{
            w->frameGeometry().x() + data.xTranslation(),
            w->frameGeometry().y() + data.yTranslation(),
            w->frameGeometry().width() * data.xScale(),
            w->frameGeometry().height() * data.yScale(),
        };
        const RectF nativeBox = transformedRect.scaled(viewport.scale()).rounded().translated(-scaledBackgroundRect.topLeft());
        const BorderRadius nativeCornerRadius = cornerRadius.scaled(viewport.scale()).rounded();
        s->setUniform(m_roundedOnscreenPass.mvpMatrixLocation, projectionMatrix);
        s->setUniform(m_roundedOnscreenPass.colorMatrixLocation, colorMatrix);
        s->setUniform(m_roundedOnscreenPass.halfpixelLocation, halfpixel);
        s->setUniform(m_roundedOnscreenPass.offsetLocation, offset);
        s->setUniform(m_roundedOnscreenPass.boxLocation, QVector4D(nativeBox.horizontalCenter(), nativeBox.verticalCenter(), nativeBox.width() * 0.5, nativeBox.height() * 0.5));
        s->setUniform(m_roundedOnscreenPass.cornerRadiusLocation, nativeCornerRadius.toVector());
        s->setUniform(m_roundedOnscreenPass.opacityLocation, modulation);
        s->setUniform(m_roundedOnscreenPass.tintLocation, recipe.tint);
        s->setUniform(m_roundedOnscreenPass.luminosityLocation, recipe.luminosity);
        source->bind();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        GLVertexBuffer::streamingBuffer()->draw(GL_TRIANGLES, 6, vertexCount);
        glDisable(GL_BLEND);
        ShaderManager::instance()->popShader();
        return;
    }

    auto *s = m_onscreenPass.shader.get();
    ShaderManager::instance()->pushShader(s);
    s->setUniform(m_onscreenPass.mvpMatrixLocation, projectionMatrix);
    s->setUniform(m_onscreenPass.colorMatrixLocation, colorMatrix);
    s->setUniform(m_onscreenPass.halfpixelLocation, halfpixel);
    s->setUniform(m_onscreenPass.offsetLocation, offset);
    s->setUniform(m_onscreenPass.tintLocation, recipe.tint);
    s->setUniform(m_onscreenPass.luminosityLocation, recipe.luminosity);
    source->bind();
    if (modulation < 1.0) {
        glEnable(GL_BLEND);
        glBlendColor(0, 0, 0, modulation);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    }
    GLVertexBuffer::streamingBuffer()->draw(GL_TRIANGLES, 6, vertexCount);
    if (modulation < 1.0) {
        glDisable(GL_BLEND);
    }
    ShaderManager::instance()->popShader();
}

GLTexture *LumenMaterialsEffect::ensureNoiseTexture()
{
    if (m_noiseStrength == 0) {
        return nullptr;
    }
    const qreal scale = std::max(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);
    if (!m_noisePass.noiseTexture || m_noisePass.noiseTextureScale != scale || m_noisePass.noiseTextureStength != m_noiseStrength) {
        std::srand(uint(QTime::currentTime().msec()));
        QImage noiseImage(QSize(256, 256), QImage::Format_Grayscale8);
        for (int y = 0; y < noiseImage.height(); y++) {
            auto *line = noiseImage.scanLine(y);
            for (int x = 0; x < noiseImage.width(); x++) {
                line[x] = std::rand() % (m_noiseStrength + 1);
            }
        }
        noiseImage = noiseImage.scaled(noiseImage.size() * scale);
        m_noisePass.noiseTexture = GLTexture::upload(noiseImage);
        if (!m_noisePass.noiseTexture) {
            return nullptr;
        }
        m_noisePass.noiseTexture->setFilter(GL_NEAREST);
        m_noisePass.noiseTexture->setWrapMode(GL_REPEAT);
        m_noisePass.noiseTextureScale = scale;
        m_noisePass.noiseTextureStength = m_noiseStrength;
    }
    return m_noisePass.noiseTexture.get();
}

/// Six vertices: two triangles covering `r`, texcoords from `uv` (flipped V like framebuffer blits).
static void pushQuad(std::span<GLVertex2D> map, size_t &i, const RectF &r, const RectF &uv)
{
    const float x0 = r.left(), y0 = r.top(), x1 = r.right(), y1 = r.bottom();
    const float u0 = uv.left(), v0 = 1.0f - uv.top(), u1 = uv.right(), v1 = 1.0f - uv.bottom();
    map[i++] = GLVertex2D{.position = QVector2D(x0, y0), .texcoord = QVector2D(u0, v0)};
    map[i++] = GLVertex2D{.position = QVector2D(x1, y1), .texcoord = QVector2D(u1, v1)};
    map[i++] = GLVertex2D{.position = QVector2D(x0, y1), .texcoord = QVector2D(u0, v1)};
    map[i++] = GLVertex2D{.position = QVector2D(x0, y0), .texcoord = QVector2D(u0, v0)};
    map[i++] = GLVertex2D{.position = QVector2D(x1, y0), .texcoord = QVector2D(u1, v0)};
    map[i++] = GLVertex2D{.position = QVector2D(x1, y1), .texcoord = QVector2D(u1, v1)};
}

void LumenMaterialsEffect::maybeSampleTone(MicaCache &cache)
{
    if (m_toneOpacity <= 0 || !cache.captured || !cache.toneDirty || cache.wallpaperDamaged) {
        return;
    }
    // Wallpaper slideshows cross-fade: wait until the desktop stops changing.
    const auto settled = cache.lastCapture + ToneSettle;
    if (const auto now = ToneClock::now(); now < settled) {
        const auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(settled - now) + std::chrono::milliseconds(1);
        if (!m_toneTimer.isActive() || m_toneTimer.remainingTimeAsDuration() > delay) {
            m_toneTimer.start(delay);
        }
        return;
    }
    sampleTone(cache);
}

void LumenMaterialsEffect::sampleTone(MicaCache &cache)
{
    const auto started = ToneClock::now();
    if (cache.chain.textures.size() < 2) {
        return;
    }
    if (cache.needsBlur) {
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->reset();
        vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));
        auto result = vbo->map<GLVertex2D>(6);
        if (!result) {
            return;
        }
        size_t i = 0;
        const QSize size = cache.rect.size();
        pushQuad(*result, i, RectF(0, 0, size.width(), size.height()), RectF(0, 0, 1, 1));
        vbo->unmap();
        vbo->bindArrays();
        kawase(cache.chain, size, m_micaOffset, vbo);
        GLFramebuffer::popFramebuffer();
        vbo->unbindArrays();
        cache.needsBlur = false;
    }

    // The deepest level keeps the last downsample: a tiny, already averaged wallpaper.
    const GLTexture *smallest = cache.chain.textures.back().get();
    const int w = smallest->width(), h = smallest->height();
    std::vector<float> rgba(size_t(w) * h * 4);
    GLint readFramebuffer, packBuffer, packAlignment, packRowLength, packSkipRows, packSkipPixels;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &packRowLength);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &packSkipRows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &packSkipPixels);
    // Read into tightly packed CPU memory, preserving the compositor's GL state.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, cache.chain.framebuffers.back()->handle());
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    GLint componentType = GL_NONE;
    // GLES readback must match the actual attachment, even if its requested texture format differs.
    glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &componentType);
    if (componentType == GL_FLOAT) {
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, rgba.data());
    } else {
        std::vector<GLubyte> bytes(rgba.size());
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
        std::ranges::transform(bytes, rgba.begin(), [](GLubyte b) {
            return b / 255.f;
        });
    }
    cache.toneReadError = glGetError();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, packRowLength);
    glPixelStorei(GL_PACK_SKIP_ROWS, packSkipRows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, packSkipPixels);

    QVector3D sum;
    float weight = 0;
    for (size_t i = 0; i < rgba.size(); i += 4) {
        sum += QVector3D(rgba[i], rgba[i + 1], rgba[i + 2]); // premultiplied, so alpha already weights it
        weight += rgba[i + 3];
    }
    if (weight <= 0 || cache.toneReadError != GL_NO_ERROR) {
        return;
    }
    const QVector3D tone = sum / weight;
    cache.toneFrom = cache.toneValid ? currentTone(cache) : tone;
    cache.toneTo = tone;
    cache.toneStart = ToneClock::now();
    if (cache.toneFrom == cache.toneTo) {
        cache.toneStart -= ToneFade;
    }
    cache.toneValid = true;
    cache.toneDirty = false;
    ++cache.samples;
    cache.sampleTime += ToneClock::now() - started;
    repaintMaterials();
}

MicaCache *LumenMaterialsEffect::wallpaperFor(const EffectWindow *w)
{
    MicaCache *current = nullptr;
    if (auto it = m_mica.find(m_currentView); it != m_mica.end() && it->second.captured) {
        current = &it->second;
        if (current->toneValid) {
            return current;
        }
    }
    // Screenshots use temporary render views without a wallpaper cache of their own.
    for (auto &[view, cache] : m_mica) {
        if (cache.captured && cache.toneValid && cache.output == w->screen()) {
            return &cache;
        }
    }
    MicaCache *best = nullptr;
    qreal largest = 0;
    for (auto &[view, cache] : m_mica) {
        if (cache.captured && cache.toneValid) {
            const Rect overlap = cache.rect.intersected(w->frameGeometry().rounded());
            const qreal area = qreal(overlap.width()) * overlap.height();
            if (area > largest) {
                largest = area;
                best = &cache;
            }
        }
    }
    return best ? best : current;
}

MaterialRecipe LumenMaterialsEffect::toned(const MaterialRecipe &base, const MicaCache *wallpaper) const
{
    if (m_toneOpacity <= 0 || !wallpaper || !wallpaper->toneValid) {
        return base;
    }
    // Keep the material's luminance while borrowing the wallpaper's hue.
    QVector3D c = currentTone(*wallpaper);
    const QVector3D grey(luma(c), luma(c), luma(c));
    c = setLum(grey + (c - grey) * 2.0f, base.luminosity.x());
    const float a = base.tint.w() + (1.f - base.tint.w()) * m_toneOpacity * 0.4f;
    const QVector3D tint = base.tint.toVector3D() * (1.f - m_toneOpacity) + c * base.tint.w() * m_toneOpacity;
    return {QVector4D(tint + c * (a - base.tint.w()), a), base.luminosity};
}

QString LumenMaterialsEffect::debug(const QString &parameter) const
{
    Q_UNUSED(parameter)
    QString result = QStringLiteral("wallpaperTone=%1%\n").arg(m_toneOpacity * 100.f);
    for (const auto &[view, cache] : m_mica) {
        result += QStringLiteral("view=%1,%2 %3x%4 captured=%5 dirty=%6 toneValid=%7 toneDirty=%8 "
                                 "captures=%9 samples=%10 rgb=%11,%12,%13 captureUs=%14 sampleUs=%15 readError=%16\n")
                      .arg(cache.rect.x()).arg(cache.rect.y()).arg(cache.rect.width()).arg(cache.rect.height())
                      .arg(cache.captured).arg(cache.wallpaperDamaged).arg(cache.toneValid).arg(cache.toneDirty)
                      .arg(cache.captures).arg(cache.samples)
                      .arg(cache.toneTo.x()).arg(cache.toneTo.y()).arg(cache.toneTo.z())
                      .arg(cache.captures ? std::chrono::duration<double, std::micro>(cache.captureTime).count() / cache.captures : 0)
                      .arg(cache.samples ? std::chrono::duration<double, std::micro>(cache.sampleTime).count() / cache.samples : 0)
                      .arg(cache.toneReadError);
    }
    return result;
}

bool LumenMaterialsEffect::blur(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    auto it = m_windows.find(w);
    if (it == m_windows.end() || !shouldBlur(w, mask, data)) {
        return false;
    }
    MaterialWindowData &blurInfo = it->second;
    MicaCache *wallpaper = wallpaperFor(w);

    RegionF blurShape = blurRegion(w);
    if (data.xScale() != 1 || data.yScale() != 1) {
        blurShape.scale(data.xScale(), data.yScale());
    }
    if (data.xTranslation() || data.yTranslation()) {
        blurShape.translate(data.xTranslation(), data.yTranslation());
    }
    blurShape.translate(w->pos());

    const Rect backgroundRect = blurShape.boundingRect().rounded();
    const Rect scaledBackgroundRect = backgroundRect.scaled(viewport.scale()).rounded();
    const Rect deviceBackgroundRect = viewport.mapToDeviceCoordinates(backgroundRect).rounded();
    const auto opacity = w->opacity() * data.opacity();

    QList<RectF> effectiveShape;
    effectiveShape.reserve(blurShape.rects().size());
    if (deviceRegion != Region::infinite()) {
        for (const Rect &clipRect : deviceRegion.rects()) {
            const RectF deviceClipRect = clipRect.translated(-deviceBackgroundRect.topLeft());
            for (const RectF &shapeRect : blurShape.rects()) {
                const RectF deviceShapeRect = shapeRect.translated(-backgroundRect.topLeft()).scaled(viewport.scale()).rounded();
                if (const RectF intersected = deviceClipRect.intersected(deviceShapeRect); !intersected.isEmpty()) {
                    effectiveShape.append(intersected);
                }
            }
        }
    } else {
        for (const RectF &rect : blurShape.rects()) {
            effectiveShape.append(rect.translated(-backgroundRect.topLeft()).scaled(viewport.scale()).rounded());
        }
    }
    if (effectiveShape.isEmpty()) {
        return false;
    }
    const auto scissor = disableScissor();
    GLTexture *popup = nullptr;
    if (blurInfo.popup) {
        popup = capturePopup(renderTarget, viewport, w, mask, data, backgroundRect, blurInfo.capture[m_currentView]);
        if (!popup) {
            return false;
        }
    }

    const float modulation = opacity * opacity;
    const int vertexCount = effectiveShape.size() * 6;
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();

    // Mica: sample the cached wallpaper blur at screen-aligned coordinates, no per-window passes.
    if (materialFor(w, blurInfo) == Material::Mica) {
        if (wallpaper) {
            MicaCache &cache = *wallpaper;
            const QSize cacheSize = cache.rect.size();
            vbo->reset();
            vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));
            auto result = vbo->map<GLVertex2D>(6 + vertexCount);
            if (!result) {
                return false;
            }
            auto map = *result;
            size_t i = 0;
            pushQuad(map, i, RectF(0, 0, cacheSize.width(), cacheSize.height()), RectF(0, 0, 1, 1));
            const qreal s = viewport.scale();
            for (const RectF &rect : effectiveShape) {
                const RectF global((rect.left() + scaledBackgroundRect.x()) / s, (rect.top() + scaledBackgroundRect.y()) / s, rect.width() / s, rect.height() / s);
                const RectF uv((global.left() - cache.rect.x()) / cacheSize.width(), (global.top() - cache.rect.y()) / cacheSize.height(),
                               global.width() / cacheSize.width(), global.height() / cacheSize.height());
                pushQuad(map, i, rect, uv);
            }
            vbo->unmap();
            vbo->bindArrays();
            if (cache.needsBlur) {
                kawase(cache.chain, cacheSize, m_micaOffset, vbo);
                GLFramebuffer::popFramebuffer();
                cache.needsBlur = false;
            }
            drawOnscreen(viewport, w, data, cache.chain.textures[1].get(), scaledBackgroundRect, m_micaColorMatrix, m_micaOffset, toned(m_micaRecipe, wallpaper), modulation, vertexCount);
            vbo->unbindArrays();
            return false;
        }
    }

    BlurChain &renderInfo = blurInfo.render[m_currentView];
    GLenum textureFormat = GL_RGBA8;
    if (renderTarget.texture()) {
        textureFormat = renderTarget.texture()->internalFormat();
    }
    if (!ensureChain(renderInfo, backgroundRect.size(), textureFormat, m_iterationCount)) {
        return false;
    }

    const Region dirtyRegion = viewport.mapFromDeviceCoordinatesContained(deviceRegion) & backgroundRect;
    for (const Rect &dirtyRect : dirtyRegion.rects()) {
        renderInfo.framebuffers[0]->blitFromRenderTarget(renderTarget, viewport, dirtyRect, dirtyRect.translated(-backgroundRect.topLeft()));
    }

    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));
    if (auto result = vbo->map<GLVertex2D>(6 + vertexCount)) {
        auto map = *result;
        size_t i = 0;
        pushQuad(map, i, RectF(0, 0, backgroundRect.width(), backgroundRect.height()), RectF(0, 0, 1, 1));
        for (const RectF &rect : effectiveShape) {
            const RectF uv(rect.left() / scaledBackgroundRect.width(), rect.top() / scaledBackgroundRect.height(), rect.width() / scaledBackgroundRect.width(),
                           rect.height() / scaledBackgroundRect.height());
            pushQuad(map, i, rect, uv);
        }
        vbo->unmap();
    } else {
        qCWarning(LUMEN_MATERIALS) << "Failed to map vertex buffer";
        return false;
    }
    vbo->bindArrays();

    kawase(renderInfo, backgroundRect.size(), float(m_offset), vbo);
    GLFramebuffer::popFramebuffer();
    if (popup) {
        drawPopup(viewport, renderInfo.textures[1].get(), popup, scaledBackgroundRect, opacity, blurInfo.keyOpaque, vertexCount, wallpaper);
        vbo->unbindArrays();
        return true;
    }
    drawOnscreen(viewport, w, data, renderInfo.textures[1].get(), scaledBackgroundRect, m_colorMatrix, float(m_offset), toned(m_acrylicRecipe, wallpaper), modulation, vertexCount);

    if (m_noiseStrength > 0) {
        // Additive noise masks banding in smooth blurred gradients (Fluent Acrylic grain).
        glEnable(GL_BLEND);
        if (opacity < 1.0) {
            glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
        } else {
            glBlendFunc(GL_ONE, GL_ONE);
        }
        if (GLTexture *noiseTexture = ensureNoiseTexture()) {
            ShaderManager::instance()->pushShader(m_noisePass.shader.get());
            QMatrix4x4 projectionMatrix = viewport.projectionMatrix();
            projectionMatrix.translate(scaledBackgroundRect.x(), scaledBackgroundRect.y());
            m_noisePass.shader->setUniform(m_noisePass.mvpMatrixLocation, projectionMatrix);
            m_noisePass.shader->setUniform(m_noisePass.noiseTextureSizeLocation, QVector2D(noiseTexture->width(), noiseTexture->height()));
            noiseTexture->bind();
            vbo->draw(GL_TRIANGLES, 6, vertexCount);
            ShaderManager::instance()->popShader();
        }
        glDisable(GL_BLEND);
    }
    vbo->unbindArrays();
    return false;
}

GLTexture *LumenMaterialsEffect::capturePopup(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const WindowPaintData &data,
                                              const Rect &rect, BlurChain &capture)
{
    const GLenum format = renderTarget.texture() ? renderTarget.texture()->internalFormat() : GL_RGBA8;
    if (!ensureChain(capture, rect.scaled(viewport.scale()).rounded().size(), format, 0)) {
        return nullptr;
    }
    GLFramebuffer *fbo = capture.framebuffers[0].get();
    const RenderTarget target(fbo, renderTarget.colorDescription());
    const RenderViewport offscreen(rect, viewport.scale(), target, QPoint());
    const auto scissor = disableScissor();
    GLFramebuffer::pushFramebuffer(fbo);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    WindowPaintData paintData = data;
    paintData.setOpacity(1.0); // applied when compositing, so coverage stays comparable to the background's
    const bool drawn = effects->drawWindow(target, offscreen, w, mask, Region::infinite(), paintData);
    GLFramebuffer::popFramebuffer();
    return drawn ? capture.textures[0].get() : nullptr;
}

void LumenMaterialsEffect::drawPopup(const RenderViewport &viewport, GLTexture *source, GLTexture *popup, const Rect &scaledBackgroundRect, float opacity, bool keyOpaque, int vertexCount, const MicaCache *wallpaper)
{
    QMatrix4x4 projectionMatrix = viewport.projectionMatrix();
    projectionMatrix.translate(scaledBackgroundRect.x(), scaledBackgroundRect.y());
    GLTexture *noise = ensureNoiseTexture();
    auto *s = m_popupPass.shader.get();
    ShaderManager::instance()->pushShader(s);
    s->setUniform(m_popupPass.mvpMatrixLocation, projectionMatrix);
    s->setUniform(m_popupPass.colorMatrixLocation, m_colorMatrix);
    s->setUniform(m_popupPass.halfpixelLocation, QVector2D(0.5 / source->width(), 0.5 / source->height()));
    s->setUniform(m_popupPass.offsetLocation, float(m_offset));
    for (int i = 0; i < 2; ++i) {
        const MaterialRecipe r = toned(recipe(i ? Lumen::T::Materials::AcrylicLight : Lumen::T::Materials::AcrylicDark), wallpaper);
        s->setUniform(m_popupPass.tintLocation[i], r.tint);
        s->setUniform(m_popupPass.luminosityLocation[i], r.luminosity);
    }
    s->setUniform(m_popupPass.opacityLocation, opacity);
    s->setUniform(m_popupPass.noiseSizeLocation, noise ? QVector2D(noise->width(), noise->height()) : QVector2D());
    s->setUniform(m_popupPass.keyOpaqueLocation, int(keyOpaque));
    if (noise) {
        glActiveTexture(GL_TEXTURE2);
        noise->bind();
    }
    glActiveTexture(GL_TEXTURE1);
    popup->bind();
    glActiveTexture(GL_TEXTURE0);
    source->bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    GLVertexBuffer::streamingBuffer()->draw(GL_TRIANGLES, 6, vertexCount);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE1);
    popup->unbind();
    if (noise) {
        glActiveTexture(GL_TEXTURE2);
        noise->unbind();
    }
    glActiveTexture(GL_TEXTURE0);
    ShaderManager::instance()->popShader();
}

bool LumenMaterialsEffect::isActive() const
{
    return m_valid && !effects->isScreenLocked();
}

bool LumenMaterialsEffect::blocksDirectScanout() const
{
    return false;
}

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(LumenMaterialsEffect,
                                      "metadata.json",
                                      return LumenMaterialsEffect::supported();
                                      ,
                                      return LumenMaterialsEffect::enabledByDefault();)

} // namespace KWin

#include "lumenmaterials.moc"
#include "moc_lumenmaterials.cpp"
