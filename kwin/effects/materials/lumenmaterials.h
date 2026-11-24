/*
    Lumen materials: KWin 6.8 blur effect extended with a Fluent Mica material.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
    SPDX-FileCopyrightText: 2026 Lumen

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "opengl/glutils.h"
#include "scene/item.h"

#include <KConfigWatcher>

#include <QList>
#include <QTimer>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <chrono>

#include <unordered_map>

namespace KWin
{

class BackgroundEffectItem;

/// Dual Kawase render targets; textures[0] holds the unblurred source.
/// Material layers drawn over the blurred backdrop (see onscreen.frag).
struct MaterialRecipe
{
    QVector4D tint; // premultiplied
    QVector2D luminosity; // target luma, opacity
};

struct BlurChain
{
    std::vector<std::unique_ptr<GLTexture>> textures;
    std::vector<std::unique_ptr<GLFramebuffer>> framebuffers;
};

struct MaterialWindowData
{
    std::optional<RegionF> content;
    std::optional<RegionF> frame;
    std::unordered_map<RenderView *, BlurChain> render;
    std::unique_ptr<BackgroundEffectItem> blurItem;
    bool popup = false; // client paints its own background and asked for no blur
    bool keyOpaque = true; // false: only a translucent background is keyed (Gecko: opaque means web content)
    std::unordered_map<RenderView *, BlurChain> capture;
};

/// Blurred wallpaper per output, captured when the desktop window repaints.
struct MicaCache
{
    BlurChain chain;
    Rect rect;
    EffectWindow *desktop = nullptr;
    LogicalOutput *output = nullptr;
    bool captured = false;
    bool needsBlur = true;
    bool wallpaperDamaged = true;

    // Wallpaper tone: average color of the blurred wallpaper, eased from the previous one.
    QVector3D toneFrom;
    QVector3D toneTo;
    std::chrono::steady_clock::time_point toneStart;
    std::chrono::steady_clock::time_point lastCapture;
    bool toneValid = false;
    bool toneDirty = true;
    uint64_t captures = 0;
    uint64_t samples = 0;
    GLenum toneReadError = GL_NO_ERROR;
    std::chrono::nanoseconds captureTime{};
    std::chrono::nanoseconds sampleTime{};
};

class LumenMaterialsEffect : public KWin::Effect
{
    Q_OBJECT

public:
    LumenMaterialsEffect();
    ~LumenMaterialsEffect() override;

    static bool supported();
    static bool enabledByDefault();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    bool paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;
    bool drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data) override;

    bool provides(Feature feature) override;
    bool isActive() const override;
    QString debug(const QString &parameter) const override;

    int requestedEffectChainPosition() const override
    {
        return 20;
    }

    bool eventFilter(QObject *watched, QEvent *event) override;
    bool blocksDirectScanout() const override;

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotViewRemoved(KWin::RenderView *view);
#if KWIN_BUILD_X11
    void slotPropertyNotify(KWin::EffectWindow *w, long atom);
#endif
    void setupDecorationConnections(EffectWindow *w);

private:
    enum class Material {
        Acrylic,
        Mica,
    };

    void initBlurStrengthValues();
    void readColorScheme();
    void repaintMaterials();
    RegionF blurRegion(EffectWindow *w) const;
    RegionF decorationBlurRegion(const EffectWindow *w) const;
    bool decorationSupportsBlurBehind(const EffectWindow *w) const;
    bool shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const;
    Material materialFor(const EffectWindow *w, const MaterialWindowData &d) const;
    void updateBlurRegion(EffectWindow *w);
    /// Returns true when it also drew the window (popups composited over their acrylic).
    bool blur(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data);
    static bool isPopup(const EffectWindow *w);
    static bool isGecko(EffectWindow *w);
    GLTexture *capturePopup(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const WindowPaintData &data, const Rect &rect,
                            BlurChain &capture);
    void drawPopup(const RenderViewport &viewport, GLTexture *source, GLTexture *popup, const Rect &scaledBackgroundRect, float opacity, bool keyOpaque, int vertexCount, const MicaCache *wallpaper);
    void captureWallpaper(EffectWindow *desktop, const RenderTarget &renderTarget, const RenderViewport &viewport);
    bool ensureChain(BlurChain &chain, const QSize &size, GLenum format, size_t iterations, bool *reallocated = nullptr) const;
    /// Runs down/up passes on `chain` using the first 6 vertices of the bound VBO; leaves framebuffers[1] pushed.
    void kawase(BlurChain &chain, const QSize &size, float offset, GLVertexBuffer *vbo);
    void drawOnscreen(const RenderViewport &viewport, EffectWindow *w, const WindowPaintData &data, GLTexture *source, const Rect &scaledBackgroundRect,
                      const QMatrix4x4 &colorMatrix, float offset, const MaterialRecipe &recipe, float modulation, int vertexCount);
    GLTexture *ensureNoiseTexture();
    /// Averages the smallest level of the wallpaper blur (one small readback per wallpaper change).
    void sampleTone(MicaCache &cache);
    void maybeSampleTone(MicaCache &cache);
    MicaCache *wallpaperFor(const EffectWindow *w);
    MaterialRecipe toned(const MaterialRecipe &base, const MicaCache *wallpaper) const;

    struct
    {
        std::unique_ptr<GLShader> shader;
        int mvpMatrixLocation;
        int colorMatrixLocation;
        int offsetLocation;
        int halfpixelLocation;
        int tintLocation;
        int luminosityLocation;
    } m_onscreenPass;

    struct
    {
        std::unique_ptr<GLShader> shader;
        int mvpMatrixLocation;
        int colorMatrixLocation;
        int offsetLocation;
        int halfpixelLocation;
        int boxLocation;
        int cornerRadiusLocation;
        int opacityLocation;
        int tintLocation;
        int luminosityLocation;
    } m_roundedOnscreenPass;

    struct
    {
        std::unique_ptr<GLShader> shader;
        int mvpMatrixLocation;
        int colorMatrixLocation;
        int offsetLocation;
        int halfpixelLocation;
        int tintLocation[2];
        int luminosityLocation[2];
        int opacityLocation;
        int noiseSizeLocation;
        int keyOpaqueLocation;
    } m_popupPass;

    struct
    {
        std::unique_ptr<GLShader> shader;
        int mvpMatrixLocation;
        int offsetLocation;
        int halfpixelLocation;
    } m_downsamplePass;

    struct
    {
        std::unique_ptr<GLShader> shader;
        int mvpMatrixLocation;
        int offsetLocation;
        int halfpixelLocation;
    } m_upsamplePass;

    struct
    {
        std::unique_ptr<GLShader> shader;
        int mvpMatrixLocation;
        int noiseTextureSizeLocation;

        std::unique_ptr<GLTexture> noiseTexture;
        qreal noiseTextureScale = 1.0;
        int noiseTextureStength = 0;
    } m_noisePass;

    bool m_valid = false;
#if KWIN_BUILD_X11
    long net_wm_blur_region = 0;
#endif
    RenderView *m_currentView = nullptr;

    QMatrix4x4 m_colorMatrix;
    size_t m_iterationCount;
    float m_offset;
    int m_expandSize;
    int m_noiseStrength;

    QMatrix4x4 m_micaColorMatrix;
    size_t m_micaIterations;
    float m_micaOffset;
    MaterialRecipe m_micaRecipe;
    MaterialRecipe m_acrylicRecipe;
    bool m_dark = true;
    bool m_micaForPanels = false;
    float m_toneOpacity = 0; // tint opacity the wallpaper tone brings in, 0 = off
    QTimer m_toneTimer; // samples the tone once the wallpaper settles

    struct OffsetStruct
    {
        float minOffset;
        float maxOffset;
        int expandSize;
    };
    QList<OffsetStruct> blurOffsets;

    struct BlurValuesStruct
    {
        int iteration;
        float offset;
    };
    QList<BlurValuesStruct> blurStrengthValues;

    QMap<EffectWindow *, QMetaObject::Connection> windowBlurChangedConnections;
    std::unordered_map<EffectWindow *, MaterialWindowData> m_windows;
    std::unordered_map<RenderView *, MicaCache> m_mica;
    KConfigWatcher::Ptr m_kdeglobals;
};

inline bool LumenMaterialsEffect::provides(Effect::Feature feature)
{
    if (feature == Blur) {
        return true;
    }
    return KWin::Effect::provides(feature);
}

} // namespace KWin
