#include "theme.h"
#include "preferences.h"

#include <QApplication>
#include <QStyleFactory>
#include <QAbstractButton>
#include <QAction>
#include <QEvent>
#include <QImage>
#include <QPixmap>
#include <QWidget>
#include <QAbstractSpinBox>
#include <QFontMetrics>
#include <QFont>
#include <cmath>
#include <algorithm>
#include <utility>

Theme::Kind Theme::kind = Theme::Kind::Light;
bool Theme::tablet = false;

namespace {

const char *PROP_ORIG = "theme_origIcon";
const char *PROP_SAMPLE = "theme_widthSample";
const char *PROP_KIND = "theme_iconKind";

// true if the icon is a dark glyph on transparent background
bool isDarkGlyph(const QIcon &icon)
{
    if(icon.isNull()) {
        return false;
    }
    auto img = icon.pixmap(32, 32).toImage().convertToFormat(QImage::Format_ARGB32);
    long sum = 0, n = 0;
    for(int y=0;y<img.height();y++) {
        for(int x=0;x<img.width();x++) {
            auto px = img.pixel(x, y);
            if(qAlpha(px) > 128) {
                sum += qGray(px);
                n++;
            }
        }
    }
    return n > 0 && (sum / n) < 96;
}

QIcon invertIcon(const QIcon &icon)
{
    QIcon out;
    auto sizes = icon.availableSizes();
    if(sizes.isEmpty()) {
        sizes = {QSize(16,16), QSize(24,24), QSize(32,32), QSize(48,48), QSize(64,64), QSize(96,96)}; // SVG icons: cover 150 % scaling
    }
    for(auto sz : sizes) {
        auto img = icon.pixmap(sz).toImage().convertToFormat(QImage::Format_ARGB32);
        img.invertPixels(QImage::InvertRgb);
        out.addPixmap(QPixmap::fromImage(img));
    }
    return out;
}

// Adapts icons of buttons and actions as widgets get created/changed
class IconFilter : public QObject
{
public:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        auto t = e->type();
        if(t == QEvent::Polish) {
            if(auto b = qobject_cast<QAbstractButton*>(o)) {
                Theme::adaptIcon(b);
            }
            if(auto w = qobject_cast<QWidget*>(o)) {
                if(Theme::isReadout(w)) {
                    w->setFont(Theme::readoutFont());
                }
            }
            if(auto w = qobject_cast<QWidget*>(o)) {
                for(auto a : w->actions()) {
                    Theme::adaptIcon(a);
                }
            }
        } else if(t == QEvent::ActionAdded || t == QEvent::ActionChanged) {
            if(auto w = qobject_cast<QWidget*>(o)) {
                for(auto a : w->actions()) {
                    Theme::adaptIcon(a);
                }
            }
        }
        return false;
    }
};

IconFilter *iconFilter = nullptr;

struct Colors {
    QString window, base, alt, text, textDim, border, accent, accentText, accentLine, hover, pressed, disabled;
};

// Bench-instrument palette. The graphs are near-black with an amber trace, so the
// chrome around them is neutral graphite (no blue cast) and the single accent
// is the amber of the trace / of an illuminated instrument key.
Colors colorsFor(Theme::Kind k)
{
    if(k == Theme::Kind::Scope) {
        // painted front panel of an old scope: warm grey enamel, black lettering, green indicator lamps
        return {
            "#c8cac3", // window
            "#e2e4dd", // base
            "#d6d8d1", // alternate rows
            "#1f2420", // text
            "#5c635d", // dim text
            "#9aa096", // border
            "#2f8f4e", // accent fill (green lamp)
            "#f2f7f0", // text on accent
            "#1f6f3a", // accent line
            "#bcbfb7", // hover
            "#adb0a8", // pressed
            "#7f857f", // disabled text
        };
    }
    if(k == Theme::Kind::Acid) {
        // green-tinted graphite, everything that glows is acid green
        return {
            "#0f1412", // window
            "#151c18", // base
            "#121916", // alternate rows
            "#d9f0d5", // text (greenish white)
            "#86a08a", // dim text
            "#28362c", // border
            "#39ff14", // accent fill
            "#0a0f0c", // text on accent
            "#39ff14", // accent line
            "#1a261e", // hover
            "#22322a", // pressed
            "#5a6e5f", // disabled text
        };
    }
    if(k == Theme::Kind::Dark) {
        return {
            "#141618", // window
            "#1c1f22", // base (inputs, lists)
            "#191c1f", // alternate rows
            "#e8e6df", // text (slightly warm, matches the trace colour)
            "#9a9a92", // dim text
            "#33383d", // border
            "#f5b800", // accent fill (selection, checked keys)
            "#141618", // text on accent
            "#f5b800", // accent line (focus, links, active tab)
            "#26292d", // hover
            "#30343a", // pressed
            "#676b70", // disabled text
        };
    }
    return {
        "#e9eaec", // window: grey bench, not cream
        "#ffffff", // base
        "#f2f3f5", // alternate rows
        "#1b1d1f", // text
        "#5f646a", // dim text
        "#b9bec5", // border
        "#f5b800", // accent fill
        "#1b1d1f", // text on accent
        "#8a5a00", // accent line: darker amber for 4.5:1 on white
        "#dfe2e6", // hover
        "#cfd3d8", // pressed
        "#8d9297", // disabled text
    };
}


// Trace palette, ordered so that with up to four ports S21 / S22 / ... keep their
// index (i*4+j), the way the upstream default colors did. Dark entries are tuned
// for the graphite plot background, light entries are the same hues darkened for white.
const QColor tracePaletteDark[] = {
    QColor("#f5b800"), // 0  amber (S11)
    QColor("#6da2f0"), // 1  sky
    QColor("#3fb8d6"), // 2  cyan
    QColor("#3a9d8f"), // 3  teal
    QColor("#5fc26b"), // 4  green (S21)
    QColor("#ef6c4d"), // 5  coral (S22)
    QColor("#b5d24a"), // 6  lime
    QColor("#d4b483"), // 7  sand
    QColor("#a58cf2"), // 8  violet
    QColor("#ff8c00"), // 9  orange
    QColor("#e879b3"), // 10 pink
    QColor("#c8a2ff"), // 11 lavender
    QColor("#f2a07b"), // 12 salmon
    QColor("#a0a4a8"), // 13 grey
    QColor("#7fd8c4"), // 14 mint
    QColor("#d64545"), // 15 red
};
const QColor tracePaletteLight[] = {
    // same order, ~4.5:1 on white so a 1 px line still reads
    QColor("#8a5a00"), QColor("#1f5bb5"), QColor("#147a94"), QColor("#1f665c"),
    QColor("#22782e"), QColor("#b83a22"), QColor("#607a0a"), QColor("#806230"),
    QColor("#6543c2"), QColor("#b85c00"), QColor("#a8306f"), QColor("#7040cc"),
    QColor("#b55520"), QColor("#55595e"), QColor("#1a8a70"), QColor("#9e1c1c"),
};
constexpr int tracePaletteCount = 16;
const QColor tracePaletteScope[] = {
    QColor("#9dff9d"), // 0  P31 phosphor green (S11)
    QColor("#6da2f0"), QColor("#3fb8d6"), QColor("#3a9d8f"),
    QColor("#f5b800"), // 4  amber (S21)
    QColor("#ef6c4d"), QColor("#b5d24a"), QColor("#d4b483"),
    QColor("#a58cf2"), QColor("#ff8c00"), QColor("#e879b3"), QColor("#c8a2ff"),
    QColor("#f2a07b"), QColor("#a0a4a8"), QColor("#7fd8c4"), QColor("#d64545"),
};
const QColor tracePaletteAcid[] = {
    QColor("#39ff14"), // 0  acid green (S11)
    QColor("#6da2f0"), QColor("#3fb8d6"), QColor("#3a9d8f"),
    QColor("#f5b800"), // 4  amber (S21)
    QColor("#ef6c4d"), QColor("#b5d24a"), QColor("#d4b483"),
    QColor("#a58cf2"), QColor("#ff8c00"), QColor("#e879b3"), QColor("#c8a2ff"),
    QColor("#f2a07b"), QColor("#a0a4a8"), QColor("#7fd8c4"), QColor("#d64545"),
};

}

QString Theme::kindName(Kind k)
{
    switch(k) {
    case Kind::Dark: return "Dark";
    case Kind::Acid: return "Acid green";
    case Kind::Scope: return "Phosphor scope";
    default: return "Light";
    }
}

Theme::GraphColors Theme::graphColors(Kind k)
{
    if(k == Kind::Scope) {
        // CRT: dark green glass, graticule lines clearly visible like the etched grid
        return { QColor("#0d1a12"), QColor("#8fcf9a"), QColor("#2f5c3c"), QColor("#112217") };
    }
    if(k == Kind::Acid) {
        return { QColor("#0a0f0c"), QColor("#b9d6b4"), QColor("#1f3324"), QColor("#0e1510") };
    }
    if(k == Kind::Dark) {
        // slightly darker than the window (#141618 chrome) so the plot reads as a "screen"
        return { QColor("#0f1113"), QColor("#c9c7c0"), QColor("#2e3339"), QColor("#15181b") };
    }
    return { QColor("#ffffff"), QColor("#24282c"), QColor("#c4c9cf"), QColor("#f2f3f5") };
}

void Theme::applyGraphColors()
{
    auto g = graphColors(kind);
    auto &c = Preferences::getInstance().Graphs.Color;
    c.background = g.background;
    c.axis = g.axis;
    c.Ticks.divisions = g.divisions;
    c.Ticks.Background.background = g.ticksBackground;
    for(auto w : qApp->allWidgets()) {
        w->update();
    }
}

int Theme::tracePaletteSize()
{
    return tracePaletteCount;
}

QColor Theme::traceColor(int index, Kind k)
{
    index = ((index % tracePaletteCount) + tracePaletteCount) % tracePaletteCount;
    switch(k) {
    case Kind::Acid: return tracePaletteAcid[index];
    case Kind::Scope: return tracePaletteScope[index];
    case Kind::Dark: return tracePaletteDark[index];
    default: return tracePaletteLight[index];
    }
}

QColor Theme::remapTraceColor(const QColor &c, Kind from, Kind to)
{
    // upstream default colors (setups saved before the palette existed) → same slot
    static const std::pair<QColor, int> legacy[] = {
        {Qt::yellow, 0}, {Qt::blue, 1}, {Qt::cyan, 2}, {Qt::darkCyan, 3},
        {Qt::green, 4}, {Qt::red, 5}, {Qt::darkGreen, 6}, {Qt::gray, 7},
        {Qt::darkBlue, 8}, {Qt::darkYellow, 9}, {Qt::magenta, 10}, {Qt::darkMagenta, 11},
        {Qt::darkGray, 13}, {Qt::lightGray, 13}, {Qt::darkRed, 15},
        {QColor(255, 140, 0), 9}, // former "S11 TDR" orange
    };
    for(auto &l : legacy) {
        if(l.first == c) {
            return traceColor(l.second, to);
        }
    }
    if(from == to) {
        return c;
    }
    for(int i=0;i<tracePaletteCount;i++) {
        if(traceColor(i, from) == c) {
            return traceColor(i, to);
        }
    }
    return c;
}

QColor Theme::readableOn(const QColor &c, const QColor &bg)
{
    auto luminance = [](const QColor &col) {
        auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * lin(col.redF()) + 0.7152 * lin(col.greenF()) + 0.0722 * lin(col.blueF());
    };
    auto contrast = [&](const QColor &a, const QColor &b) {
        double la = luminance(a), lb = luminance(b);
        return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
    };
    const double target = 3.0;
    bool lightBg = luminance(bg) > 0.5;
    QColor r = c;
    for(int i=0;i<12 && contrast(r, bg) < target;i++) {
        r = lightBg ? r.darker(115) : r.lighter(115);
    }
    return r;
}

Theme::GraphMetrics Theme::graphMetrics()
{
    auto &g = Preferences::getInstance().Graphs;
    const double fs = graphFontScale();
    const double ls = graphLineScale();
    auto px = [fs](int v) { return (int) std::lround(v * fs); };
    GraphMetrics m;
    m.fontSizeTitle = px(g.fontSizeTitle);
    m.fontSizeAxis = px(g.fontSizeAxis);
    m.fontSizeCursorOverlay = px(g.fontSizeCursorOverlay);
    m.fontSizeMarkerData = px(g.fontSizeMarkerData);
    m.fontSizeTraceNames = px(g.fontSizeTraceNames);
    m.lineWidth = g.lineWidth * ls;
    m.triangleSize = (int) std::lround(g.SweepIndicator.triangleSize * ls);
    return m;
}

QFont Theme::graphFont(int pixelSize)
{
    QFont f = readoutFont();
    f.setPixelSize(pixelSize);
    return f;
}

QColor Theme::graphTextColor(Kind k)
{
    switch(k) {
    case Kind::Acid: return QColor("#d9f0d5");
    case Kind::Scope: return QColor("#d6f5d6");
    case Kind::Dark: return QColor("#e8e6df");
    default: return QColor("#1b1d1f");
    }
}

QColor Theme::limitColor(Kind k)
{
    return (isDark(k) || k == Kind::Scope) ? QColor("#ff5c5c") : QColor("#c62828");
}

QPalette Theme::palette(Kind k)
{
    auto c = colorsFor(k);
    QPalette p;
    p.setColor(QPalette::Window, QColor(c.window));
    p.setColor(QPalette::WindowText, QColor(c.text));
    p.setColor(QPalette::Base, QColor(c.base));
    p.setColor(QPalette::AlternateBase, QColor(c.alt));
    p.setColor(QPalette::Text, QColor(c.text));
    p.setColor(QPalette::Button, QColor(c.window));
    p.setColor(QPalette::ButtonText, QColor(c.text));
    p.setColor(QPalette::ToolTipBase, QColor(c.base));
    p.setColor(QPalette::ToolTipText, QColor(c.text));
    p.setColor(QPalette::Highlight, QColor(c.accent));
    p.setColor(QPalette::HighlightedText, QColor(c.accentText));
    p.setColor(QPalette::Link, QColor(c.accentLine));
    p.setColor(QPalette::PlaceholderText, QColor(c.textDim));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(c.disabled));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(c.disabled));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(c.disabled));
    return p;
}

QString Theme::styleSheet(Kind k, bool tablet)
{
    auto c = colorsFor(k);
    const int h = tablet ? 40 : 26;      // control height
    const int pad = tablet ? 8 : 3;      // inner padding
    const int fs = fontPointSize();      // font size, pt
    const int tabH = tablet ? 38 : 26;

    QString s = R"(
QWidget { font-size: %FS%pt; }

QMainWindow::separator { background: %PRESSED%; width: %SPL%px; height: %SPL%px; }

QToolBar {
    background: %WINDOW%;
    border-bottom: 1px solid %BORDER%;
    spacing: %PAD%px;
    padding: 2px;
}
QToolBar::separator { background: %DISABLED%; width: %TBSEP%px; margin: 4px 4px; }

QDockWidget::title {
    background: %ALT%;
    border-bottom: 1px solid %BORDER%;
    padding: %PAD%px;
    text-align: left;
}

QMenuBar { background: %WINDOW%; border-bottom: 1px solid %BORDER%; }
QMenuBar::item { padding: %PAD%px 10px; background: transparent; }
QMenuBar::item:selected { background: %HOVER%; }
QMenu { background: %BASE%; border: 1px solid %BORDER%; padding: 4px; }
QMenu::item { padding: %PAD%px 24px %PAD%px 24px; min-height: %H%px; }
QMenu::item:selected { background: %ACCENT%; color: %ACCENTTEXT%; }
QMenu::separator { height: 1px; background: %BORDER%; margin: 4px 8px; }

QPushButton, QToolButton {
    background: %BASE%;
    border: 1px solid %BORDER%;
    border-radius: 2px;
    padding: %PAD%px 10px;
    min-height: %H%px;
}
QToolButton { padding: %PAD%px; min-width: %H%px; }
QPushButton:hover, QToolButton:hover { background: %HOVER%; }
QPushButton:pressed, QToolButton:pressed { background: %PRESSED%; }
QPushButton:focus, QToolButton:focus { border: 1px solid %ACCENTLINE%; }
QPushButton:checked, QToolButton:checked { background: %ACCENT%; color: %ACCENTTEXT%; border-color: %ACCENT%; }
QPushButton:checked:hover, QToolButton:checked:hover { background: %ACCENT%; }
QPushButton:disabled, QToolButton:disabled { color: %DISABLED%; }
QToolButton::menu-indicator { subcontrol-position: bottom right; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
    background: %BASE%;
    border: 1px solid %BORDER%;
    border-radius: 2px;
    padding: %PAD%px 6px;
    min-height: %H%px;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENTTEXT%;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid %ACCENTLINE%; }
QComboBox::drop-down { width: %H%px; border: none; }
QComboBox QAbstractItemView { background: %BASE%; border: 1px solid %BORDER%; selection-background-color: %ACCENT%; }
QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: %H%px; }

QCheckBox::indicator, QRadioButton::indicator { width: %IND%px; height: %IND%px; }
QCheckBox, QRadioButton { spacing: 8px; min-height: %H%px; }

QTabBar::tab {
    background: %ALT%;
    border: 1px solid %BORDER%;
    border-bottom: none;
    padding: %PAD%px 14px;
    min-height: %TABH%px;
}
QTabBar::tab:selected { background: %BASE%; border-bottom: 2px solid %ACCENTLINE%; }
QTabBar::tab:hover { background: %HOVER%; }
QTabWidget::pane { border: 1px solid %BORDER%; top: -1px; }

QTableView, QTreeView, QListView {
    background: %BASE%;
    alternate-background-color: %ALT%;
    border: 1px solid %BORDER%;
    gridline-color: %BORDER%;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENTTEXT%;
}
QHeaderView::section { background: %ALT%; border: none; border-right: 1px solid %BORDER%; border-bottom: 1px solid %BORDER%; padding: %PAD%px; }
QTableView::item, QTreeView::item, QListView::item { min-height: %H%px; }

QScrollBar:vertical { background: %WINDOW%; width: %SB%px; margin: 0; }
QScrollBar:horizontal { background: %WINDOW%; height: %SB%px; margin: 0; }
QScrollBar::handle { background: %BORDER%; border-radius: 4px; min-height: 24px; min-width: 24px; }
QScrollBar::handle:hover { background: %DISABLED%; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }

QGroupBox { border: 1px solid %BORDER%; border-radius: 2px; margin-top: 10px; padding-top: 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }

QStatusBar { background: %WINDOW%; border-top: 1px solid %BORDER%; }
QStatusBar::item { border: none; }

QToolTip { background: %BASE%; color: %TEXT%; border: 1px solid %BORDER%; padding: 4px; }
QSlider::groove:horizontal { height: 6px; background: %BORDER%; border-radius: 3px; }
QSlider::handle:horizontal { width: %SLH%px; height: %SLH%px; margin: -%SLM%px 0; background: %ACCENT%; border-radius: %SLR%px; }
QSplitter::handle { background: %PRESSED%; }
QSplitter::handle:horizontal { width: %SPL%px; }
QSplitter::handle:vertical { height: %SPL%px; }
QSplitter::handle:hover { background: %DISABLED%; }
)";

    s.replace("%FS%", QString::number(fs));
    s.replace("%H%", QString::number(h));
    s.replace("%PAD%", QString::number(pad));
    s.replace("%TABH%", QString::number(tabH));
    s.replace("%IND%", QString::number(tablet ? 24 : 16));
    s.replace("%SB%", QString::number(tablet ? 16 : 10));
    s.replace("%SLH%", QString::number(tablet ? 24 : 16));
    s.replace("%SPL%", QString::number(tablet ? 10 : 4));    // splitter / dock separator thickness
    s.replace("%TBSEP%", QString::number(tablet ? 2 : 1));   // toolbar separator width
    s.replace("%SLM%", QString::number(tablet ? 9 : 5));
    s.replace("%SLR%", QString::number(tablet ? 12 : 8));
    s.replace("%WINDOW%", c.window);
    s.replace("%BASE%", c.base);
    s.replace("%ALT%", c.alt);
    s.replace("%TEXT%", c.text);
    s.replace("%BORDER%", c.border);
    s.replace("%ACCENT%", c.accent);
    s.replace("%ACCENTTEXT%", c.accentText);
    s.replace("%ACCENTLINE%", c.accentLine);
    s.replace("%HOVER%", c.hover);
    s.replace("%PRESSED%", c.pressed);
    s.replace("%DISABLED%", c.disabled);
    return s;
}

QIcon Theme::adapted(const QIcon &orig)
{
    if(isDark(kind) && isDarkGlyph(orig)) {
        return invertIcon(orig);
    }
    return orig;
}

void Theme::adaptIcon(QAbstractButton *b)
{
    if(!b) return;
    if(b->property(PROP_KIND).isValid() && b->property(PROP_KIND).toInt() == (int) kind) {
        return;
    }
    QIcon orig;
    if(b->property(PROP_ORIG).isValid()) {
        orig = b->property(PROP_ORIG).value<QIcon>();
    } else {
        orig = b->icon();
        if(orig.isNull()) return;
        b->setProperty(PROP_ORIG, QVariant::fromValue(orig));
    }
    b->setProperty(PROP_KIND, (int) kind);
    b->setIcon(adapted(orig));
}

void Theme::adaptIcon(QAction *a)
{
    if(!a) return;
    if(a->property(PROP_KIND).isValid() && a->property(PROP_KIND).toInt() == (int) kind) {
        return;
    }
    QIcon orig;
    if(a->property(PROP_ORIG).isValid()) {
        orig = a->property(PROP_ORIG).value<QIcon>();
    } else {
        orig = a->icon();
        if(orig.isNull()) return;
        a->setProperty(PROP_ORIG, QVariant::fromValue(orig));
    }
    a->setProperty(PROP_KIND, (int) kind);
    a->setIcon(adapted(orig));
}

QFont Theme::readoutFont()
{
    QFont f = qApp->font();
    f.setFamilies({"Cascadia Mono", "Consolas", "Courier New"});
    f.setStyleHint(QFont::Monospace);
    f.setPointSize(fontPointSize());
    return f;
}

bool Theme::isReadout(const QWidget *w)
{
    if(!w) return false;
    return qobject_cast<const QAbstractSpinBox*>(w) || w->inherits("SIUnitEdit");
}

void Theme::fitWidth(QWidget *w, const QString &sample)
{
    if(!w) return;
    w->setProperty(PROP_SAMPLE, sample);
    QFont f = isReadout(w) ? readoutFont() : qApp->font();
    f.setPointSize(fontPointSize());
    int width = QFontMetrics(f).horizontalAdvance(sample);
    width += 2 * (tablet ? 8 : 3) + 2 * 6 + 4;   // padding + inner margins + border
    if(qobject_cast<QAbstractSpinBox*>(w)) {
        width += controlHeight();                 // up/down buttons
    }
    w->setFixedWidth(width);
}

void Theme::refreshAllWidths()
{
    for(auto w : qApp->allWidgets()) {
        auto v = w->property(PROP_SAMPLE);
        if(v.isValid()) {
            fitWidth(w, v.toString());
        }
    }
}

void Theme::refreshAllIcons()
{
    for(auto w : qApp->allWidgets()) {
        if(auto b = qobject_cast<QAbstractButton*>(w)) {
            adaptIcon(b);
        }
        for(auto a : w->actions()) {
            adaptIcon(a);
        }
    }
}

void Theme::apply(Kind k, bool t)
{
    kind = k;
    tablet = t;
    if(!iconFilter) {
        iconFilter = new IconFilter;
        qApp->installEventFilter(iconFilter);
    }
    // Fusion renders palettes consistently on all platforms
    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setPalette(palette(k));
    qApp->setStyleSheet(styleSheet(k, t));
    refreshAllIcons();
    for(auto w : qApp->allWidgets()) {
        if(isReadout(w)) {
            w->setFont(readoutFont());
        }
    }
    refreshAllWidths();
    if(Preferences::getInstance().UI.graphColorsFollowTheme) {
        applyGraphColors();
    }
}

void Theme::applyFromPreferences()
{
    auto &p = Preferences::getInstance();
    // UI.theme (0 light / 1 dark / 2 acid) supersedes the old UI.darkTheme flag
    if(p.UI.theme < 0 || p.UI.theme > (int) Kind::Scope) {
        p.UI.theme = p.UI.darkTheme ? (int) Kind::Dark : (int) Kind::Light;
    }
    p.UI.darkTheme = p.UI.theme != (int) Kind::Light;
    apply((Kind) p.UI.theme, p.UI.tabletMode);
}
