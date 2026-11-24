"use strict";

const blacklist = {
    "ksmserver ksmserver": [],
    "ksmserver-logout-greeter ksmserver-logout-greeter": [],
    "ksplashqml ksplashqml": [],
    "spectacle org.kde.spectacle": ["region-editor"],
};

class LumenOpenEffect {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effect.animationEnded.connect(LumenOpenEffect.releaseRoles);
        effects.windowAdded.connect(this.onAdded.bind(this));
        effects.windowClosed.connect(this.onClosed.bind(this));
        effects.windowDataChanged.connect(this.onDataChanged.bind(this));
        this.loadConfig();
    }

    loadConfig() {
        this.openDuration = animationTime(effect.readConfig("OpenDuration", @@motion.normal_ms@@));
        this.closeDuration = animationTime(effect.readConfig("CloseDuration", @@motion.fast_ms@@));
        this.openScale = effect.readConfig("OpenScale", @@motion.window_open_scale@@);
        this.closeScale = effect.readConfig("CloseScale", @@motion.window_close_scale@@);
    }

    static eligible(w) {
        if (w.windowClass == "plasmashell plasmashell" || w.windowClass == "plasmashell org.kde.plasmashell") {
            return w.hasDecoration;
        }
        const tags = blacklist[w.windowClass];
        if (tags && (tags.length === 0 || tags.includes(w.tag))) {
            return false;
        }
        if (w.hasDecoration) {
            return true;
        }
        if (w.popupWindow || w.lockScreen || w.outline || !w.managed) {
            return false;
        }
        return w.normalWindow || w.dialog;
    }

    static holdRoles(w) {
        w.setData(Effect.WindowForceBackgroundContrastRole, true);
        w.setData(Effect.WindowForceBlurRole, true);
    }

    static releaseRoles(w) {
        w.setData(Effect.WindowForceBackgroundContrastRole, null);
        w.setData(Effect.WindowForceBlurRole, null);
    }

    onAdded(w) {
        if (effects.hasActiveFullScreenEffect || !LumenOpenEffect.eligible(w) || !w.visible || effect.isGrabbed(w, Effect.WindowAddedGrabRole)) {
            return;
        }
        LumenOpenEffect.holdRoles(w);
        w.lumenOpen = animate({
            window: w,
            curve: QEasingCurve.OutQuint,
            duration: this.openDuration,
            animations: [
                { type: Effect.Scale, from: this.openScale },
                { type: Effect.Opacity, from: 0, curve: QEasingCurve.OutCubic },
            ],
        });
    }

    onClosed(w) {
        if (effects.hasActiveFullScreenEffect || !LumenOpenEffect.eligible(w) || !w.visible || w.skipsCloseAnimation || effect.isGrabbed(w, Effect.WindowClosedGrabRole)) {
            return;
        }
        if (w.lumenOpen) {
            cancel(w.lumenOpen);
            delete w.lumenOpen;
        }
        LumenOpenEffect.holdRoles(w);
        w.lumenClose = animate({
            window: w,
            curve: QEasingCurve.InCubic,
            duration: this.closeDuration,
            animations: [
                { type: Effect.Scale, to: this.closeScale },
                { type: Effect.Opacity, to: 0 },
            ],
        });
    }

    onDataChanged(w, role) {
        const key = role == Effect.WindowAddedGrabRole ? "lumenOpen" : role == Effect.WindowClosedGrabRole ? "lumenClose" : null;
        if (key && w[key] && effect.isGrabbed(w, role)) {
            cancel(w[key]);
            delete w[key];
            LumenOpenEffect.releaseRoles(w);
        }
    }
}

new LumenOpenEffect();
