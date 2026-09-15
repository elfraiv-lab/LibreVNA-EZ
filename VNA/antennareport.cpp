#include "antennareport.h"

#include "Traces/tracemodel.h"
#include "Traces/trace.h"
#include "Traces/traceplot.h"
#include "Traces/tracexyplot.h"
#include "Traces/tracesmithchart.h"
#include "Traces/Marker/markermodel.h"
#include "Traces/Marker/marker.h"
#include "Util/util.h"
#include "unit.h"
#include "preferences.h"

#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QPageLayout>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QRegularExpression>
#include <map>
#include <limits>
#include <cmath>
#include <algorithm>

using namespace std;

namespace {

constexpr int DPI = 150;

// Temporarily switches the graphs to a print-friendly palette (white background,
// black axes) and darkens bright trace colors. Everything is restored on destruction.
class PrintMode {
public:
    PrintMode(TraceModel &model)
    {
        auto &c = Preferences::getInstance().Graphs.Color;
        background = c.background;
        axis = c.axis;
        divisions = c.Ticks.divisions;
        ticksBackgroundEnabled = c.Ticks.Background.enabled;
        ticksBackground = c.Ticks.Background.background;

        c.background = Qt::white;
        c.axis = Qt::black;
        c.Ticks.divisions = QColor(190, 190, 190);
        c.Ticks.Background.enabled = false;
        c.Ticks.Background.background = Qt::white;

        for(auto t : model.getTraces()) {
            auto col = t->color();
            traceColors[t] = col;
            if(col.lightness() > 80) {
                t->setColor(col.darker(190));
            }
        }
    }
    ~PrintMode()
    {
        auto &c = Preferences::getInstance().Graphs.Color;
        c.background = background;
        c.axis = axis;
        c.Ticks.divisions = divisions;
        c.Ticks.Background.enabled = ticksBackgroundEnabled;
        c.Ticks.Background.background = ticksBackground;
        for(auto &tc : traceColors) {
            tc.first->setColor(tc.second);
        }
    }
private:
    QColor background, axis, divisions, ticksBackground;
    bool ticksBackgroundEnabled;
    map<Trace*, QColor> traceColors;
};

// Simple top-down page layout on a QPdfWriter with automatic page breaks.
class Page {
public:
    Page(QPdfWriter &writer, const QString &footerText)
        : writer(writer), p(&writer), footerText(footerText), pageNo(1)
    {
        pageW = writer.width();
        pageH = writer.height();
        margin = DPI / 2;
        base = QFont("Sans Serif", 10);
        p.setFont(base);
        footerH = metrics(small()).height() + DPI / 8;
        y = margin;
        drawFooter();
    }

    int contentWidth() const { return pageW - 2 * margin; }
    int left() const { return margin; }

    // metrics must be taken for the PDF device (150 dpi), screen metrics are too small
    QFontMetrics metrics(const QFont &f) const { return QFontMetrics(f, &writer); }

    QFont small() const { QFont f = base; f.setPointSize(8); return f; }
    QFont bold() const { QFont f = base; f.setBold(true); return f; }
    QFont heading() const { QFont f = base; f.setPointSize(12); f.setBold(true); return f; }
    QFont title() const { QFont f = base; f.setPointSize(16); f.setBold(true); return f; }

    void ensure(int h)
    {
        if(y + h > pageH - margin - footerH) {
            writer.newPage();
            pageNo++;
            y = margin;
            drawFooter();
        }
    }

    void space(int h) { y += h; }

    // starts a new page in the given orientation (subsequent pages keep it)
    void newPage(QPageLayout::Orientation orientation)
    {
        writer.setPageOrientation(orientation);
        writer.newPage();
        pageW = writer.width();
        pageH = writer.height();
        pageNo++;
        y = margin;
        drawFooter();
    }

    int remainingHeight() const { return pageH - margin - footerH - y; }
    int currentPage() const { return pageNo; }

    void text(const QString &s, const QFont &font, const QColor &color = Qt::black)
    {
        p.setFont(font);
        p.setPen(color);
        QFontMetrics fm = metrics(font);
        auto needed = fm.boundingRect(QRect(0, 0, contentWidth(), 100000), Qt::TextWordWrap, s).height();
        ensure(needed);
        QRect used;
        p.drawText(QRect(margin, y, contentWidth(), needed + fm.height()), Qt::TextWordWrap, s, &used);
        y += used.height() + fm.height() / 3;
    }

    void headingText(const QString &s)
    {
        ensure(DPI / 2);
        space(DPI / 10);
        text(s, heading());
        p.setPen(QPen(QColor(120, 120, 120), 1));
        p.drawLine(margin, y + 2, margin + contentWidth(), y + 2);
        space(DPI / 12);
    }

    void keyValue(const QString &key, const QString &value)
    {
        p.setFont(base);
        QFontMetrics fm = metrics(base);
        int keyW = DPI * 6 / 5;
        auto needed = fm.boundingRect(QRect(0, 0, contentWidth() - keyW, 100000), Qt::TextWordWrap, value).height();
        needed = max(needed, fm.height());
        ensure(needed);
        p.setPen(QColor(90, 90, 90));
        p.drawText(QRect(margin, y, keyW, needed + fm.height()), Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, key);
        p.setPen(Qt::black);
        QRect used;
        p.drawText(QRect(margin + keyW, y, contentWidth() - keyW, needed + fm.height()), Qt::TextWordWrap, value, &used);
        y += max(used.height(), fm.height()) + fm.height() / 2;
    }

    // Columns are given as relative weights; long cells wrap onto further lines.
    void table(const QStringList &headers, const vector<int> &weights, const vector<QStringList> &rows)
    {
        QFont f = base;
        f.setPointSize(9);
        QFontMetrics fm = metrics(f);
        const int cellPad = 4;
        const int minRowH = fm.height() * 8 / 5;
        int total = 0;
        for(auto w : weights) {
            total += w;
        }
        vector<int> widths;
        for(auto w : weights) {
            widths.push_back(contentWidth() * w / total);
        }
        auto drawRow = [&](const QStringList &cells, bool header) {
            QFont rf = f;
            rf.setBold(header);
            QFontMetrics rfm = metrics(rf);
            const int flags = Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap;
            // row height from the tallest (wrapped) cell
            int rowH = minRowH;
            for(int i = 0; i < cells.size() && i < (int) widths.size(); i++) {
                auto needed = rfm.boundingRect(QRect(0, 0, widths[i] - 2 * cellPad, 100000), flags, cells[i]).height();
                rowH = max(rowH, needed + fm.height() * 3 / 5);
            }
            ensure(rowH);
            if(header) {
                p.fillRect(QRect(margin, y, contentWidth(), rowH), QColor(225, 225, 225));
            }
            p.setFont(rf);
            p.setPen(Qt::black);
            int x = margin;
            for(int i = 0; i < cells.size() && i < (int) widths.size(); i++) {
                p.drawText(QRect(x + cellPad, y, widths[i] - 2 * cellPad, rowH), flags, cells[i]);
                x += widths[i];
            }
            p.setPen(QPen(QColor(200, 200, 200), 1));
            p.drawLine(margin, y + rowH, margin + contentWidth(), y + rowH);
            y += rowH;
        };
        drawRow(headers, true);
        for(auto &r : rows) {
            drawRow(r, false);
        }
        space(DPI / 12);
    }

    void widget(QWidget *w, double maxHeightFraction)
    {
        widgetFit(w, pageH * maxHeightFraction);
    }

    // renders the widget scaled to the content width, limited to maxH pixels of height
    void widgetFit(QWidget *w, double maxH)
    {
        if(!w || w->width() <= 0 || w->height() <= 0) {
            return;
        }
        double scale = (double) contentWidth() / w->width();
        if(w->height() * scale > maxH) {
            scale = maxH / w->height();
        }
        int tw = w->width() * scale;
        int th = w->height() * scale;
        ensure(th + DPI / 8);
        int x = margin + (contentWidth() - tw) / 2;
        p.save();
        p.translate(x, y);
        p.scale(scale, scale);
        w->render(&p, QPoint(), QRegion(), QWidget::RenderFlags());
        p.restore();
        p.setPen(QPen(QColor(160, 160, 160), 1));
        p.drawRect(QRect(x, y, tw, th));
        y += th + DPI / 8;
    }

    // Renders the widget filling targetW x targetH: the widget is temporarily resized to
    // the target aspect ratio (keeping its width, so fonts stay readable) and scaled uniformly.
    void widgetStretch(QWidget *w, int targetW, int targetH)
    {
        if(!w || w->width() <= 0 || w->height() <= 0 || targetW <= 0 || targetH <= 0) {
            return;
        }
        QSize original = w->size();
        int renderW = original.width();
        int renderH = max(1, (int) llround((double) renderW * targetH / targetW));
        w->resize(renderW, renderH);
        double scale = (double) targetW / renderW;
        int th = renderH * scale;
        ensure(th + DPI / 8);
        p.save();
        p.translate(margin, y);
        p.scale(scale, scale);
        w->render(&p, QPoint(), QRegion(), QWidget::RenderFlags());
        p.restore();
        w->resize(original);
        p.setPen(QPen(QColor(160, 160, 160), 1));
        p.drawRect(QRect(margin, y, targetW, th));
        y += th + DPI / 8;
    }

    QPainter &painter() { return p; }

private:
    void drawFooter()
    {
        p.save();
        p.setFont(small());
        p.setPen(QColor(120, 120, 120));
        auto rect = QRect(margin, pageH - margin - footerH + DPI / 16, contentWidth(), footerH);
        p.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, footerText);
        p.drawText(rect, Qt::AlignRight | Qt::AlignVCenter, QString("Page %1").arg(pageNo));
        p.restore();
    }

    QPdfWriter &writer;
    QPainter p;
    QString footerText;
    int pageNo;
    int pageW, pageH, margin, footerH;
    int y;
    QFont base;
};

QString freq(double f)
{
    return Unit::ToString(f, "Hz", " kMG", 6, 3);
}

QString vswrString(complex<double> s)
{
    if(abs(s) >= 1.0) {
        return "∞";
    }
    return QString::number(Util::SparamToVSWR(s), 'f', 2);
}

QString dBString(complex<double> s)
{
    return QString::number(Util::SparamTodB(s), 'f', 1) + " dB";
}

QString impedanceString(complex<double> s, double z0)
{
    auto z = Util::SparamToImpedance(s, z0);
    QString sign = z.imag() < 0 ? " - j" : " + j";
    return QString::number(z.real(), 'f', 1) + sign + QString::number(fabs(z.imag()), 'f', 1) + " Ω";
}

QString axesName(TraceXYPlot *xy)
{
    QString axes = YAxis::TypeToName(xy->getYAxisType(0));
    if(xy->getYAxisType(1) != YAxis::Type::Disabled) {
        axes += " / " + YAxis::TypeToName(xy->getYAxisType(1));
    }
    return axes;
}

// VSWR and magnitude graphs get their own landscape page in the report
bool isMainGraph(TracePlot *p)
{
    auto xy = dynamic_cast<TraceXYPlot*>(p);
    if(!xy) {
        return false;
    }
    for(int i = 0; i < 2; i++) {
        auto type = xy->getYAxisType(i);
        if(type == YAxis::Type::VSWR || type == YAxis::Type::Magnitude) {
            return true;
        }
    }
    return false;
}

bool isFrequencyTrace(Trace *t)
{
    return t && t->size() > 0 && t->outputType() == Trace::DataType::Frequency;
}

QString sanitize(QString s)
{
    s.replace(QRegularExpression("[^A-Za-z0-9_\\-]+"), "_");
    return s;
}

bool writeTouchstone(const QString &path, Trace *t, const AntennaReport::Meta &meta, QString &error)
{
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = "Unable to write " + path;
        return false;
    }
    QTextStream out(&file);
    out << "! LibreVNA antenna report, trace " << t->name() << "\n";
    if(!meta.antennaName.isEmpty()) {
        out << "! Antenna: " << meta.antennaName << "\n";
    }
    out << "! Date: " << meta.time.toString(Qt::ISODate) << "\n";
    out << "! Device: " << meta.device << "\n";
    out << "! Calibration: " << meta.calibration << "\n";
    out << "# Hz S RI R " << QString::number(t->getReferenceImpedance(), 'f', 1) << "\n";
    for(unsigned int i = 0; i < t->size(); i++) {
        auto s = t->sample(i);
        out << QString::number(s.x, 'f', 3) << " "
            << QString::number(s.y.real(), 'e', 9) << " " << QString::number(s.y.imag(), 'e', 9) << "\n";
    }
    return true;
}

} // namespace

bool AntennaReport::write(const QString &pdfPath, const Meta &meta, const Options &opt,
                          TraceModel &traces, MarkerModel &markers,
                          QStringList &written, QString &error)
{
    {
        // QPdfWriter fails silently on an unwritable path, check up front
        QFile probe(pdfPath);
        if(!probe.open(QIODevice::WriteOnly)) {
            error = "Unable to write " + pdfPath;
            return false;
        }
        probe.close();
    }
    QPdfWriter writer(pdfPath);
    writer.setResolution(DPI);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setTitle("Antenna report" + (meta.antennaName.isEmpty() ? QString() : " - " + meta.antennaName));
    writer.setCreator("LibreVNA-GUI");

    QString appVersion = QCoreApplication::applicationVersion();
    QString footer = "LibreVNA-GUI" + (appVersion.isEmpty() ? QString() : " " + appVersion)
            + " · " + meta.time.toString("yyyy-MM-dd HH:mm");

    // reflection traces in the frequency domain are the subject of the report
    vector<Trace*> reflection;
    for(auto t : traces.getTraces()) {
        if(isFrequencyTrace(t) && t->isReflection()) {
            reflection.push_back(t);
        }
    }

    {
        Page page(writer, footer);

        // ---- header ----
        page.text("Antenna measurement report", page.title());
        if(!meta.antennaName.isEmpty()) {
            page.text(meta.antennaName, page.heading());
        }
        page.space(DPI / 10);
        page.keyValue("Date", meta.time.toString("yyyy-MM-dd HH:mm"));
        page.keyValue("Device", meta.device);
        page.keyValue("Calibration", meta.calibration);
        page.keyValue("Sweep", meta.sweep);
        if(!meta.cable.isEmpty()) {
            page.keyValue("Cable", meta.cable);
        }
        if(!meta.notes.trimmed().isEmpty()) {
            page.keyValue("Notes", meta.notes.trimmed());
        }

        // ---- per-trace summary ----
        page.headingText("Reflection summary");
        if(reflection.empty()) {
            page.text("No reflection trace (S11/S22) with data.", page.small(), QColor(150, 0, 0));
        } else {
            vector<QStringList> rows;
            for(auto t : reflection) {
                unsigned int minIdx = 0, maxIdx = 0;
                double minAbs = numeric_limits<double>::max();
                double maxAbs = -1.0;
                for(unsigned int i = 0; i < t->size(); i++) {
                    auto a = abs(t->sample(i).y);
                    if(isnan(a)) {
                        continue;
                    }
                    if(a < minAbs) {
                        minAbs = a;
                        minIdx = i;
                    }
                    if(a > maxAbs) {
                        maxAbs = a;
                        maxIdx = i;
                    }
                }
                if(maxAbs < 0) {
                    continue;
                }
                auto best = t->sample(minIdx);
                auto worst = t->sample(maxIdx);
                rows.push_back({t->name(),
                                freq(best.x), vswrString(best.y), dBString(best.y), impedanceString(best.y, t->getReferenceImpedance()),
                                freq(worst.x), vswrString(worst.y)});
            }
            page.table({"Trace", "Resonance", "VSWR", "|S|", "Z at resonance", "Worst at", "Worst VSWR"},
                       {10, 15, 8, 10, 22, 17, 18}, rows);
        }

        // ---- antenna band markers ----
        vector<QStringList> bandRows;
        vector<QStringList> cableRows;
        vector<QStringList> markerRows;
        vector<QStringList> statRows;
        vector<QStringList> lossRows;
        if(opt.includeMarkers) {
            for(auto m : markers.getMarkers()) {
                if(m->getParent() || !m->getTrace() || !m->isVisible()) {
                    continue;
                }
                auto t = m->getTrace();
                if(m->getType() == Marker::Type::CableImpedance) {
                    cableRows.push_back({t->name(), m->readableData(Marker::Format::CableImpedance),
                                         m->readableData(Marker::Format::CableLength) + ", "
                                         + m->readableData(Marker::Format::CableDelay)});
                    continue;
                }
                if(m->getType() == Marker::Type::CableLoss) {
                    auto &st = m->getBandStats();
                    if(!st.valid) {
                        continue;
                    }
                    QString band = Unit::ToString(st.fLow, "Hz", " kMG", 4) + " \u2013 " + Unit::ToString(st.fHigh, "Hz", " kMG", 4);
                    QString worst = QString::number(st.lossMax, 'f', 2) + " dB @ " + m->readablePosition();
                    if(st.lossSuspicious) {
                        worst += " (?)";
                    }
                    QString range = QString::number(st.lossMin, 'f', 2) + " / " + QString::number(st.lossMax, 'f', 2)
                            + " / " + QString::number(st.lossMean, 'f', 2) + " dB";
                    QString perLength = m->readableData(Marker::Format::CableLossPerLength);
                    perLength.remove(QRegularExpression("^Loss(/length)?: "));
                    lossRows.push_back({t->name(), band, worst, range, perLength,
                                        QString::number(st.ripplePP, 'f', 2) + " dB"});
                    continue;
                }
                if(m->getType() == Marker::Type::Statistics) {
                    auto &st = m->getBandStats();
                    if(!st.valid) {
                        continue;
                    }
                    QString band = Unit::ToString(st.fLow, "Hz", " kMG", 4) + " – " + Unit::ToString(st.fHigh, "Hz", " kMG", 4);
                    QString range;
                    if(t->isReflection()) {
                        range = "VSWR " + QString::number(st.vswrMin, 'f', 2) + " / " + QString::number(st.vswrMax, 'f', 2)
                                + " / " + QString::number(st.vswrMean, 'f', 2);
                    } else {
                        range = QString::number(st.magMin, 'f', 2) + " / " + QString::number(st.magMax, 'f', 2)
                                + " / " + QString::number(st.magMean, 'f', 2) + " dB";
                    }
                    QString worst = m->readablePosition() + ", " + (t->isReflection() ? vswrString(m->getData()) : dBString(m->getData()));
                    QString ripple = QString::number(st.ripplePP, 'f', 2) + " dB";
                    if(m->hasRippleLimit()) {
                        ripple += " (≤ " + QString::number(m->getRippleLimit(), 'f', 2) + ") " + (m->isRipplePass() ? "PASS" : "FAIL");
                    }
                    statRows.push_back({t->name(), band, range, worst, ripple});
                    continue;
                }
                if(m->getType() == Marker::Type::AntennaBand) {
                    auto d = m->getData();
                    bandRows.push_back({t->name(),
                                        QString::number(m->getVSWRThreshold(), 'g', 3),
                                        m->readablePosition(), vswrString(d), dBString(d),
                                        impedanceString(d, t->getReferenceImpedance()),
                                        m->readableData(Marker::Format::BandEdges),
                                        m->readableData(Marker::Format::Bandwidth)});
                } else {
                    QString vswr, z;
                    if(t->isReflection() && m->getType() != Marker::Type::Delta) {
                        vswr = vswrString(m->getData());
                        z = impedanceString(m->getData(), t->getReferenceImpedance());
                    }
                    markerRows.push_back({QString::number(m->getNumber()), t->name(), m->readableType(),
                                          m->readablePosition(), m->readableData(Marker::Format::dB), vswr, z});
                }
            }
        }
        if(!bandRows.empty()) {
            page.headingText("Antenna band (VSWR threshold)");
            page.table({"Trace", "Limit", "Resonance", "VSWR", "|S|", "Z", "Band edges", "Bandwidth"},
                       {7, 6, 12, 6, 8, 14, 25, 22}, bandRows);
        }

        if(!statRows.empty()) {
            page.headingText("Band statistics");
            page.table({"Trace", "Band", "min / max / mean", "Worst point", "Ripple p-p"},
                       {8, 24, 26, 22, 20}, statRows);
        }

        if(!lossRows.empty()) {
            page.headingText("Cable loss (one way, far end open or shorted)");
            page.table({"Trace", "Band", "Worst case", "min / max / mean", "Per length", "Ripple"},
                       {7, 19, 20, 21, 20, 13}, lossRows);
        }

        if(!cableRows.empty()) {
            page.headingText("Cable impedance (TDR)");
            page.table({"Trace", "Impedance", "Length / velocity factor"}, {14, 36, 50}, cableRows);
        }

        // ---- limit lines ----
        vector<TracePlot*> plots;
        for(auto p : TracePlot::getPlots()) {
            if(&p->getModel() == &traces) {
                plots.push_back(p);
            }
        }
        // keep the on-screen order (top to bottom, left to right)
        sort(plots.begin(), plots.end(), [](TracePlot *a, TracePlot *b) {
            auto pa = a->mapToGlobal(QPoint(0, 0));
            auto pb = b->mapToGlobal(QPoint(0, 0));
            if(abs(pa.y() - pb.y()) > 10) {
                return pa.y() < pb.y();
            }
            return pa.x() < pb.x();
        });
        // one row per limit line; VSWR and magnitude limits are re-evaluated against the reflection
        // traces so that the report can name the worst point, other lines use the graph's verdict
        vector<QStringList> limitRows;
        bool allPass = true;
        const bool nanPasses = Preferences::getInstance().Graphs.limitNaNpasses;
        for(auto p : plots) {
            auto xy = dynamic_cast<TraceXYPlot*>(p);
            if(!xy || !xy->hasLimitLines()) {
                continue;
            }
            QString axes = axesName(xy);
            for(auto line : xy->getConstantLines()) {
                auto &pts = line->getPoints();
                int axisIdx = line->getAxis() == XYPlotConstantLine::Axis::Primary ? 0 : 1;
                auto type = xy->getYAxisType(axisIdx);
                auto pf = line->getPassFail();
                QString range;
                if(pts.size() >= 2) {
                    range = freq(pts.front().x()) + " – " + freq(pts.back().x());
                }
                QString unit = type == YAxis::Type::VSWR ? "" : " dB";
                QString limitText = line->getName();
                if(pts.size() >= 2 && pf != XYPlotConstantLine::PassFail::DontCare) {
                    limitText = QString(pf == XYPlotConstantLine::PassFail::HighLimit ? "≤ " : "≥ ")
                            + QString::number(pts.front().y(), 'f', 2) + unit;
                    if(pts.back().y() != pts.front().y()) {
                        limitText += " … " + QString::number(pts.back().y(), 'f', 2) + unit;
                    }
                }
                bool evaluable = pts.size() >= 2 && pf != XYPlotConstantLine::PassFail::DontCare
                        && (type == YAxis::Type::VSWR || type == YAxis::Type::Magnitude) && !reflection.empty();
                QString result;
                bool pass;
                if(evaluable) {
                    // limit value at x (polyline), NaN outside of the line's range
                    auto limitAt = [&](double x) -> double {
                        if(x < pts.front().x() || x > pts.back().x()) {
                            return numeric_limits<double>::quiet_NaN();
                        }
                        for(size_t i = 1; i < pts.size(); i++) {
                            if(x <= pts[i].x()) {
                                double alpha = (x - pts[i-1].x()) / (pts[i].x() - pts[i-1].x());
                                return pts[i-1].y() * (1 - alpha) + pts[i].y() * alpha;
                            }
                        }
                        return pts.back().y();
                    };
                    double worstMargin = numeric_limits<double>::lowest();
                    double worstValue = 0, worstFreq = 0;
                    bool anyNaN = false;
                    for(auto t : reflection) {
                        for(unsigned int i = 0; i < t->size(); i++) {
                            auto smp = t->sample(i);
                            double lim = limitAt(smp.x);
                            if(isnan(lim)) {
                                continue;
                            }
                            double value;
                            if(type == YAxis::Type::VSWR) {
                                value = abs(smp.y) < 1.0 ? Util::SparamToVSWR(smp.y) : numeric_limits<double>::quiet_NaN();
                            } else {
                                value = Util::SparamTodB(smp.y);
                            }
                            if(isnan(value) || isinf(value)) {
                                anyNaN = true;
                                continue;
                            }
                            double margin = pf == XYPlotConstantLine::PassFail::HighLimit ? value - lim : lim - value;
                            if(margin > worstMargin) {
                                worstMargin = margin;
                                worstValue = value;
                                worstFreq = smp.x;
                            }
                        }
                    }
                    pass = worstMargin <= 0 && (!anyNaN || nanPasses);
                    result = pass ? "PASS" : "FAIL";
                    if(worstMargin > numeric_limits<double>::lowest()) {
                        result += QString(", %1 %2%3 @ %4").arg(pf == XYPlotConstantLine::PassFail::HighLimit ? "max" : "min")
                                .arg(QString::number(worstValue, 'f', 2)).arg(unit).arg(freq(worstFreq));
                    }
                    if(anyNaN && !nanPasses) {
                        result += ", undefined points";
                    }
                } else {
                    pass = xy->getLimitPassing();
                    result = pass ? "PASS" : "FAIL";
                }
                allPass &= pass;
                limitRows.push_back({axes, range, limitText, result});
            }
        }
        if(!limitRows.empty()) {
            page.headingText(QString("Limit lines: ") + (allPass ? "PASS" : "FAIL"));
            page.table({"Graph", "Band", "Limit", "Result"}, {22, 26, 16, 36}, limitRows);
        }

        if(!markerRows.empty()) {
            page.headingText("Markers");
            page.table({"#", "Trace", "Type", "Frequency", "|S|", "VSWR", "Z"},
                       {5, 12, 16, 18, 13, 10, 26}, markerRows);
        }

        // ---- graphs ----
        if(opt.includePlots && !plots.empty()) {
            PrintMode print(traces);
            // Smith chart: only in the free space of the first page, right after the tables
            TracePlot *smith = nullptr;
            for(auto p : plots) {
                if(dynamic_cast<TraceSmithChart*>(p)) {
                    smith = p;
                    break;
                }
            }
            if(smith && page.currentPage() == 1 && page.remainingHeight() > DPI * 3 / 2) {
                page.space(DPI / 10);
                page.widgetFit(smith, page.remainingHeight() - DPI / 8);
            }
            // VSWR / magnitude: one landscape page each, as large as the page allows
            vector<TracePlot*> others;
            for(auto p : plots) {
                if(dynamic_cast<TraceSmithChart*>(p)) {
                    continue;
                }
                if(!isMainGraph(p)) {
                    others.push_back(p);
                    continue;
                }
                page.newPage(QPageLayout::Landscape);
                page.text(axesName(static_cast<TraceXYPlot*>(p)), page.heading());
                page.space(DPI / 12);
                page.widgetStretch(p, page.contentWidth(), page.remainingHeight() - DPI / 8);
            }
            // everything else (Smith chart, TDR, ...) shares portrait pages
            if(!others.empty()) {
                page.newPage(QPageLayout::Portrait);
                page.headingText("Other graphs");
                for(auto p : others) {
                    page.widget(p, 0.42);
                }
            }
        }
        // Page (and its QPainter) end here, before the writer is closed
    }
    written.push_back(pdfPath);

    // ---- touchstone next to the pdf ----
    if(opt.saveTouchstone) {
        QFileInfo fi(pdfPath);
        QString base = fi.path() + "/" + fi.completeBaseName();
        for(auto t : reflection) {
            QString path = base;
            if(reflection.size() > 1) {
                path += "_" + sanitize(t->name());
            }
            path += ".s1p";
            if(!writeTouchstone(path, t, meta, error)) {
                return false;
            }
            written.push_back(path);
        }
    }
    return true;
}
