"use strict";

class LumenMinimizeEffect {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effect.animationEnded.connect((w) => w.setData(Effect.WindowForceBlurRole, null));
        effects.windowAdded.connect(this.watch.bind(this));
        effects.stackingOrder.forEach(this.watch.bind(this));
        this.loadConfig();
    }

    loadConfig() {
        this.duration = animationTime(effect.readConfig("Duration", @@motion.minimize_ms@@));
    }

    /// Geometry of the window shrunk to its taskbar icon, keeping aspect for a softer arc.
    static target(w) {
        const icon = w.iconGeometry;
        const frame = w.geometry;
        if (icon.width <= 0 || icon.height <= 0) {
            return null;
        }
        const scale = Math.max(icon.width / frame.width, icon.height / frame.height) * 2;
        return {
            width: Math.max(1, frame.width * scale),
            height: Math.max(1, frame.height * scale),
            dx: icon.x + icon.width / 2 - (frame.x + frame.width / 2),
            dy: icon.y + icon.height / 2 - (frame.y + frame.height / 2),
        };
    }

    run(w, minimizing) {
        if (effects.hasActiveFullScreenEffect || !w.visible && !minimizing) {
            return;
        }
        const t = LumenMinimizeEffect.target(w);
        if (!t) {
            return;
        }
        if (w.lumenMinimize) {
            cancel(w.lumenMinimize);
            delete w.lumenMinimize;
        }
        w.setData(Effect.WindowForceBlurRole, true);
        const small = { value1: t.width, value2: t.height };
        const full = { value1: w.geometry.width, value2: w.geometry.height };
        const moved = { value1: t.dx, value2: t.dy };
        const home = { value1: 0, value2: 0 };
        w.lumenMinimize = animate({
            window: w,
            duration: this.duration,
            curve: minimizing ? QEasingCurve.InOutCubic : QEasingCurve.OutQuint,
            animations: [
                { type: Effect.Size, from: minimizing ? full : small, to: minimizing ? small : full },
                { type: Effect.Translation, from: minimizing ? home : moved, to: minimizing ? moved : home },
                { type: Effect.Opacity, from: minimizing ? 1 : 0, to: minimizing ? 0 : 1 },
            ],
        });
    }

    watch(w) {
        w.minimizedChanged.connect(() => this.run(w, w.minimized));
    }
}

new LumenMinimizeEffect();
