#ifndef ANTENNAREPORT_H
#define ANTENNAREPORT_H

#include <QString>
#include <QStringList>
#include <QDateTime>

class TraceModel;
class MarkerModel;

// PDF report for an antenna (reflection) measurement: setup metadata, per-trace
// resonance/worst VSWR summary, antenna band markers, other markers, limit line
// PASS/FAIL, and the current graphs rendered on a white background.
class AntennaReport
{
public:
    class Meta {
    public:
        QString antennaName;
        QString notes;
        QString device;         // serial, hardware, firmware
        QString calibration;    // calibration type and file
        QString sweep;          // start/stop/points/IFBW/averaging/level
        QString cable;          // cable compensation between calibration plane and antenna (may be empty)
        QDateTime time;
    };
    class Options {
    public:
        bool includePlots = true;
        bool includeMarkers = true;
        bool saveTouchstone = true;
    };

    // Writes the PDF (and optional .s1p files next to it). Returns false and sets
    // error on failure; written lists every file that was created.
    static bool write(const QString &pdfPath, const Meta &meta, const Options &opt,
                      TraceModel &traces, MarkerModel &markers,
                      QStringList &written, QString &error);
};

#endif // ANTENNAREPORT_H
