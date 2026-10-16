#include "theme.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFile>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>
#include <QSettings>
#include <QSvgRenderer>
#include <QStyle>
#include <QStyleFactory>

#include "pencildef.h"
#include "pencilsettings.h"

namespace
{
QPalette darkPalette()
{
    QPalette p;
    // Link and placeholder colors keep WCAG AA contrast (4.5:1) on base.
    const QColor window(53, 53, 53), base(35, 35, 35), text(Qt::white), disabled(127, 127, 127);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, window);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(90, 160, 240));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, disabled);
    return p;
}

double luminance(const QColor& c)
{
    auto channel = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}

double contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// Darkens on light panels, lightens on dark ones, until marks keep 3:1 (WCAG non-text).
void setAccent(QPalette& p, Theme::Accent accent)
{
    static const QRgb base[Theme::ACCENT_COUNT] = {
        0x3d8bfd, 0x14a3a3, 0x3fae5a, 0xe0a800, 0xf07f2c, 0xe5484d, 0xe0559b, 0x8e6cf0 };
    const QColor window = p.color(QPalette::Window), field = p.color(QPalette::Base);
    const bool dark = window.lightness() < 128;
    QColor c(base[accent]);
    for (int i = 0; i < 30 && (contrast(c, window) < 3.0 || contrast(c, field) < 3.0); ++i)
        c = dark ? c.lighter(106) : c.darker(106);
    const QColor text = contrast(Qt::white, c) >= contrast(Qt::black, c) ? Qt::white : Qt::black;
    p.setColor(QPalette::Highlight, c);
    p.setColor(QPalette::HighlightedText, text);
}

class TintedSvgEngine : public QIconEngine
{
public:
    TintedSvgEngine(const QString& path, QPalette::ColorRole on) : mPath(path), mOn(on)
    {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            mSvg = file.readAll();
        if (!QSvgRenderer(mSvg).isValid())
        {
            qWarning("Missing or invalid icon %s", qPrintable(path));
            mSvg.clear();
        }
    }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, pixmap(rect.size() * dpr, mode, state));
    }

    // QIcon passes the size in device pixels, so the result is sharp at any scale.
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        if (mSvg.isEmpty() || size.isEmpty())
            return QPixmap();
        const QPalette p = QApplication::palette();
        QColor color = p.color(state == QIcon::On ? mOn : QPalette::WindowText);
        if (mode == QIcon::Disabled)
            color = p.color(QPalette::Disabled, QPalette::WindowText);
        else if (mode == QIcon::Selected)
            color = p.color(QPalette::HighlightedText);

        const QString key = QStringLiteral("ja-icon:%1:%2x%3:%4")
            .arg(mPath).arg(size.width()).arg(size.height()).arg(color.rgba(), 0, 16);
        QPixmap pm;
        if (QPixmapCache::find(key, &pm))
            return pm;
        QByteArray svg = mSvg;
        svg.replace("currentColor", color.name().toLatin1());
        pm = QPixmap(size);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        QSvgRenderer(svg).render(&painter);
        painter.end();
        QPixmapCache::insert(key, pm);
        return pm;
    }

    bool isNull() override { return mSvg.isEmpty(); }
    QIconEngine* clone() const override { return new TintedSvgEngine(*this); }

private:
    QString mPath;
    QPalette::ColorRole mOn;
    QByteArray mSvg;
};

// Captured on first use, before any change, so System restores the platform look.
struct Original
{
    QString style = QApplication::style()->objectName();
    QPalette palette = QApplication::palette();
};

const Original& original()
{
    static const Original o;
    return o;
}
}

namespace Theme
{
Mode saved()
{
    QSettings& s = pencilSettings();
    const int v = s.value(SETTING_THEME, System).toInt();
    return (v == Light || v == Dark) ? static_cast<Mode>(v) : System;
}

void save(Mode mode)
{
    pencilSettings().setValue(SETTING_THEME, static_cast<int>(mode));
}

Accent savedAccent()
{
    const int v = pencilSettings().value(SETTING_ACCENT, Blue).toInt();
    return (v >= 0 && v < ACCENT_COUNT) ? static_cast<Accent>(v) : Blue;
}

void saveAccent(Accent accent)
{
    pencilSettings().setValue(SETTING_ACCENT, static_cast<int>(accent));
}

QPalette palette(Mode mode, Accent accent)
{
    QPalette p;
    switch (mode)
    {
    case Dark: p = darkPalette(); break;
    case Light: p = QPalette(QColor(240, 240, 240)); break;
    default:
        p = original().palette;
        // Every role set: setPalette fills unset roles from the current palette, so Dark would stay.
        p.setResolveMask(~QPalette::ResolveMask(0));
        break;
    }
    setAccent(p, accent);
    return p;
}

void apply(Mode mode)
{
    // Capture the platform look before the first change, whatever mode comes first.
    const Original& platform = original();
    const QString style = (mode == System) ? platform.style : QStringLiteral("Fusion");
    // Palette first: setStyle repolishes, so style sheets resolve palette(highlight) to the new accent.
    QApplication::setPalette(palette(mode));
    if (QStyleFactory::keys().contains(style, Qt::CaseInsensitive))
        QApplication::setStyle(style);
}

bool isDark()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

QIcon icon(const QString& svgPath, QPalette::ColorRole on)
{
    return QIcon(new TintedSvgEngine(svgPath, on));
}
}
