#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QPalette>
#include <QIcon>
#include <QFont>
#include <QColor>

class QAbstractButton;
class QAction;
class QWidget;

// Application-wide look: light/dark palette + stylesheet,
// optional "tablet" mode with enlarged touch targets.
class Theme
{
public:
    enum class Kind {
        Light = 0,
        Dark = 1,
        Acid = 2,   // dark, green-tinted graphite with an acid-green accent / S11
        Scope = 3,  // old CRT oscilloscope: grey painted panel, green phosphor screen with a bright graticule
    };
    static bool isDark(Kind k) { return k == Kind::Dark || k == Kind::Acid; }
    static bool isDark() { return isDark(kind); }
    static QString kindName(Kind k);

    // Applies palette and stylesheet to the whole application.
    static void apply(Kind kind, bool tablet);
    // Re-applies whatever is currently stored in Preferences.
    static void applyFromPreferences();

    static Kind current() { return kind; }
    static bool isTablet() { return tablet; }

    // Minimum height for interactive widgets in current mode (px)
    static int controlHeight() { return tablet ? 40 : 26; }
    static int iconSize() { return tablet ? 28 : 20; }

    // Monochrome (black glyph) icons are inverted in the dark theme.
    // Called automatically for buttons/actions as they appear; can be
    // called manually for icons set after the widget was shown.
    static void adaptIcon(QAbstractButton *b);
    static void adaptIcon(QAction *a);

    // Fixed width for an input widget sized to fit `sample` in the current
    // theme font (padding/spin buttons included). Re-applied on theme change.
    static void fitWidth(QWidget *w, const QString &sample);
    static int fontPointSize() { return tablet ? 11 : 9; }

    // Monospace face for numeric readouts (frequency, level, points...): digits keep
    // their width while values change, like the display of a bench instrument.
    static QFont readoutFont();
    // true for widgets that show numeric readouts (SIUnitEdit, spin boxes)
    static bool isReadout(const QWidget *w);

    // Colors of the plot area (background, axes, grid, alternating tick bands)
    struct GraphColors {
        QColor background, axis, divisions, ticksBackground;
    };
    static GraphColors graphColors(Kind k);
    // Writes graphColors(current) into Preferences::Graphs.Color and repaints the plots
    static void applyGraphColors();

    // Curated trace palette; index wraps around. In the light theme the same hues are darker.
    static int tracePaletteSize();
    static QColor traceColor(int index, Kind k);
    static QColor traceColor(int index) { return traceColor(index, kind); }
    // If `c` is an entry of the `from` palette, returns the same entry of the `to` palette
    // (used to re-tint the default traces when the theme is switched); otherwise `c` unchanged.
    static QColor remapTraceColor(const QColor &c, Kind from, Kind to);
    // `c` adjusted (darkened on a light background, lightened on a dark one) until
    // text in this color has at least 3:1 contrast against `bg`. Used for trace
    // names / marker data drawn in the trace color.
    static QColor readableOn(const QColor &c, const QColor &bg);
    // Graph typography / stroke widths: the Preferences values scaled for the
    // current mode (tablet: larger text and thicker traces for a 150 % touch screen).
    struct GraphMetrics {
        int fontSizeTitle, fontSizeAxis, fontSizeCursorOverlay, fontSizeMarkerData, fontSizeTraceNames;
        double lineWidth;
        int triangleSize;
    };
    static GraphMetrics graphMetrics();
    static double graphFontScale() { return tablet ? 1.4 : 1.0; }
    static double graphLineScale() { return tablet ? 1.5 : 1.0; }
    // Readout font at a pixel size, for axis ticks / marker data on the graphs
    static QFont graphFont(int pixelSize);
    // Foreground that contrasts with the plot background (marker outlines, marker lines)
    static QColor graphTextColor(Kind k);
    static QColor graphTextColor() { return graphTextColor(kind); }
    // Limit / pass-fail lines
    static QColor limitColor(Kind k);
    static QColor limitColor() { return limitColor(kind); }

private:
    static QIcon adapted(const QIcon &orig);
    static void refreshAllIcons();
    static void refreshAllWidths();
    static QPalette palette(Kind kind);
    static QString styleSheet(Kind kind, bool tablet);

    static Kind kind;
    static bool tablet;
};

#endif // THEME_H
